#include "travelwindow.hpp"

#include <MyGUI_Button.h>
#include <MyGUI_Gui.h>
#include <MyGUI_ScrollView.h>

#include <components/debug/debuglog.hpp>
#include <components/esm3/loadcrea.hpp>
#include <components/esm3/loadgmst.hpp>
#include <components/misc/strings/conversion.hpp>
#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/actionteleport.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/containerstore.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/store.hpp"
#include "../mwworld/worldmodel.hpp"

#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/creaturestats.hpp"

namespace MWGui
{
    TravelWindow::TravelWindow()
        : WindowBase("openmw_travel_window.layout")
        , mCurrentY(0)
    {
        getWidget(mCancelButton, "CancelButton");
        getWidget(mPlayerGold, "PlayerGold");
        getWidget(mDestinationsView, "DestinationsView");

        mCancelButton->eventMouseButtonClick += MyGUI::newDelegate(this, &TravelWindow::onCancelButtonClicked);

        if (Settings::gui().mControllerMenus)
        {
            mDisableGamepadCursor = true;
            mControllerButtons.mA = "#{Interface:Travel}";
            mControllerButtons.mB = "#{Interface:Cancel}";

            mPanel = std::make_unique<ServicePanel>(ServicePanel::Service{ "#{Interface:ServiceTravel}",
                "#{Interface:Travel}", "#{Interface:ServiceNoDestinations}", [this] { return mPtr; },
                [this] { return controllerEntries(); }, [this](MyGUI::Widget* button) { onTravelButtonClick(button); },
                [this] { onCancelButtonClicked(mCancelButton); } });
        }
    }

    TravelWindow::~TravelWindow() = default;

    void TravelWindow::onOpen()
    {
        if (mPanel == nullptr)
            return;
        ControllerUi::hideWindow(mMainWidget);
        mPanel->show();
    }

    void TravelWindow::onClose()
    {
        if (mPanel != nullptr)
            mPanel->hide();
        // Once the window is closed, rather than hidden under another window, it forgets the guide, as the
        // container and barter windows do. The guide may be gone by the time the window opens again, and the
        // window opens before it is given its next one.
        if (!MWBase::Environment::get().getWindowManager()->containsMode(GM_Travel))
            resetReference();
    }

    void TravelWindow::onFrame(float dt)
    {
        if (mPanel != nullptr)
            mPanel->onFrame(dt);
    }

    std::vector<ServicePanel::Entry> TravelWindow::controllerEntries()
    {
        const MWWorld::Ptr player = MWMechanics::getPlayer();
        const int playerGold = player.getClass().getContainerStore(player).count(MWWorld::ContainerStore::sGoldId);
        const float travelTimeMult = MWBase::Environment::get()
                                         .getESMStore()
                                         ->get<ESM::GameSetting>()
                                         .find("fTravelTimeMult")
                                         ->mValue.getFloat();
        std::vector<ServicePanel::Entry> entries;
        for (std::size_t i = 0; i < mDestinationsView->getChildCount(); ++i)
        {
            MyGUI::Widget* button = mDestinationsView->getChildAt(i);
            const ESM::Position* pos = button->getUserData<ESM::Position>(false);
            if (pos == nullptr)
                continue;
            ServicePanel::Entry entry;
            entry.mButton = button;
            entry.mName = ControllerUi::localised("#{sCell=" + std::string(button->getUserString("Destination")) + "}");
            entry.mPrice = Misc::StringUtils::toNumeric<int>(button->getUserString("price"), 0);
            if (entry.mPrice > playerGold)
                entry.mUnavailable = "#{Interface:BarterNotEnoughGold}";
            // Travel from outdoors takes time, as the window works it out. Guild guides teleport.
            std::string time = "#{Interface:ServiceInstant}";
            if (mPtr.getCell()->isExterior())
            {
                const ESM::Position playerPos = player.getRefData().getPosition();
                const float distance
                    = (osg::Vec2f(pos->pos[0], pos->pos[1]) - osg::Vec2f(playerPos.pos[0], playerPos.pos[1])).length();
                const int hours = static_cast<int>(distance / travelTimeMult);
                time = std::to_string(hours) + " "
                    + (hours == 1 ? "#{Interface:ServiceHour}" : "#{Interface:ServiceHours}");
            }
            entry.mSubtitle = time;
            entry.mFacts.emplace_back("#{Interface:ServiceTravelTime}", time);
            entries.push_back(std::move(entry));
        }
        return entries;
    }

