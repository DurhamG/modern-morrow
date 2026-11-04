#ifndef MWGUI_CONFIRMATIONDIALOG_H
#define MWGUI_CONFIRMATIONDIALOG_H

#include <memory>

#include "controllerdialog.hpp"
#include "windowbase.hpp"

namespace MWGui
{
    class ConfirmationDialog : public WindowModal
    {
    public:
        ConfirmationDialog();
        ~ConfirmationDialog() override;

        void onClose() override;
        void askForConfirmation(const std::string& message);
        bool exit() override;

        typedef MyGUI::delegates::MultiDelegate<> EventHandle_Void;

        /** Event : Ok button was clicked.\n
            signature : void method()\n
        */
        EventHandle_Void eventOkClicked;
        EventHandle_Void eventCancelClicked;

    private:
        MyGUI::EditBox* mMessage;
        MyGUI::Button* mOkButton;
        MyGUI::Button* mCancelButton;

        void onCancelButtonClicked(MyGUI::Widget* sender);
        void onOkButtonClicked(MyGUI::Widget* sender);

        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg) override;
        bool mOkButtonFocus = true;

        // In controller menus mode, the question shows in a dialog instead of this window.
        std::unique_ptr<ControllerDialog> mDialog;
    };

}

#endif
