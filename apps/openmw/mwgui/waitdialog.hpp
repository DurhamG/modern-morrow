#ifndef MWGUI_WAIT_DIALOG_H
#define MWGUI_WAIT_DIALOG_H

#include "timeadvancer.hpp"
#include "windowbase.hpp"
#include <components/esm/refid.hpp>

#include <functional>

namespace MWGui
{

    class WaitDialogProgressBar : public WindowBase
    {
    public:
        WaitDialogProgressBar();

        void onOpen() override;

        void setProgress(int cur, int total);

    protected:
        MyGUI::ProgressBar* mProgressBar;
        MyGUI::TextBox* mProgressText;
    };

    class WaitDialog : public WindowBase
    {
    public:
        WaitDialog();

        void setPtr(const MWWorld::Ptr& ptr) override;

        void onOpen() override;

        bool exit() override;

        void clear() override;

        void onFrame(float dt) override;
        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg) override;

        bool getSleeping() { return mTimeAdvancer.isRunning() && mSleeping; }
        void wakeUp();

        /// Whether the player is resting or waiting, from when the screen fades out until the rest stops.
        bool isWaiting() const { return mTimeAdvancer.isRunning() || mFadeTimeRemaining > 0 || mRequestedHours > 0; }

        /// Rests or waits for @a hoursToWait, as the window's buttons do, from the next frame on. When the rest stops,
        /// @a onStopped receives whether it lasted all the hours. It receives false if the window closes, or one of its
        /// buttons starts another rest, before this one starts.
        void requestWaiting(int hoursToWait, std::function<void(bool)> onStopped);
        void autosave();

        WindowBase* getProgressBar() { return &mProgressBar; }

        std::string_view getWindowIdForLua() const override { return "WaitDialog"; }

        ControllerButtons* getControllerButtons() override;

    protected:
        MyGUI::TextBox* mDateTimeText;
        MyGUI::TextBox* mRestText;
        MyGUI::TextBox* mHourText;
        MyGUI::Button* mUntilHealedButton;
        MyGUI::Button* mWaitButton;
        MyGUI::Button* mCancelButton;
        MyGUI::ScrollBar* mHourSlider;

        TimeAdvancer mTimeAdvancer;
        bool mSleeping;
        int mHours;
        int mManualHours; // stores the hours to rest selected via slider
        float mFadeTimeRemaining;

        int mInterruptAt;
        ESM::RefId mInterruptCreatureList;

        WaitDialogProgressBar mProgressBar;

        int mRequestedHours = 0;
        std::function<void(bool)> mOnStopped;

        void onUntilHealedButtonClicked(MyGUI::Widget* sender);
        void onWaitButtonClicked(MyGUI::Widget* sender);
        void onCancelButtonClicked(MyGUI::Widget* sender);
        void onHourSliderChangedPosition(MyGUI::ScrollBar* sender, size_t position);
        void onKeyButtonPressed(MyGUI::Widget* sender, MyGUI::KeyCode key, MyGUI::Char character);

        void onWaitingProgressChanged(int cur, int total);
        void onWaitingInterrupted();
        void onWaitingFinished();

        void setCanRest(bool canRest);

        void startWaiting(int hoursToWait);
        void stopWaiting(bool finished = false);
    };

}

#endif