    void TravelWindow::addDestination(const ESM::RefId& name, const ESM::Position& pos, bool interior)
    {
        int price;

        const MWWorld::Store<ESM::GameSetting>& gmst
            = MWBase::Environment::get().getESMStore()->get<ESM::GameSetting>();

        MWWorld::Ptr player = MWBase::Environment::get().getWorld()->getPlayerPtr();
        int playerGold = player.getClass().getContainerStore(player).count(MWWorld::ContainerStore::sGoldId);

        if (!mPtr.getCell()->isExterior())
        {
            price = gmst.find("fMagesGuildTravel")->mValue.getInteger();
        }
        else
        {
            const ESM::Position playerPos = player.getRefData().getPosition();
            double d = std::sqrt(std::pow(pos.pos[0] - playerPos.pos[0], 2) + std::pow(pos.pos[1] - playerPos.pos[1], 2)
                + std::pow(pos.pos[2] - playerPos.pos[2], 2));
            float fTravelMult = gmst.find("fTravelMult")->mValue.getFloat();
            if (fTravelMult != 0)
                price = static_cast<int>(d / fTravelMult);
            else
                price = static_cast<int>(d);
        }

        // Add price for the travelling followers
        std::set<MWWorld::Ptr> followers;
        MWWorld::ActionTeleport::getFollowers(player, followers, !interior);

        // Apply followers cost, unlike vanilla the first follower doesn't travel for free
        price *= 1 + static_cast<int>(followers.size());

        price = std::max(1, price);
        price = MWBase::Environment::get().getMechanicsManager()->getBarterOffer(mPtr, price, true);

        const int lineHeight = Settings::gui().mFontSize + 2;

        MyGUI::Button* toAdd = mDestinationsView->createWidget<MyGUI::Button>(
            "SandTextButton", 0, mCurrentY, 200, lineHeight, MyGUI::Align::Default);
        toAdd->setEnabled(price <= playerGold);
        mCurrentY += lineHeight;
        if (interior)
            toAdd->setUserString("interior", "y");
        else
            toAdd->setUserString("interior", "n");

        const std::string& nameString = name.getRefIdString();
        toAdd->setUserString("price", std::to_string(price));
        toAdd->setCaptionWithReplacing("#{sCell=" + nameString + "}  - " + MyGUI::utility::toString(price) + "#{sgp}");
        toAdd->setSize(mDestinationsView->getWidth(), lineHeight);
        toAdd->eventMouseWheel += MyGUI::newDelegate(this, &TravelWindow::onMouseWheel);
        toAdd->setUserString("Destination", nameString);
        toAdd->setUserData(pos);
        toAdd->eventMouseButtonClick += MyGUI::newDelegate(this, &TravelWindow::onTravelButtonClick);
        if (price <= playerGold)
            mDestinationButtons.emplace_back(toAdd);
    }

    void TravelWindow::clearDestinations()
    {
        mDestinationsView->setViewOffset(MyGUI::IntPoint(0, 0));
        mCurrentY = 0;
        while (mDestinationsView->getChildCount())
            MyGUI::Gui::getInstance().destroyWidget(mDestinationsView->getChildAt(0));
        mDestinationButtons.clear();
    }

