#ifndef MWGUI_JAILSCREEN_H
#define MWGUI_JAILSCREEN_H

#include "timeadvancer.hpp"
#include "windowbase.hpp"

namespace MWGui
{
    class JailScreen : public WindowBase
    {
    public:
        JailScreen();
        ~JailScreen() override;
        void goToJail(int days);

        void onOpen() override;
        void onClose() override;

        void onFrame(float dt) override;

        bool exit() override { return false; }

        std::string_view getWindowIdForLua() const override { return "JailScreen"; }

    private:
        int mDays;

        float mFadeTimeRemaining;

        MyGUI::ScrollBar* mProgressBar;

        void onJailProgressChanged(int cur, int total);
        void onJailFinished();

        TimeAdvancer mTimeAdvancer;

        // In controller menus mode, the progress shows in a panel that scales with the screen instead.
        MyGUI::Widget* mPanel = nullptr;
        float mProgress = 0.f;
        void drawPanel();
    };
}

#endif
