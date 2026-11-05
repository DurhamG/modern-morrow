#include "waitdialog.hpp"

#include <MyGUI_InputManager.h>
#include <MyGUI_ProgressBar.h>
#include <MyGUI_ScrollBar.h>

#include <components/misc/rng.hpp>

#include <components/esm3/loadregn.hpp>
#include <components/settings/values.hpp>
#include <components/widgets/box.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/luamanager.hpp"
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
#include "../mwmechanics/resting.hpp"

#include <format>
#include <utility>

namespace MWGui
{

    WaitDialogProgressBar::WaitDialogProgressBar(std::string_view luaId)
        : WindowBase("openmw_wait_dialog_progressbar.layout")
        , mLuaId(luaId)
    {
        getWidget(mProgressBar, "ProgressBar");
        getWidget(mProgressText, "ProgressText");
    }

    void WaitDialogProgressBar::onOpen()
    {
        center();
    }

    void WaitDialogProgressBar::setProgress(int cur, int total)
    {
        mProgressBar->setProgressRange(total);
        mProgressBar->setProgressPosition(cur);
        mProgressText->setCaption(std::format("{}/{}", cur, total));
    }

    // ---------------------------------------------------------------------------------------------------------

    WaitDialog::WaitDialog()
        : WindowBase("openmw_wait_dialog.layout")
        , mSleeping(false)
        , mHours(1)
        , mManualHours(1)
        , mFadeTimeRemaining(0)
        , mInterruptAt(-1)
        , mProgressBar("WaitDialogProgressBar")
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
    }

    void WaitDialog::setPtr(const MWWorld::Ptr& ptr)
    {
        const MWMechanics::RestConditions conditions = MWMechanics::getRestConditions(ptr);

        if (!conditions.canWait())
        {
            if (!conditions.mOnSolidGround)
                MWBase::Environment::get().getWindowManager()->messageBox("#{sNotifyMessage1}");

            if (conditions.mEnemiesNearby)
                MWBase::Environment::get().getWindowManager()->messageBox("#{sNotifyMessage2}");

            MWBase::Environment::get().getWindowManager()->popGuiMode();
            return;
        }

        setCanRest(conditions.mCanSleep);

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
            // Closing the menu also ends a rest that a script asked for, even if it hasn't started yet.
            std::function<void(bool)> onStopped = std::exchange(mOnStopped, nullptr);
            clear();
            stopWaiting();
            if (onStopped)
                onStopped(false);
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
        mRequestedHours = 0;
        mOnStopped = nullptr;
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

        onHourSliderChangedPosition(mHourSlider, 0);
        mHourSlider->setScrollPosition(0);

        const MWWorld::DateTimeManager& timeManager = *MWBase::Environment::get().getWorld()->getTimeManager();
        std::string_view month = timeManager.getMonthName();
        ESM::EpochTimeStamp currentDate = timeManager.getEpochTimeStamp();
        std::string timeOfDay = timeManager.formatTimeOfDay(timeManager.getTimeStamp().getHour());
        std::string dateTimeText = std::format(
            "{} {} (#{{Calendar:day}} {}) {}", currentDate.mDay, month, timeManager.getTimeStamp().getDay(), timeOfDay);
        mDateTimeText->setCaptionWithReplacing(dateTimeText);
    }

    void WaitDialog::setVisible(bool visible)
    {
        WindowBase::setVisible(visible);
        // A window that Lua replaced is never shown, so onOpen doesn't run for it. The rest menu still closes when
        // resting is off, such as during character creation.
        if (visible && !mTimeAdvancer.isRunning() && !MWBase::Environment::get().getWindowManager()->getRestEnabled())
            MWBase::Environment::get().getWindowManager()->popGuiMode();
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

    void WaitDialog::requestWaiting(int hoursToWait, std::function<void(bool)> onStopped)
    {
        mRequestedHours = hoursToWait;
        mOnStopped = std::move(onStopped);
    }

    void WaitDialog::startWaiting(int hoursToWait)
    {
        // A button press takes the place of a rest that a script asked for, if that rest hasn't started yet.
        if (mOnStopped)
            std::exchange(mOnStopped, nullptr)(false);
        mRequestedHours = 0;

        if (Settings::saves().mAutosave) // autosaves when enabled
            MWBase::Environment::get().getStateManager()->quickSave("Autosave");

        MWBase::World* world = MWBase::Environment::get().getWorld();
        MWBase::Environment::get().getWindowManager()->fadeScreenOut(0.2f);
        mFadeTimeRemaining = 0.4f;
        setVisible(false);

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
        MWBase::Environment::get().getLuaManager()->restProgress(cur, total);

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
        stopWaiting(true);

        MWWorld::Ptr player = MWMechanics::getPlayer();
        const MWMechanics::NpcStats& pcstats = player.getClass().getNpcStats(player);

        // trigger levelup if possible
        if (mSleeping && pcstats.isLevelUpReady())
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
    }

    void WaitDialog::onFrame(float dt)
    {
        // A rest that a script asked for starts here, outside the script's update, because it may save the game.
        if (mRequestedHours > 0)
        {
            std::function<void(bool)> onStopped = std::move(mOnStopped);
            startWaiting(mRequestedHours);
            mOnStopped = std::move(onStopped);
        }

        mTimeAdvancer.onFrame(dt);

        if (mFadeTimeRemaining <= 0)
            return;

        mFadeTimeRemaining -= dt;

        if (mFadeTimeRemaining <= 0)
        {
            mProgressBar.setVisible(true);
            mTimeAdvancer.run(mHours, mInterruptAt);
            MWBase::Environment::get().getLuaManager()->restProgress(0, mHours);
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
        else if (arg.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT)
            MWBase::Environment::get().getWindowManager()->injectKeyPress(MyGUI::KeyCode::ArrowDown, 0, false);
        else if (arg.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT)
            MWBase::Environment::get().getWindowManager()->injectKeyPress(MyGUI::KeyCode::ArrowUp, 0, false);
        else if (arg.button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER)
        {
            mHourSlider->setScrollPosition(0);
            onHourSliderChangedPosition(mHourSlider, mHourSlider->getScrollPosition());
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)
        {
            mHourSlider->setScrollPosition(mHourSlider->getScrollRange() - 1);
            onHourSliderChangedPosition(mHourSlider, mHourSlider->getScrollPosition());
        }

        return true;
    }

    void WaitDialog::stopWaiting(bool finished)
    {
        MWBase::Environment::get().getWindowManager()->fadeScreenIn(0.2f);
        mProgressBar.setVisible(false);
        MWBase::Environment::get().getWindowManager()->removeGuiMode(GM_Rest);
        mTimeAdvancer.stop();
        if (mOnStopped)
            std::exchange(mOnStopped, nullptr)(finished);
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