    void TravelWindow::setPtr(const MWWorld::Ptr& actor)
    {
        if (actor.isEmpty() || !actor.getClass().isActor())
            throw std::runtime_error("Invalid argument in TravelWindow::setPtr");

        center();
        mPtr = actor;
        clearDestinations();

        std::vector<ESM::Transport::Dest> transport;
        if (mPtr.getClass().isNpc())
            transport = mPtr.get<ESM::NPC>()->mBase->getTransport();
        else if (mPtr.getType() == ESM::Creature::sRecordId)
            transport = mPtr.get<ESM::Creature>()->mBase->getTransport();

        for (const auto& dest : transport)
        {
            std::string_view cellname = dest.mCellName;
            bool interior = true;
            const ESM::ExteriorCellLocation cellIndex
                = ESM::positionToExteriorCellLocation(dest.mPos.pos[0], dest.mPos.pos[1]);
            const MWWorld::WorldModel& worldModel = *MWBase::Environment::get().getWorldModel();
            if (cellname.empty())
            {
                MWWorld::CellStore& cell = worldModel.getExterior(cellIndex);
                cellname = MWBase::Environment::get().getWorld()->getCellName(&cell);
                interior = false;
            }
            else
            {
                const MWWorld::CellStore* destCell = worldModel.findCell(cellname, false);
                if (destCell == nullptr)
                {
                    Log(Debug::Error) << "Failed to add travel destination: unknown cell (" << cellname << ")";
                    continue;
                }
                interior = !destCell->getCell()->isExterior();
            }
            addDestination(ESM::RefId::stringRefId(cellname), dest.mPos, interior);
        }

        updateLabels();

        if (Settings::gui().mControllerMenus)
        {
            mControllerFocus = 0;
            if (mDestinationButtons.size() > 0)
                mDestinationButtons[0]->setStateSelected(true);
        }

        // Canvas size must be expressed with VScroll disabled, otherwise MyGUI would expand the scroll area when the
        // scrollbar is hidden
        mDestinationsView->setVisibleVScroll(false);
        mDestinationsView->setCanvasSize(
            MyGUI::IntSize(mDestinationsView->getWidth(), std::max(mDestinationsView->getHeight(), mCurrentY)));
        mDestinationsView->setVisibleVScroll(true);

        if (mPanel != nullptr)
            mPanel->update();
    }

    void TravelWindow::onTravelButtonClick(MyGUI::Widget* sender)
    {
        const int price = Misc::StringUtils::toNumeric<int>(sender->getUserString("price"), 0);

        MWWorld::Ptr player = MWMechanics::getPlayer();
        int playerGold = player.getClass().getContainerStore(player).count(MWWorld::ContainerStore::sGoldId);

        if (playerGold < price)
            return;

        // Set "traveling" flag, so GetPCTraveling can detect teleportation.
        // We will reset this flag during next world update.
        MWBase::Environment::get().getWorld()->setPlayerTraveling(true);

        if (!mPtr.getCell()->isExterior())
            // Interior cell -> mages guild transport
            MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("mysticism cast"));

        player.getClass().getContainerStore(player).remove(MWWorld::ContainerStore::sGoldId, price);

        // add gold to NPC trading gold pool
        MWMechanics::CreatureStats& npcStats = mPtr.getClass().getCreatureStats(mPtr);
        npcStats.setGoldPool(npcStats.getGoldPool() + price);

        MWBase::Environment::get().getWindowManager()->fadeScreenOut(1);
        ESM::Position pos = *sender->getUserData<ESM::Position>();
        std::string_view cellname = sender->getUserString("Destination");
        bool interior = sender->getUserString("interior") == "y";
        if (mPtr.getCell()->isExterior())
        {
            ESM::Position playerPos = player.getRefData().getPosition();
            float d = (osg::Vec2f(pos.pos[0], pos.pos[1]) - osg::Vec2f(playerPos.pos[0], playerPos.pos[1])).length();
            const float fTravelTimeMult = MWBase::Environment::get()
                                              .getESMStore()
                                              ->get<ESM::GameSetting>()
                                              .find("fTravelTimeMult")
                                              ->mValue.getFloat();
            int hours = static_cast<int>(d / fTravelTimeMult);
            MWBase::Environment::get().getMechanicsManager()->rest(hours, true);
            MWBase::Environment::get().getWorld()->advanceTime(hours);
        }

