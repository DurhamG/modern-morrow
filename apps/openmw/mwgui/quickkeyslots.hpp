#ifndef MWGUI_QUICKKEYSLOTS_H
#define MWGUI_QUICKKEYSLOTS_H

#include <array>

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
}

#endif
