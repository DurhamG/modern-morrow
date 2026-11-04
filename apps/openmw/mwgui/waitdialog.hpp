#ifndef MWGUI_WAIT_DIALOG_H
#define MWGUI_WAIT_DIALOG_H

#include <components/esm/refid.hpp>

#include "controllerscreen.hpp"
#include "timeadvancer.hpp"
#include "windowbase.hpp"

namespace MWGui
{

    class WaitDialogProgressBar : public WindowBase
    {
    public:
        WaitDialogProgressBar();
        ~WaitDialogProgressBar() override;

        void onOpen() override;
        void onClose() override;

        void setProgress(int cur, int total);

    protected:
        MyGUI::ProgressBar* mProgressBar;
        MyGUI::TextBox* mProgressText;

        // In controller menus mode, the progress shows in a panel in the style of the controller screens instead.
        MyGUI::Widget* mPanel = nullptr;
        int mCurrent = 0;
        int mTotal = 0;
        void drawPanel();
    };

    class WaitDialog : public WindowBase
    {
    public:
        WaitDialog();
        ~WaitDialog() override;

        void setPtr(const MWWorld::Ptr& ptr) override;

        void onOpen() override;
        void onClose() override;

        bool exit() override;

        void clear() override;

        void onFrame(float dt) override;
        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg) override;

        bool getSleeping() { return mTimeAdvancer.isRunning() && mSleeping; }
        void wakeUp();
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
        void stopWaiting();

        // In controller menus mode, a panel in the middle of the screen shows the choice instead of this window.
        MyGUI::Widget* mPanel = nullptr;
        HeldDirection mHeldDirection;
        void drawPanel();
        void changeHours(int delta);
    };

}

#endif
