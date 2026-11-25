#ifndef OPENMW_MWGUI_GAMEOPTIONSDIALOG_H
#define OPENMW_MWGUI_GAMEOPTIONSDIALOG_H

#include "controllerscreen.hpp"
#include "windowbase.hpp"

namespace MWGui
{
    /// The gameplay options a new game offers before the intro plays: the options that fix common complaints with
    /// Morrowind, such as Glancing Blows Instead of Missing and Quest Markers. Each row shows an option and its value, and the focused option's
    /// description shows beside the list. The player turns them on or off, then begins the game. It only shows in
    /// controller menus mode, since it is drawn as a controller screen.
    class GameOptionsDialog : public WindowModal
    {
    public:
        GameOptionsDialog();
        ~GameOptionsDialog() override;

        void onOpen() override;
        void onClose() override;
        void onFrame(float duration) override;
        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg) override;

        typedef MyGUI::delegates::MultiDelegate<> EventHandle_Void;

        /// The player chose to begin the game.
        EventHandle_Void eventBegin;

    private:
        void draw();
        /// Changes the focused option. With wrap, as A does, it turns an option on or off, or steps Run Fatigue along
        /// its choices and round from the last to the first. Without wrap, as the d-pad does, a positive delta turns
        /// an option on and a negative one turns it off, and Run Fatigue stops at either end of its choices.
        void change(int delta, bool wrap);

        MyGUI::Widget* mPanel = nullptr;
        ControllerList mList;
    };
}

#endif
