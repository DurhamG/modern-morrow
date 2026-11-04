#include "waitdialog.hpp"

#include <MyGUI_Gui.h>
#include <MyGUI_ILayer.h>
#include <MyGUI_InputManager.h>
#include <MyGUI_LayerManager.h>
#include <MyGUI_ProgressBar.h>
#include <MyGUI_ScrollBar.h>

#include <components/misc/rng.hpp>

#include <components/esm3/loadregn.hpp>
#include <components/settings/values.hpp>
#include <components/widgets/box.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/statemanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/cellstore.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/datetimemanager.hpp"
#include "../mwworld/esmstore.hpp"

#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/creaturestats.hpp"
#include "../mwmechanics/npcstats.hpp"

#include <format>

namespace MWGui
{
    namespace
    {
        using namespace ControllerUi;

        const MyGUI::Colour sPanelBackColour = MyGUI::Colour::parse("#0d0b08");

        // A time of day, such as "6 pm".
        std::string hourText(int hour)
        {
            const bool pm = hour >= 12;
            hour %= 12;
            if (hour == 0)
                hour = 12;
            return std::to_string(hour) + " " + (pm ? "#{Calendar:pm}" : "#{Calendar:am}");
        }
    }

    WaitDialogProgressBar::WaitDialogProgressBar()
        : WindowBase("openmw_wait_dialog_progressbar.layout")
    {
        getWidget(mProgressBar, "ProgressBar");
        getWidget(mProgressText, "ProgressText");
        mPanel = createPanelRoot("ControllerMenus");
    }

    WaitDialogProgressBar::~WaitDialogProgressBar()
    {
        if (mPanel != nullptr)
            MyGUI::Gui::getInstance().destroyWidget(mPanel);
    }

    void WaitDialogProgressBar::onOpen()
    {
        center();
        if (mPanel == nullptr)
            return;
        hideWindow(mMainWidget);
        mPanel->setVisible(true);
        drawPanel();
    }

    void WaitDialogProgressBar::onClose()
    {
        if (mPanel != nullptr)
            mPanel->setVisible(false);
    }

    void WaitDialogProgressBar::setProgress(int cur, int total)
    {
        mProgressBar->setProgressRange(total);
        mProgressBar->setProgressPosition(cur);
        mProgressText->setCaption(std::format("{}/{}", cur, total));
        mCurrent = cur;
        mTotal = total;
        if (mPanel != nullptr && mPanel->getVisible())
            drawPanel();
    }

    void WaitDialogProgressBar::drawPanel()
    {
        drawProgressPanel(mPanel, std::to_string(mCurrent) + " / " + std::to_string(mTotal) + " #{Interface:RestHours}",
            mTotal > 0 ? static_cast<float>(mCurrent) / mTotal : 0.f);
    }

    // ---------------------------------------------------------------------------------------------------------

    WaitDialog::WaitDialog()
        : WindowBase("openmw_wait_dialog.layout")
        , mSleeping(false)
        , mHours(1)
        , mManualHours(1)
        , mFadeTimeRemaining(0)
        , mInterruptAt(-1)
        , mProgressBar()
    {
        getWidget(mDateTimeText, "DateTimeText");
        getWidget(mRestText, "RestText");
        getWidget(mHourText, "HourText");
        getWidget(mUntilHealedButton, "UntilHealedButton");
        getWidget(mWaitButton, "WaitButton");
        getWidget(mCancelButton, "CancelButton");
        getWidget(mHourSlider, "HourSlider");

        mCancelButton->eventMouseButtonClick += MyGUI::newDelegate(this, &WaitDialog::onCancelButtonClicked);
        mUntilHealedButton->eventMouseButtonClick += MyGUI::newDelegate(this, &WaitDialog::onUntilHealedButtonClicked);
        mWaitButton->eventMouseButtonClick += MyGUI::newDelegate(this, &WaitDialog::onWaitButtonClicked);
        mHourSlider->eventScrollChangePosition += MyGUI::newDelegate(this, &WaitDialog::onHourSliderChangedPosition);

        mCancelButton->eventKeyButtonPressed += MyGUI::newDelegate(this, &WaitDialog::onKeyButtonPressed);
        mWaitButton->eventKeyButtonPressed += MyGUI::newDelegate(this, &WaitDialog::onKeyButtonPressed);
        mUntilHealedButton->eventKeyButtonPressed += MyGUI::newDelegate(this, &WaitDialog::onKeyButtonPressed);

        mTimeAdvancer.eventProgressChanged += MyGUI::newDelegate(this, &WaitDialog::onWaitingProgressChanged);
        mTimeAdvancer.eventInterrupted += MyGUI::newDelegate(this, &WaitDialog::onWaitingInterrupted);
        mTimeAdvancer.eventFinished += MyGUI::newDelegate(this, &WaitDialog::onWaitingFinished);

        mControllerButtons.mB = "#{Interface:Cancel}";
        mDisableGamepadCursor = Settings::gui().mControllerMenus;
        mPanel = createPanelRoot("ControllerMenus");
    }