        MWBase::Environment::get().getWindowManager()->removeGuiMode(GM_Travel);
        MWBase::Environment::get().getWindowManager()->exitCurrentGuiMode();

        MWBase::Environment::get().getWindowManager()->fadeScreenOut(1);
        const ESM::ExteriorCellLocation posCell = ESM::positionToExteriorCellLocation(pos.pos[0], pos.pos[1]);
        ESM::RefId cellId = ESM::Cell::generateIdForCell(!interior, cellname, posCell.mX, posCell.mY);

        // Teleports any followers, too.
        MWWorld::ActionTeleport action(cellId, pos, true);
        action.execute(player);

        MWBase::Environment::get().getWindowManager()->fadeScreenOut(0);
        MWBase::Environment::get().getWindowManager()->fadeScreenIn(1);
    }

    void TravelWindow::onCancelButtonClicked(MyGUI::Widget* /*sender*/)
    {
        MWBase::Environment::get().getWindowManager()->removeGuiMode(GM_Travel);
    }

    void TravelWindow::updateLabels()
    {
        MWWorld::Ptr player = MWBase::Environment::get().getWorld()->getPlayerPtr();
        int playerGold = player.getClass().getContainerStore(player).count(MWWorld::ContainerStore::sGoldId);

        mPlayerGold->setCaptionWithReplacing("#{sGold}: " + MyGUI::utility::toString(playerGold));
        mPlayerGold->setCoord(8, mPlayerGold->getTop(), mPlayerGold->getTextSize().width, mPlayerGold->getHeight());
    }

    void TravelWindow::onReferenceUnavailable()
    {
        MWBase::Environment::get().getWindowManager()->removeGuiMode(GM_Travel);
        MWBase::Environment::get().getWindowManager()->exitCurrentGuiMode();
    }

    void TravelWindow::onMouseWheel(MyGUI::Widget* /*sender*/, int rel)
    {
        if (mDestinationsView->getViewOffset().top + rel * 0.3f > 0)
            mDestinationsView->setViewOffset(MyGUI::IntPoint(0, 0));
        else
            mDestinationsView->setViewOffset(
                MyGUI::IntPoint(0, static_cast<int>(mDestinationsView->getViewOffset().top + rel * 0.3f)));
    }

    bool TravelWindow::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
    {
        if (mPanel != nullptr)
            return mPanel->onControllerButtonEvent(arg);

        if (arg.button == SDL_CONTROLLER_BUTTON_A)
        {
            if (mControllerFocus < mDestinationButtons.size())
            {
                onTravelButtonClick(mDestinationButtons[mControllerFocus]);
                MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
            }
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_B)
        {
            onCancelButtonClicked(mCancelButton);
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_DPAD_UP)
        {
            if (mDestinationButtons.size() <= 1)
                return true;

            setControllerFocus(mDestinationButtons, mControllerFocus, false);
            mControllerFocus = wrap(mControllerFocus, mDestinationButtons.size(), -1);
            setControllerFocus(mDestinationButtons, mControllerFocus, true);
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN)
        {
            if (mDestinationButtons.size() <= 1)
                return true;

            setControllerFocus(mDestinationButtons, mControllerFocus, false);
            mControllerFocus = wrap(mControllerFocus, mDestinationButtons.size(), 1);
            setControllerFocus(mDestinationButtons, mControllerFocus, true);
        }

        // Scroll the list to keep the active item in view
        if (mControllerFocus <= 5)
            mDestinationsView->setViewOffset(MyGUI::IntPoint(0, 0));
        else
        {
            const int lineHeight = Settings::gui().mFontSize + 2;
            mDestinationsView->setViewOffset(MyGUI::IntPoint(0, -lineHeight * static_cast<int>(mControllerFocus - 5)));
        }

        return true;
    }
}
