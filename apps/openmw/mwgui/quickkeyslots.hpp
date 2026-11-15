#ifndef MWGUI_QUICKKEYSLOTS_H
#define MWGUI_QUICKKEYSLOTS_H

#include <array>
#include <functional>

#include <SDL_gamecontroller.h>

namespace MWGui::QuickKeySlots
{
    // In controller menus mode, the d-pad uses the quick keys during play. Pressing up, right, down or left uses slots
    // 1 to 4, and holding the same directions uses slots 5 to 8.
    inline constexpr std::array<SDL_GameControllerButton, 4> sDirections = { SDL_CONTROLLER_BUTTON_DPAD_UP,
        SDL_CONTROLLER_BUTTON_DPAD_RIGHT, SDL_CONTROLLER_BUTTON_DPAD_DOWN, SDL_CONTROLLER_BUTTON_DPAD_LEFT };

    // How long a direction is held before it counts as a hold, in seconds.
    inline constexpr float sHoldTime = 0.35f;

    /// Returns the button's position in sDirections, or -1 for a button that isn't on the d-pad.
    inline int directionOf(int button)
    {
        for (std::size_t i = 0; i < sDirections.size(); ++i)
            if (sDirections[i] == button)
                return static_cast<int>(i);
        return -1;
    }

    /// Returns the slot, from 1 to 8, that pressing or holding the direction uses.
    inline int slotFor(int direction, bool held)
    {
        return direction + 1 + (held ? static_cast<int>(sDirections.size()) : 0);
    }

    /// A d-pad direction while it is held down during play, to tell a press from a hold.
    struct DpadPress
    {
        bool mDown = false;
        // Whether the press has used its quick key, or is to do nothing
        bool mDone = false;
        float mTime = 0.f;
    };

    /// Follows a d-pad direction from one frame to the next, and returns the slot to use now, or 0 for none. Down is
    /// whether the direction is held, and playing whether the game is outside the menus. holdAssigned says whether
    /// something is on the direction's hold slot. It is only asked when a press starts during play.
    inline int updatePress(
        DpadPress& press, int direction, bool down, bool playing, const std::function<bool()>& holdAssigned, float dt)
    {
        if (!down)
        {
            // A direction let go before it became a hold uses its press slot.
            const bool use = press.mDown && !press.mDone && playing;
            press = DpadPress{};
            return use ? slotFor(direction, false) : 0;
        }
        if (!press.mDown)
        {
            press.mDown = true;
            // A press that starts in a menu, such as one that moves the selection there, does nothing in play.
            press.mDone = !playing;
            // When nothing is on the direction's hold, there is no need to wait and see if the press becomes one.
            if (!press.mDone && !holdAssigned())
            {
                press.mDone = true;
                return slotFor(direction, false);
            }
            return 0;
        }
        press.mTime += dt;
        if (!playing)
            press.mDone = true;
        if (!press.mDone && press.mTime >= sHoldTime)
        {
            press.mDone = true;
            return slotFor(direction, true);
        }
        return 0;
    }
}

#endif