    WaitDialog::~WaitDialog()
    {
        if (mPanel != nullptr)
            MyGUI::Gui::getInstance().destroyWidget(mPanel);
    }

    void WaitDialog::onClose()
    {
        if (mPanel != nullptr)
            mPanel->setVisible(false);
    }

    void WaitDialog::changeHours(int delta)
    {
        const int range = static_cast<int>(mHourSlider->getScrollRange());
        const int position = std::clamp(static_cast<int>(mHourSlider->getScrollPosition()) + delta, 0, range - 1);
        mHourSlider->setScrollPosition(position);
        onHourSliderChangedPosition(mHourSlider, position);
        drawPanel();
    }

    void WaitDialog::drawPanel()
    {
        if (mPanel == nullptr || !mPanel->getVisible())
            return;
        clearPanel(mPanel);
        const MyGUI::IntSize size = mPanel->getSize();
        constexpr int w = 560;
        constexpr int h = 330;
        const int x = (size.width - w) / 2;
        const int y = (size.height - h) / 2;
        addRect(mPanel, 0, 0, size.width, size.height, MyGUI::Colour::Black, 0.4f);
        addRect(mPanel, x, y, w, h, sPanelBackColour);
        addRect(mPanel, x, y, w, 1, sAccentColour, 0.6f);
        const int innerX = x + 28;
        const int innerW = w - 56;

        // What kind of rest this is, and the time now.
        addText(mPanel, innerX, y + 16, innerW, 36, mSleeping ? "#{sRest}" : "#{sWait}", sFontTitle, sBrightColour);
        addText(mPanel, innerX, y + 16, innerW, 36, mDateTimeText->getCaption().asUTF8(), sFontSmall, sDimColour,
            MyGUI::Align::Right | MyGUI::Align::VCenter);
        int ty = y + 58;
        if (!mSleeping)
            for (const std::string& line :
                wrapText(mPanel, localised(mRestText->getCaption().asUTF8()), innerW, sFontNormal, 2))
            {
                addText(mPanel, innerX, ty, innerW, 26, escaped(line), sFontNormal, sTextColour);
                ty += 26;
            }

        // The hours, with arrows for the d-pad, and when the rest ends.
        ty = y + 128;
        const int range = static_cast<int>(mHourSlider->getScrollRange());
        addGlyph(mPanel, innerX, ty + 10, 26, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
        addGlyph(mPanel, innerX + innerW - 26, ty + 10, 26, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
        addText(mPanel, innerX, ty, innerW, 46,
            std::to_string(mManualHours) + " "
                + (mManualHours == 1 ? "#{Interface:RestHour}" : "#{Interface:RestHours}"),
            sFontTitle, sActiveColour, MyGUI::Align::Center);
        ty += 54;
        addRect(mPanel, innerX, ty, innerW, 8, MyGUI::Colour::Black, 0.6f);
        if (range > 1)
            addRect(mPanel, innerX, ty, innerW * mManualHours / range, 8, sAccentColour);
        ty += 18;
        const MWWorld::DateTimeManager& timeManager = *MWBase::Environment::get().getWorld()->getTimeManager();
        const int endHour = static_cast<int>(timeManager.getTimeStamp().getHour()) + mManualHours;
        std::string ends = "#{Interface:RestUntil} " + hourText(endHour % 24);
        if (endHour >= 24)
            ends += ", " + std::string(endHour >= 48 ? "#{Interface:RestInTwoDays}" : "#{Interface:RestTomorrow}");
        addText(mPanel, innerX, ty, innerW, 26, ends, sFontNormal, sTextColour, MyGUI::Align::Center);

        // Resting until healed, when the player is hurt and can sleep here.
        ty += 36;
        if (mUntilHealedButton->getVisible())
        {
            const int hours = MWBase::Environment::get().getMechanicsManager()->getHoursToRest();
            addText(mPanel, innerX, ty, innerW, 26,
                "#{Interface:RestHealedIn} " + std::to_string(hours) + " "
                    + (hours == 1 ? "#{Interface:RestHour}" : "#{Interface:RestHours}"),
                sFontSmall, sDimColour, MyGUI::Align::Center);
        }

        // The button hints along the bottom.
        int hx = innerX;
        const int hintY = y + h - 44;
        const auto hint = [&](SDL_GameControllerButton button, const std::string& label) {
            addGlyph(mPanel, hx, hintY + 4, 22, button);
            MyGUI::TextBox* text = addText(mPanel, hx + 30, hintY, 220, 30, label, sFontNormal, sBrightColour);
            hx += 30 + text->getTextSize().width + 22;
        };
        hint(SDL_CONTROLLER_BUTTON_A, mSleeping ? "#{sRest}" : "#{sWait}");
        if (mUntilHealedButton->getVisible())
            hint(SDL_CONTROLLER_BUTTON_X, "#{sUntilHealed}");
        hint(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, "#{Interface:RestOneHour}");
        hint(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, "#{Interface:RestAllDay}");
    }

    void WaitDialog::setPtr(const MWWorld::Ptr& ptr)
    {
        const int restFlags = MWBase::Environment::get().getWorld()->canRest();

        const bool underwater = (restFlags & MWBase::World::Rest_PlayerIsUnderwater) != 0;
        // Resting in air is allowed if you're using a bed
        const bool inAir = ptr.isEmpty() && (restFlags & MWBase::World::Rest_PlayerIsInAir) != 0;
        const bool enemiesNearby = (restFlags & MWBase::World::Rest_EnemiesAreNearby) != 0;
        const bool solidGround = !underwater && !inAir;

        if (!solidGround || enemiesNearby)
        {
            if (!solidGround)
                MWBase::Environment::get().getWindowManager()->messageBox("#{sNotifyMessage1}");

            if (enemiesNearby)
                MWBase::Environment::get().getWindowManager()->messageBox("#{sNotifyMessage2}");

            MWBase::Environment::get().getWindowManager()->popGuiMode();
            return;
        }

        const bool canSleep = !ptr.isEmpty() || (restFlags & MWBase::World::Rest_CanSleep) != 0;
        setCanRest(canSleep);

        if (mUntilHealedButton->getVisible())
            MWBase::Environment::get().getWindowManager()->setKeyFocusWidget(mUntilHealedButton);
        else
            MWBase::Environment::get().getWindowManager()->setKeyFocusWidget(mWaitButton);
    }

    bool WaitDialog::exit()
    {
        bool canExit = !mTimeAdvancer.isRunning(); // Only exit if not currently waiting
        if (canExit)
        {
            clear();
            stopWaiting();
        }
        return canExit;
    }

    void WaitDialog::clear()
    {
        mSleeping = false;
        mHours = 1;
        mManualHours = 1;
        mFadeTimeRemaining = 0;
        mInterruptAt = -1;
        mTimeAdvancer.stop();
    }

    void WaitDialog::onOpen()
    {
        if (mTimeAdvancer.isRunning())
        {
            mProgressBar.setVisible(true);
            setVisible(false);
            return;
        }
        else
        {
            mProgressBar.setVisible(false);
        }

        if (!MWBase::Environment::get().getWindowManager()->getRestEnabled())
        {
            MWBase::Environment::get().getWindowManager()->popGuiMode();
        }

        onHourSliderChangedPosition(mHourSlider, 0);
        mHourSlider->setScrollPosition(0);

        const MWWorld::DateTimeManager& timeManager = *MWBase::Environment::get().getWorld()->getTimeManager();
        std::string_view month = timeManager.getMonthName();
        int hour = static_cast<int>(timeManager.getTimeStamp().getHour());
        bool pm = hour >= 12;
        if (hour >= 13)
            hour -= 12;
        if (hour == 0)
            hour = 12;

        ESM::EpochTimeStamp currentDate = timeManager.getEpochTimeStamp();
        std::string_view formattedHour(pm ? "#{Calendar:pm}" : "#{Calendar:am}");
        std::string dateTimeText = std::format("{} {} (#{{Calendar:day}} {}) {} {}", currentDate.mDay, month,
            timeManager.getTimeStamp().getDay(), hour, formattedHour);
        mDateTimeText->setCaptionWithReplacing(dateTimeText);

        if (mPanel != nullptr)
        {
            hideWindow(mMainWidget);
            mHeldDirection.reset();
            mPanel->setVisible(true);
            drawPanel();
        }
    }

    void WaitDialog::onUntilHealedButtonClicked(MyGUI::Widget* /*sender*/)
    {
        int autoHours = MWBase::Environment::get().getMechanicsManager()->getHoursToRest();

        startWaiting(autoHours);
    }

    void WaitDialog::onWaitButtonClicked(MyGUI::Widget* /*sender*/)
    {
        startWaiting(mManualHours);
    }

    void WaitDialog::startWaiting(int hoursToWait)
    {
        if (Settings::saves().mAutosave) // autosaves when enabled
            MWBase::Environment::get().getStateManager()->quickSave("Autosave");

        MWBase::World* world = MWBase::Environment::get().getWorld();
        MWBase::Environment::get().getWindowManager()->fadeScreenOut(0.2f);
        mFadeTimeRemaining = 0.4f;
        setVisible(false);
        if (mPanel != nullptr)
            mPanel->setVisible(false);

        mHours = hoursToWait;

        // FIXME: move this somewhere else?
        mInterruptAt = -1;
        MWWorld::Ptr player = world->getPlayerPtr();
        if (mSleeping && player.getCell()->isExterior())
        {
            const ESM::RefId& regionstr = player.getCell()->getCell()->getRegion();
            if (!regionstr.empty())
            {
                const ESM::Region* region = world->getStore().get<ESM::Region>().find(regionstr);
                if (!region->mSleepList.empty())
                {
                    // figure out if player will be woken while sleeping
                    int x = Misc::Rng::rollDice(hoursToWait, world->getPrng());
                    float fSleepRandMod
                        = world->getStore().get<ESM::GameSetting>().find("fSleepRandMod")->mValue.getFloat();
                    if (x < fSleepRandMod * hoursToWait)
                    {
                        float fSleepRestMod
                            = world->getStore().get<ESM::GameSetting>().find("fSleepRestMod")->mValue.getFloat();
                        int interruptAtHoursRemaining = int(fSleepRestMod * hoursToWait);
                        if (interruptAtHoursRemaining != 0)
                        {
                            mInterruptAt = hoursToWait - interruptAtHoursRemaining;
                            mInterruptCreatureList = region->mSleepList;
                        }
                    }
                }
            }
        }

        mProgressBar.setProgress(0, hoursToWait);
    }

    void WaitDialog::onCancelButtonClicked(MyGUI::Widget* /*sender*/)
    {
        MWBase::Environment::get().getWindowManager()->removeGuiMode(GM_Rest);
    }

    void WaitDialog::onHourSliderChangedPosition(MyGUI::ScrollBar* sender, size_t position)
    {
        mHourText->setCaptionWithReplacing(std::format("{} #{{sRestMenu2}}", position + 1));
        mManualHours = static_cast<int>(position + 1);
        MWBase::Environment::get().getWindowManager()->setKeyFocusWidget(mWaitButton);
    }

    void WaitDialog::onKeyButtonPressed(MyGUI::Widget* /*sender*/, MyGUI::KeyCode key, MyGUI::Char character)
    {
        if (key == MyGUI::KeyCode::ArrowUp)
            mHourSlider->setScrollPosition(
                std::min(mHourSlider->getScrollPosition() + 1, mHourSlider->getScrollRange() - 1));
        else if (key == MyGUI::KeyCode::ArrowDown)
            mHourSlider->setScrollPosition(std::max(static_cast<int>(mHourSlider->getScrollPosition()) - 1, 0));
        else
            return;
        onHourSliderChangedPosition(mHourSlider, mHourSlider->getScrollPosition());
    }

    void WaitDialog::onWaitingProgressChanged(int cur, int total)
    {
        mProgressBar.setProgress(cur, total);
        MWBase::Environment::get().getMechanicsManager()->rest(1, mSleeping);
        MWBase::Environment::get().getWorld()->advanceTime(1);

        MWWorld::Ptr player = MWBase::Environment::get().getWorld()->getPlayerPtr();
        if (player.getClass().getCreatureStats(player).isDead())
            stopWaiting();
    }

    void WaitDialog::onWaitingInterrupted()
    {
        MWBase::Environment::get().getWindowManager()->messageBox("#{sSleepInterrupt}");
        MWBase::Environment::get().getWorld()->spawnRandomCreature(mInterruptCreatureList);
        stopWaiting();
    }

    void WaitDialog::onWaitingFinished()
    {
        stopWaiting();

        MWWorld::Ptr player = MWMechanics::getPlayer();
        const MWMechanics::NpcStats& pcstats = player.getClass().getNpcStats(player);

        // trigger levelup if possible
        const MWWorld::Store<ESM::GameSetting>& gmst
            = MWBase::Environment::get().getESMStore()->get<ESM::GameSetting>();
        if (mSleeping && pcstats.getLevelProgress() >= gmst.find("iLevelUpTotal")->mValue.getInteger())
        {
            MWBase::Environment::get().getWindowManager()->pushGuiMode(GM_Levelup);
        }
    }

    void WaitDialog::setCanRest(bool canRest)
    {
        MWWorld::Ptr player = MWMechanics::getPlayer();
        MWMechanics::CreatureStats& stats = player.getClass().getCreatureStats(player);
        bool full = (stats.getHealth().getCurrent() >= stats.getHealth().getModified())
            && (stats.getMagicka().getCurrent() >= stats.getMagicka().getModified());
        MWMechanics::NpcStats& npcstats = player.getClass().getNpcStats(player);
        bool werewolf = npcstats.isWerewolf();

        mUntilHealedButton->setVisible(canRest && !full);
        mWaitButton->setCaptionWithReplacing(canRest ? "#{sRest}" : "#{sWait}");
        mRestText->setCaptionWithReplacing(
            canRest ? "#{sRestMenu3}" : (werewolf ? "#{sWerewolfRestMessage}" : "#{sRestIllegal}"));

        mSleeping = canRest;

        Gui::Box* box = dynamic_cast<Gui::Box*>(mMainWidget);
        if (box == nullptr)
            throw std::runtime_error("main widget must be a box");
        box->notifyChildrenSizeChanged();
        center();
        drawPanel();
    }

    void WaitDialog::onFrame(float dt)
    {
        mTimeAdvancer.onFrame(dt);

        // The d-pad and the left stick change the hours, repeating while held.
        int dx = 0;
        int dy = 0;
        if (mPanel != nullptr && mPanel->getVisible() && mHeldDirection.update(dt, dx, dy) && dx != 0)
            changeHours(dx);

        if (mFadeTimeRemaining <= 0)
            return;

        mFadeTimeRemaining -= dt;

        if (mFadeTimeRemaining <= 0)
        {
            mProgressBar.setVisible(true);
            mTimeAdvancer.run(mHours, mInterruptAt);
        }
    }

    ControllerButtons* WaitDialog::getControllerButtons()
    {
        mControllerButtons.mX.clear();
        if (mSleeping)
        {
            mControllerButtons.mA = "#{Interface:Rest}";
            if (mUntilHealedButton->getVisible())
                mControllerButtons.mX = "#{Interface:UntilHealed}";
        }
        else
        {
            mControllerButtons.mA = "#{Interface:Wait}";
        }
        return &mControllerButtons;
    }

    bool WaitDialog::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
    {
        if (arg.button == SDL_CONTROLLER_BUTTON_A)
        {
            onWaitButtonClicked(mWaitButton);
            MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_B)
            onCancelButtonClicked(mCancelButton);
        else if (arg.button == SDL_CONTROLLER_BUTTON_X && mUntilHealedButton->getVisible())
        {
            onUntilHealedButtonClicked(mUntilHealedButton);
            MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
        }
        else if (mPanel != nullptr
            && (arg.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT || arg.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
        {
            // The panel reads the d-pad every frame in onFrame, so it can repeat while held.
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT)
            MWBase::Environment::get().getWindowManager()->injectKeyPress(MyGUI::KeyCode::ArrowDown, 0, false);
        else if (arg.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT)
            MWBase::Environment::get().getWindowManager()->injectKeyPress(MyGUI::KeyCode::ArrowUp, 0, false);
        else if (arg.button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER)
        {
            mHourSlider->setScrollPosition(0);
            onHourSliderChangedPosition(mHourSlider, mHourSlider->getScrollPosition());
            drawPanel();
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)
        {
            mHourSlider->setScrollPosition(mHourSlider->getScrollRange() - 1);
            onHourSliderChangedPosition(mHourSlider, mHourSlider->getScrollPosition());
            drawPanel();
        }

        return true;
    }

    void WaitDialog::stopWaiting()
    {
        MWBase::Environment::get().getWindowManager()->fadeScreenIn(0.2f);
        mProgressBar.setVisible(false);
        MWBase::Environment::get().getWindowManager()->removeGuiMode(GM_Rest);
        mTimeAdvancer.stop();
    }

    void WaitDialog::wakeUp()
    {
        mSleeping = false;
        if (mInterruptAt != -1)
            onWaitingInterrupted();
        else
            stopWaiting();
    }

}
