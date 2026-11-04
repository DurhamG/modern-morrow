#include "confirmationdialog.hpp"

#include <MyGUI_Button.h>
#include <MyGUI_EditBox.h>

#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"

#include "controllerscreen.hpp"

namespace MWGui
{
    ConfirmationDialog::ConfirmationDialog()
        : WindowModal("openmw_confirmation_dialog.layout")
    {
        getWidget(mMessage, "Message");
        getWidget(mOkButton, "OkButton");
        getWidget(mCancelButton, "CancelButton");

        mCancelButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ConfirmationDialog::onCancelButtonClicked);
        mOkButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ConfirmationDialog::onOkButtonClicked);

        if (Settings::gui().mControllerMenus)
        {
            mDisableGamepadCursor = true;
            mControllerButtons.mA = "#{Interface:OK}";
            mControllerButtons.mB = "#{Interface:Cancel}";
            mDialog = std::make_unique<ControllerDialog>();
        }
    }

    ConfirmationDialog::~ConfirmationDialog() = default;

    void ConfirmationDialog::onClose()
    {
        if (mDialog != nullptr)
            mDialog->hide();
    }

    void ConfirmationDialog::askForConfirmation(const std::string& message)
    {
        setVisible(true);

        mMessage->setCaptionWithReplacing(message);

        int height = mMessage->getTextSize().height + 60;

        int width = mMessage->getTextSize().width + 24;

        mMainWidget->setSize(width, height);

        mMessage->setSize(mMessage->getWidth(), mMessage->getTextSize().height + 24);

        MWBase::Environment::get().getWindowManager()->setKeyFocusWidget(mOkButton);

        if (Settings::gui().mControllerMenus)
        {
            mOkButtonFocus = true;
            mOkButton->setStateSelected(true);
            mCancelButton->setStateSelected(false);
        }

        center();

        if (mDialog != nullptr)
        {
            ControllerUi::hideWindow(mMainWidget);
            mDialog->show(message, { mOkButton->getCaption().asUTF8(), mCancelButton->getCaption().asUTF8() }, 0, 1);
        }
    }

    bool ConfirmationDialog::exit()
    {
        setVisible(false);
        eventCancelClicked();
        return true;
    }

    void ConfirmationDialog::onCancelButtonClicked(MyGUI::Widget* /*sender*/)
    {
        exit();
    }

    void ConfirmationDialog::onOkButtonClicked(MyGUI::Widget* /*sender*/)
    {
        setVisible(false);

        eventOkClicked();
    }

    bool ConfirmationDialog::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
    {
        if (mDialog != nullptr)
        {
            if (const std::optional<std::size_t> answer = mDialog->onControllerButtonEvent(arg))
            {
                if (*answer == 0)
                    onOkButtonClicked(mOkButton);
                else
                    onCancelButtonClicked(mCancelButton);
            }
            return true;
        }

        if (arg.button == SDL_CONTROLLER_BUTTON_A)
        {
            if (mOkButtonFocus)
                onOkButtonClicked(mOkButton);
            else
                onCancelButtonClicked(mCancelButton);
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_B)
        {
            onCancelButtonClicked(mCancelButton);
        }
        else if ((arg.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT && !mOkButtonFocus)
            || (arg.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT && mOkButtonFocus))
        {
            mOkButtonFocus = !mOkButtonFocus;
            mOkButton->setStateSelected(mOkButtonFocus);
            mCancelButton->setStateSelected(!mOkButtonFocus);
        }

        return true;
    }
}
