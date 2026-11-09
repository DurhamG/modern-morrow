#ifndef MWINPUT_MWCONTROLLERMANAGER_H
#define MWINPUT_MWCONTROLLERMANAGER_H

#include <array>
#include <filesystem>
#include <set>
#include <string>

#include <components/sdlutil/events.hpp>
#include <components/settings/settings.hpp>

namespace MWInput
{
    class BindingsManager;
    class MouseManager;

    class ControllerManager : public SDLUtil::ControllerListener
    {
    public:
        ControllerManager(BindingsManager* bindingsManager, MouseManager* mouseManager,
            const std::filesystem::path& userControllerBindingsFile,
            const std::filesystem::path& controllerBindingsFile);

        virtual ~ControllerManager() = default;

        void update(float dt);

        void buttonPressed(int deviceID, const SDL_ControllerButtonEvent& arg) override;
        void buttonReleased(int deviceID, const SDL_ControllerButtonEvent& arg) override;
        void axisMoved(int deviceID, const SDL_ControllerAxisEvent& arg) override;
        void controllerAdded(int deviceID, const SDL_ControllerDeviceEvent& arg) override;
        void controllerRemoved(const SDL_ControllerDeviceEvent& arg) override;

        void touchpadMoved(int deviceId, const SDLUtil::TouchEvent& arg) override;
        void touchpadPressed(int deviceId, const SDLUtil::TouchEvent& arg) override;
        void touchpadReleased(int deviceId, const SDLUtil::TouchEvent& arg) override;

        void setJoystickLastUsed(bool enabled) { mJoystickLastUsed = enabled; }
        bool joystickLastUsed() const { return mJoystickLastUsed; }

        void setGuiCursorEnabled(bool enabled) { mGuiCursorEnabled = enabled; }

        void setGamepadGuiCursorEnabled(bool enabled) { mGamepadGuiCursorEnabled = enabled; }
        bool gamepadGuiCursorEnabled() const { return mGamepadGuiCursorEnabled; }

        /// Returns a value from -1 to 1. While a controller menu shows, keys can stand in for the right stick.
        float getAxisValue(SDL_GameControllerAxis axis) const;
        /// While a controller menu shows, a key that stands in for the button counts too.
        bool isButtonPressed(SDL_GameControllerButton button) const;

        /// In controller menus mode, the menus only respond to controller buttons. While one shows, this turns a key
        /// into the button it stands for, and sends that to the menu, so the menus still work without a controller,
        /// such as when its battery runs out. Returns true when the key was used.
        bool keyToMenuButton(const SDL_KeyboardEvent& arg);

        /// Records that a key is up again, for isButtonPressed and getAxisValue.
        void keyReleased(const SDL_KeyboardEvent& arg);

        bool isGyroAvailable() const;
        std::array<float, 3> getGyroValues() const;

        std::string getControllerButtonIcon(int button);
        std::string getControllerAxisIcon(int axis);

    private:
        // Return true if GUI consumes input.
        bool gamepadToGuiControl(const SDL_ControllerButtonEvent& arg);
        bool gamepadToGuiControl(const SDL_ControllerAxisEvent& arg);

        void enableGyroSensor();

        /// Whether keys stand in for controller buttons now: a controller menu shows, and it isn't taking typing.
        bool keysStandInForButtons() const;
        bool isKeyHeld(SDL_Scancode key) const { return mHeldKeys.contains(key); }

        int getControllerType();

        BindingsManager* mBindingsManager;
        MouseManager* mMouseManager;

        bool mGyroAvailable;
        bool mGamepadGuiCursorEnabled;
        bool mGuiCursorEnabled;
        bool mJoystickLastUsed;
        bool mGamepadMousePressed;
        bool mLeftTriggerGuiPressed;
        bool mRightTriggerGuiPressed;

        // The keys that are down. They come from the key events, rather than from SDL's keyboard state, so keys sent
        // as events, such as by the automation harness, count too.
        std::set<SDL_Scancode> mHeldKeys;
    };
}
#endif
