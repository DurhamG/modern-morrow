#ifndef MWINPUT_ACTIONMANAGER_H
#define MWINPUT_ACTIONMANAGER_H

#include <array>
#include <functional>

namespace MWInput
{
    class BindingsManager;

    class ActionManager
    {
    public:
        ActionManager(BindingsManager* bindingsManager, std::function<void()> takeScreenshot);

        void update(float dt);

        void executeAction(int action);

        bool checkAllowedToUseItems() const;

        void toggleMainMenu();
        void toggleConsole();
        void screenshot();
        void activate();
        void rest();
        void quickLoad();
        void quickSave();

        void quickKey(int index);

        void resetIdleTime();
        float getIdleTime() const { return mTimeIdle; }

        bool isSneaking() const;

    private:
        void handleGuiArrowKey(int action);
        void updateDpadQuickKeys(float dt);

        // A d-pad direction while it is held down during play, to tell a press from a hold.
        struct DpadPress
        {
            bool mDown = false;
            // Whether the press has used its quick key, or is to do nothing
            bool mDone = false;
            float mTime = 0.f;
        };

        BindingsManager* mBindingsManager;
        std::function<void()> mTakeScreenshot;

        float mTimeIdle;
        std::array<DpadPress, 4> mDpadPresses;
    };
}
#endif
