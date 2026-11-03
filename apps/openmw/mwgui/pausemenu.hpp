#ifndef MWGUI_PAUSEMENU_H
#define MWGUI_PAUSEMENU_H

#include <string>
#include <vector>

#include <SDL_events.h>

#include "controllerscreen.hpp"

namespace MyGUI
{
    class Widget;
}

namespace MWGui
{
    /// The menu the start button opens during a game, in controller menus mode. It lists Save, Load, Options and
    /// Exit over a darkened view of the game, in the style of the other controller screens. B returns to the game.
    /// The main menu still opens the save dialog, the options and the exit confirmation, and this menu asks it to.
    class PauseMenu
    {
    public:
        /// The items, by the main menu's button IDs, such as "savegame".
        void show(const std::vector<std::string>& items);
        void hide();
        bool isVisible() const;

        /// Returns the ID of the focused item when A is pressed, or "return" when B or the start button is.
        std::string onControllerButtonEvent(const SDL_ControllerButtonEvent& arg);
        void onFrame(float duration);

    private:
        MyGUI::Widget* mRoot = nullptr;
        std::vector<std::string> mItems;
        int mFocus = 0;
        HeldDirection mHeldDirection;

        void redraw();
    };
}

#endif
