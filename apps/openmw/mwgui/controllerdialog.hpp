#ifndef MWGUI_CONTROLLERDIALOG_H
#define MWGUI_CONTROLLERDIALOG_H

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <SDL_events.h>

namespace MyGUI
{
    class Widget;
}

namespace MWGui
{
    /// A question with a list of answers in controller menus mode, such as a script's message box or "Are you sure?".
    /// It is an opaque panel in the middle of the screen, over a darkened view of whatever is behind it. Up and down
    /// move the highlight and A picks the highlighted answer. The window that asks the question stays hidden and acts
    /// on the answer.
    class ControllerDialog
    {
    public:
        ~ControllerDialog();

        /// Shows the message and the answers, with the answer at focus highlighted. B picks the cancel answer, when
        /// there is one.
        void show(const std::string& message, std::vector<std::string> answers, std::size_t focus,
            std::optional<std::size_t> cancel);
        void hide();

        /// Moves the highlight, or returns the answer the player picked.
        std::optional<std::size_t> onControllerButtonEvent(const SDL_ControllerButtonEvent& arg);

    private:
        MyGUI::Widget* mRoot = nullptr;
        std::string mMessage;
        std::vector<std::string> mAnswers;
        std::size_t mFocus = 0;
        std::optional<std::size_t> mCancel;

        void draw();
    };
}

#endif
