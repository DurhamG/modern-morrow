#include "gamepad.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

#include <SDL_error.h>
#include <SDL_events.h>
#include <SDL_hints.h>
#include <SDL_version.h>

#include <components/debug/debuglog.hpp>

namespace MWAutomation
{
    namespace
    {
        // The buttons and axes the device reports, in SDL's order. SDL builds a gamepad mapping for a virtual
        // joystick from the masks below, matching joystick button N to the Nth set bit, so with these masks
        // joystick button N is gamepad button N.
        constexpr int sNumButtons = SDL_CONTROLLER_BUTTON_DPAD_RIGHT + 1;
        constexpr int sNumAxes = SDL_CONTROLLER_AXIS_MAX;
    }

    VirtualGamepad::VirtualGamepad()
    {
        // Without this, SDL drops joystick events while the game window doesn't have focus.
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    }

    VirtualGamepad::~VirtualGamepad()
    {
        disconnect();
    }

    void VirtualGamepad::connect()
    {
        if (isConnected())
            return;
#if SDL_VERSION_ATLEAST(2, 24, 0)
        SDL_VirtualJoystickDesc desc{};
        desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
        desc.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
        desc.naxes = sNumAxes;
        desc.nbuttons = sNumButtons;
        desc.button_mask = (1u << sNumButtons) - 1;
        desc.axis_mask = (1u << sNumAxes) - 1;
        desc.name = "OpenMW automation gamepad";

        const int deviceIndex = SDL_JoystickAttachVirtualEx(&desc);
        if (deviceIndex < 0)
            throw std::runtime_error(std::string("SDL_JoystickAttachVirtualEx failed: ") + SDL_GetError());
        mJoystick = SDL_JoystickOpen(deviceIndex);
        if (mJoystick == nullptr)
        {
            SDL_JoystickDetachVirtual(deviceIndex);
            throw std::runtime_error(std::string("SDL_JoystickOpen failed: ") + SDL_GetError());
        }
        mInstanceId = SDL_JoystickInstanceID(mJoystick);
        // Triggers rest at their minimum, which is -32768 for a raw joystick axis.
        SDL_JoystickSetVirtualAxis(mJoystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, SDL_JOYSTICK_AXIS_MIN);
        SDL_JoystickSetVirtualAxis(mJoystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, SDL_JOYSTICK_AXIS_MIN);
        Log(Debug::Info) << "Connected the automation virtual gamepad";
#else
        throw std::runtime_error("virtual gamepads need SDL 2.24 or later");
#endif
    }

    void VirtualGamepad::disconnect()
    {
        if (!isConnected())
            return;
        SDL_JoystickClose(mJoystick);
        mJoystick = nullptr;
        for (int i = 0; i < SDL_NumJoysticks(); ++i)
        {
            if (SDL_JoystickGetDeviceInstanceID(i) == mInstanceId)
            {
                SDL_JoystickDetachVirtual(i);
                break;
            }
        }
        mInstanceId = -1;
    }

    SDL_GameControllerButton VirtualGamepad::parseButton(std::string_view name)
    {
        const std::string text(name);
        const SDL_GameControllerButton button = SDL_GameControllerGetButtonFromString(text.c_str());
        if (button == SDL_CONTROLLER_BUTTON_INVALID || button >= sNumButtons)
            throw std::runtime_error("unknown gamepad button '" + text
                + "'. Use a, b, x, y, back, guide, start, leftstick, rightstick, leftshoulder, rightshoulder, "
                  "dpup, dpdown, dpleft or dpright.");
        return button;
    }

    SDL_GameControllerAxis VirtualGamepad::parseAxis(std::string_view name)
    {
        const std::string text(name);
        const SDL_GameControllerAxis axis = SDL_GameControllerGetAxisFromString(text.c_str());
        if (axis == SDL_CONTROLLER_AXIS_INVALID)
            throw std::runtime_error(
                "unknown gamepad axis '" + text + "'. Use leftx, lefty, rightx, righty, lefttrigger or righttrigger.");
        return axis;
    }

    void VirtualGamepad::setButton(SDL_GameControllerButton button, bool pressed)
    {
        connect();
        SDL_JoystickSetVirtualButton(mJoystick, button, pressed ? SDL_PRESSED : SDL_RELEASED);
    }

    void VirtualGamepad::setAxis(SDL_GameControllerAxis axis, float value)
    {
        connect();
        Sint16 raw;
        if (axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT || axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
        {
            // A gamepad trigger reads 0 to 32767, made from the full raw axis range.
            const float clamped = std::clamp(value, 0.f, 1.f);
            raw = static_cast<Sint16>(
                SDL_JOYSTICK_AXIS_MIN + clamped * (SDL_JOYSTICK_AXIS_MAX - SDL_JOYSTICK_AXIS_MIN));
        }
        else
            raw = static_cast<Sint16>(std::clamp(value, -1.f, 1.f) * SDL_JOYSTICK_AXIS_MAX);
        SDL_JoystickSetVirtualAxis(mJoystick, axis, raw);
    }
}
