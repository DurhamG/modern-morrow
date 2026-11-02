#ifndef OPENMW_MWAUTOMATION_GAMEPAD_H
#define OPENMW_MWAUTOMATION_GAMEPAD_H

#include <string_view>

#include <SDL_gamecontroller.h>
#include <SDL_joystick.h>

namespace MWAutomation
{
    // A gamepad that exists only in software, made with SDL's virtual joystick support. SDL reports it to the game
    // like a real controller, so the game opens it and reads its buttons and sticks through the normal code paths.
    class VirtualGamepad
    {
    public:
        VirtualGamepad();
        ~VirtualGamepad();

        VirtualGamepad(const VirtualGamepad&) = delete;
        VirtualGamepad& operator=(const VirtualGamepad&) = delete;

        // Throws std::runtime_error if SDL can't create the device.
        void connect();
        void disconnect();
        bool isConnected() const { return mJoystick != nullptr; }

        // Names are SDL's, such as "a", "b", "start", "dpup" or "leftshoulder". Throws std::runtime_error for unknown
        // names.
        static SDL_GameControllerButton parseButton(std::string_view name);
        // "leftx", "lefty", "rightx", "righty", "lefttrigger" or "righttrigger".
        static SDL_GameControllerAxis parseAxis(std::string_view name);

        // Connects first if needed.
        void setButton(SDL_GameControllerButton button, bool pressed);
        // Sticks take -1 to 1, with positive y pointing down. Triggers take 0 to 1.
        void setAxis(SDL_GameControllerAxis axis, float value);

    private:
        int mInstanceId = -1;
        SDL_Joystick* mJoystick = nullptr;
    };
}

#endif
