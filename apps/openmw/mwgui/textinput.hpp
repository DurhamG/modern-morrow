#ifndef MWGUI_TEXT_INPUT_H
#define MWGUI_TEXT_INPUT_H

#include "controllerscreen.hpp"
#include "windowbase.hpp"

namespace MWGui
{
    class TextInputDialog : public WindowModal
    {
    public:
        TextInputDialog();
        ~TextInputDialog() override;

        std::string getTextInput() const;
        void setTextInput(const std::string& text);

        void setNextButtonShow(bool shown);
        void setTextLabel(std::string_view label);
        void onOpen() override;
        void onClose() override;
        void onFrame(float duration) override;

        bool exit() override { return false; }

        /** Event : Dialog finished, OK button clicked.\n
            signature : void method()\n
        */
        EventHandle_WindowBase eventDone;

    protected:
        void onOkClicked(MyGUI::Widget* sender);
        void onTextAccepted(MyGUI::EditBox* sender);
        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg) override;
        bool takesTypedText() const override { return true; }

    private:
        MyGUI::EditBox* mTextEdit;

        // In controller menus mode, a panel with an on-screen keyboard shows instead of this window. The edit box
        // keeps the text, so a real keyboard still types into it.
        MyGUI::Widget* mPanel = nullptr;
        std::string mLabel;
        int mKeyRow = 0;
        int mKeyColumn = 0;
        bool mShift = false;
        HeldDirection mHeldDirection;
        void drawPanel();
        void moveKey(int dx, int dy);
        void pressKey();
        void typeText(const std::string& text);
        void deleteLast();
        void onTextChanged(MyGUI::EditBox* sender);
    };
}
#endif
