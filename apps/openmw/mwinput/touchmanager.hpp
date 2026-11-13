#ifndef MWINPUT_TOUCHMANAGER_H
#define MWINPUT_TOUCHMANAGER_H

#include <memory>
#include <vector>

#include <MyGUI_Types.h>
#include <SDL_touch.h>

#include <components/sdlutil/events.hpp>

struct SDL_Window;

namespace MWGui
{
    class TouchOverlay;
}

namespace MWInput
{
    class BindingsManager;

    /// Plays the game with fingers on a touch screen, as ui-rework/design/touch.md describes. During play the left
    /// thumb walks like a stick, and the right thumb looks and taps to act in the current mode. Swiping in from the
    /// left edge opens the inventory, and from the right edge picks the mode or a quick key. Corners quicksave,
    /// quickload and sneak.
    class TouchManager : public SDLUtil::TouchListener
    {
    public:
        TouchManager(BindingsManager* bindingsManager, SDL_Window* window);
        ~TouchManager() override;

        void fingerPressed(const SDL_TouchFingerEvent& arg) override;
        void fingerMoved(const SDL_TouchFingerEvent& arg) override;
        void fingerReleased(const SDL_TouchFingerEvent& arg) override;

        void update(float dt);

        /// Lets go of every finger and anything they hold, such as when a game loads.
        void reset();

    private:
        enum class Role
        {
            /// Does nothing until it lifts, such as a third finger, or one whose gesture is done.
            Ignored,
            /// The right thumb: looks around, and acts on a tap. A hold in attack mode charges.
            Look,
            /// The left thumb: walks and strafes.
            Move,
            /// A finger that went down by a side edge, until it shows whether it swipes in. If it doesn't, it goes
            /// back to its fallback role.
            Edge,
            /// The finger that swiped in from the right edge, choosing on the mode picker.
            Select,
            QuickSave,
            QuickLoad,
            Sneak,
            /// A finger tapping the quick keys radial.
            Radial,
        };

        /// The modes, which are also the mode picker's items. Its quick keys item opens the quick keys radial.
        enum Mode
        {
            Mode_Use = 0,
            Mode_Attack,
            Mode_QuickKeys,
            Mode_Magic,
            Mode_Jump,
            Mode_Count,
        };

        struct Finger
        {
            SDL_FingerID mId = 0;
            Role mRole = Role::Ignored;
            /// What an edge finger does when it doesn't swipe in.
            Role mFallback = Role::Ignored;
            /// For an edge finger, -1 for the left edge and 1 for the right.
            int mEdge = 0;
            MyGUI::FloatPoint mStart;
            MyGUI::FloatPoint mPosition;
            Uint32 mDownTime = 0;
            float mFarthest = 0;
            /// Whether the right thumb holds the attack.
            bool mHolding = false;
        };

        struct PendingAction
        {
            int mAction;
            int mFrames;
        };

        Finger* find(SDL_FingerID id);
        MyGUI::FloatPoint toPixels(const SDL_TouchFingerEvent& arg) const;
        MyGUI::IntSize screenSize() const;
        float pixelsPerMm() const;
        bool isTap(const Finger& finger, Uint32 upTime) const;
        bool inGame() const;
        /// The mode a tap acts in: jump, or what the player has out.
        int currentMode() const;
        /// The mode for what the player has out: use, attack or magic.
        int drawnMode() const;

        Role roleForGame(const MyGUI::FloatPoint& point) const;
        /// Which side edge's swiping band a point is in: -1 for the left, 1 for the right, or 0 for neither.
        int edgeAt(const MyGUI::FloatPoint& point) const;
        /// Whether an edge finger has swiped in, and acts if so. One that heads elsewhere goes back to its fallback.
        void updateEdge(Finger& finger);

        void look(const Finger& finger, const MyGUI::FloatPoint& previous);
        /// Turns the camera while the right thumb is past the edge of its circle.
        void keepTurning(const Finger& finger, float dt);
        /// Turns the player, in radians before the camera settings, as the mouse does.
        void turn(float yaw, float pitch);
        void act();
        void setMovement(const Finger& finger);
        void stopMovement();
        void pressUse(bool pressed);

        void openPicker(const Finger& finger);
        void updatePicker(const Finger& finger);
        void closePicker(bool commit);
        void openRadial();
        void tapRadial(const MyGUI::FloatPoint& point);
        void closeRadial();

        /// Runs an action after the given number of frames, so that it comes after the game unpauses.
        void queueAction(int action, int frames = 0);
        void pause(bool paused);

        BindingsManager* mBindingsManager;
        SDL_Window* mWindow;
        std::unique_ptr<MWGui::TouchOverlay> mOverlay;

        std::vector<Finger> mFingers;
        std::vector<PendingAction> mPending;
        int mUseFrames = 0;
        bool mUseHeld = false;
        bool mMoving = false;
        bool mPaused = false;

        /// Jump mode isn't something the player has out, so it's kept here.
        bool mJumpMode = false;

        bool mPickerOpen = false;
        int mPickerCurrent = Mode_Use;
        int mPickerHighlight = Mode_Use;
        /// The picker's boxes, one per mode, in screen pixels.
        std::vector<MyGUI::IntCoord> mPickerBoxes;
        /// Where the thumb was when the picker opened. Its moves from there pick the item.
        MyGUI::FloatPoint mPickerOrigin;
        MyGUI::FloatPoint mPickerCentre;

        bool mRadialOpen = false;
        std::vector<MyGUI::IntCoord> mRadialSlots;
        MyGUI::IntCoord mRadialCentre;
    };
}

#endif
