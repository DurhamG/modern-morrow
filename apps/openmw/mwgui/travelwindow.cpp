#include "travelwindow.hpp"

#include <MyGUI_Button.h>
#include <MyGUI_Gui.h>
#include <MyGUI_ScrollView.h>

#include <components/debug/debuglog.hpp>
#include <components/esm/util.hpp>
#include <components/misc/strings/conversion.hpp>
#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/cellstore.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/containerstore.hpp"
#include "../mwworld/worldmodel.hpp"

#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/services.hpp"

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
        }
    }

    void TravelWindow::addDestination(const ESM::RefId& name, const ESM::Transport::Dest& destination)
    {
        MWWorld::Ptr player = MWBase::Environment::get().getWorld()->getPlayerPtr();
        int playerGold = player.getClass().getContainerStore(player).count(MWWorld::ContainerStore::sGoldId);

        const int price = MWMechanics::getTravelPrice(mPtr, destination);

        const int lineHeight = Settings::gui().mFontSize + 2;

        MyGUI::Button* toAdd = mDestinationsView->createWidget<MyGUI::Button>(
            "SandTextButton", 0, mCurrentY, 200, lineHeight, MyGUI::Align::Default);
        toAdd->setEnabled(price <= playerGold);
        mCurrentY += lineHeight;

        const std::string& nameString = name.getRefIdString();
        toAdd->setUserString("price", std::to_string(price));
        toAdd->setCaptionWithReplacing("#{sCell=" + nameString + "}  - " + MyGUI::utility::toString(price) + "#{sgp}");
        toAdd->setSize(mDestinationsView->getWidth(), lineHeight);
        toAdd->eventMouseWheel += MyGUI::newDelegate(this, &TravelWindow::onMouseWheel);
        toAdd->setUserString("Destination", nameString);
        toAdd->setUserData(destination);
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

        for (const ESM::Transport::Dest& dest : MWMechanics::getTravelDestinations(mPtr))
        {
            std::string_view cellname = dest.mCellName;
            const ESM::ExteriorCellLocation cellIndex
                = ESM::positionToExteriorCellLocation(dest.mPos.pos[0], dest.mPos.pos[1]);
            const MWWorld::WorldModel& worldModel = *MWBase::Environment::get().getWorldModel();
            if (cellname.empty())
            {
                MWWorld::CellStore& cell = worldModel.getExterior(cellIndex);
                cellname = MWBase::Environment::get().getWorld()->getCellName(&cell);
            }
            else if (worldModel.findCell(cellname, false) == nullptr)
            {
                Log(Debug::Error) << "Failed to add travel destination: unknown cell (" << cellname << ")";
                continue;
            }
            addDestination(ESM::RefId::stringRefId(cellname), dest);
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
    }

    void TravelWindow::onTravelButtonClick(MyGUI::Widget* sender)
    {
        const int price = Misc::StringUtils::toNumeric<int>(sender->getUserString("price"), 0);

        MWWorld::Ptr player = MWMechanics::getPlayer();
        int playerGold = player.getClass().getContainerStore(player).count(MWWorld::ContainerStore::sGoldId);

        if (playerGold < price)
            return;

        if (!mPtr.getCell()->isExterior())
            // Interior cell -> mages guild transport
            MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("mysticism cast"));

        MWBase::Environment::get().getWindowManager()->fadeScreenOut(1);

        const MWWorld::Ptr merchant = mPtr;
        const ESM::Transport::Dest destination = *sender->getUserData<ESM::Transport::Dest>();

        MWBase::Environment::get().getWindowManager()->removeGuiMode(GM_Travel);
        MWBase::Environment::get().getWindowManager()->exitCurrentGuiMode();

        MWBase::Environment::get().getWindowManager()->fadeScreenOut(1);
        // Charge the price that the button showed. Leaving the dialogue above takes back the disposition that the
        // player gained by persuasion, so the price would be higher now.
        MWMechanics::travel(merchant, destination, price);

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
