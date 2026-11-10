#include "countdialog.hpp"

#include <MyGUI_Button.h>
#include <MyGUI_Gui.h>
#include <MyGUI_ILayer.h>
#include <MyGUI_ImageBox.h>
#include <MyGUI_LayerManager.h>
#include <MyGUI_RenderManager.h>
#include <MyGUI_ScrollBar.h>

#include <components/settings/values.hpp>
#include <components/widgets/numericeditbox.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"

namespace MWGui
{
    CountDialog::CountDialog()
        : WindowModal("openmw_count_window.layout")
    {
        getWidget(mSlider, "CountSlider");
        getWidget(mItemEdit, "ItemEdit");
        getWidget(mItemText, "ItemText");
        getWidget(mLabelText, "LabelText");
        getWidget(mOkButton, "OkButton");
        getWidget(mCancelButton, "CancelButton");

        mCancelButton->eventMouseButtonClick += MyGUI::newDelegate(this, &CountDialog::onCancelButtonClicked);
        mOkButton->eventMouseButtonClick += MyGUI::newDelegate(this, &CountDialog::onOkButtonClicked);
        mItemEdit->eventValueChanged += MyGUI::newDelegate(this, &CountDialog::onEditValueChanged);
        mSlider->eventScrollChangePosition += MyGUI::newDelegate(this, &CountDialog::onSliderMoved);
        // make sure we read the enter key being pressed to accept multiple items
        mItemEdit->eventEditSelectAccept += MyGUI::newDelegate(this, &CountDialog::onEnterKeyPressed);

        mControllerButtons.mA = "#{Interface:OK}";
        mControllerButtons.mB = "#{Interface:Cancel}";

        if (Settings::gui().mControllerMenus)
            if (MyGUI::ILayer* layer = MyGUI::LayerManager::getInstance().getByName("ControllerModal", false))
            {
                mPanel = MyGUI::Gui::getInstance().createWidget<MyGUI::Widget>(
                    "", MyGUI::IntCoord(MyGUI::IntPoint(), layer->getSize()), MyGUI::Align::Default, "ControllerModal");
                mPanel->setVisible(false);
            }
    }

    CountDialog::~CountDialog()
    {
        if (mPanel != nullptr)
            MyGUI::Gui::getInstance().destroyWidget(mPanel);
    }

    void CountDialog::onClose()
    {
        if (mPanel != nullptr)
            mPanel->setVisible(false);
        WindowModal::onClose();
    }

    void CountDialog::onFrame(float duration)
    {
        // The d-pad and the left stick change the count, repeating while held.
        int dx = 0;
        int dy = 0;
        if (mPanel != nullptr && mPanel->getVisible() && mHeldDirection.update(duration, dx, dy) && dx != 0)
        {
            setCount(static_cast<int>(mSlider->getScrollPosition()) + 1 + dx);
            drawPanel();
        }
    }

    void CountDialog::drawPanel()
    {
        using namespace ControllerUi;
        mPanel->setSize(mPanel->getLayer()->getSize());
        while (mPanel->getChildCount() > 0)
            MyGUI::Gui::getInstance().destroyWidget(mPanel->getChildAt(0));
        const MyGUI::IntSize size = mPanel->getSize();
        addRect(mPanel, 0, 0, size.width, size.height, MyGUI::Colour::Black, 0.55f)->setNeedMouseFocus(true);
        constexpr int w = 520;
        constexpr int h = 250;
        const int x = (size.width - w) / 2;
        const int y = (size.height - h) / 2;
        addRect(mPanel, x, y, w, h, MyGUI::Colour::parse("#0d0b08"));
        addRect(mPanel, x, y, w, 1, sAccentColour, 0.6f);
        const int innerX = x + 28;
        const int innerW = w - 56;

        // What happens to the item, such as "Drop", and which item.
        addText(mPanel, innerX, y + 16, innerW, 34, mMessage, sFontTitle, sBrightColour);
        addText(mPanel, innerX, y + 52, innerW, 26, mItem, sFontNormal, sTextColour);

        // The count, with arrows for the d-pad, and a bar for how much of the stack it is.
        const int count = static_cast<int>(mSlider->getScrollPosition()) + 1;
        const int total = static_cast<int>(mSlider->getScrollRange());
        const int ty = y + 92;
        addGlyph(mPanel, innerX, ty + 10, 26, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
        addGlyph(mPanel, innerX + innerW - 26, ty + 10, 26, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
        // Clicking an arrow presses that direction on the d-pad, which changes the count by one.
        addPointerTarget(
            mPanel, innerX - 6, ty + 4, 38, 38, nullptr, [] { pressButton(SDL_CONTROLLER_BUTTON_DPAD_LEFT); });
        addPointerTarget(mPanel, innerX + innerW - 32, ty + 4, 38, 38, nullptr,
            [] { pressButton(SDL_CONTROLLER_BUTTON_DPAD_RIGHT); });
        addText(mPanel, innerX, ty, innerW, 46, std::to_string(count) + " / " + std::to_string(total), sFontTitle,
            sActiveColour, MyGUI::Align::Center);
        addRect(mPanel, innerX, ty + 56, innerW, 8, MyGUI::Colour::Black, 0.6f);
        if (total > 0)
            addRect(mPanel, innerX, ty + 56, innerW * count / total, 8, sAccentColour);

        // The button hints along the bottom.
        int hx = innerX;
        const int hintY = y + h - 44;
        const auto hint = [&](SDL_GameControllerButton button, const std::string& label) {
            addGlyph(mPanel, hx, hintY + 4, 22, button);
            MyGUI::TextBox* text = addText(mPanel, hx + 30, hintY, 200, 30, label, sFontNormal, sBrightColour);
            // Clicking a hint presses its button.
            addPointerTarget(
                mPanel, hx, hintY, 30 + text->getTextSize().width, 30, nullptr, [button] { pressButton(button); });
            hx += 30 + text->getTextSize().width + 22;
        };
        hint(SDL_CONTROLLER_BUTTON_A, "#{Interface:OK}");
        hint(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, "#{Interface:CountOne}");
        hint(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, "#{Interface:CountAll}");
    }

    void CountDialog::openCountDialog(const std::string& item, const std::string& message, const int maxCount)
    {
        setVisible(true);

        mLabelText->setCaptionWithReplacing(message);

        MyGUI::IntSize viewSize = MyGUI::RenderManager::getInstance().getViewSize();

        mSlider->setScrollRange(maxCount);
        mItemText->setCaptionWithReplacing(item);

        int width = std::max(mItemText->getTextSize().width + 160, 320);
        setCoord(viewSize.width / 2 - width / 2, viewSize.height / 2 - mMainWidget->getHeight() / 2, width,
            mMainWidget->getHeight());

        // by default, the text edit field has the focus of the keyboard
        MWBase::Environment::get().getWindowManager()->setKeyFocusWidget(mItemEdit);

        mSlider->setScrollPosition(maxCount - 1);

        mItemEdit->setMinValue(1);
        mItemEdit->setMaxValue(maxCount);
        mItemEdit->setValue(maxCount);

        if (mPanel != nullptr)
        {
            ControllerUi::hideWindow(mMainWidget);
            mItem = ControllerUi::escaped(ControllerUi::localised(item));
            mMessage = ControllerUi::localised(message);
            mHeldDirection.reset();
            mPanel->setVisible(true);
            MyGUI::LayerManager::getInstance().upLayerItem(mPanel);
            drawPanel();
        }
    }

    void CountDialog::setCount(int count)
    {
        count = std::clamp(count, 1, static_cast<int>(mSlider->getScrollRange()));
        mSlider->setScrollPosition(count - 1);
        mItemEdit->setValue(count);
    }

    void CountDialog::onCancelButtonClicked(MyGUI::Widget* /*sender*/)
    {
        setVisible(false);
    }

    void CountDialog::onOkButtonClicked(MyGUI::Widget* /*sender*/)
    {
        // The order here matters. Hide the dialog first so the OK event tooltips reappear.
        setVisible(false);
        eventOkClicked(nullptr, mSlider->getScrollPosition() + 1);
    }

    // essentially duplicating what the OK button does if user presses
    // Enter key
    void CountDialog::onEnterKeyPressed(MyGUI::EditBox* sender)
    {
        onOkButtonClicked(sender);

        // To do not spam onEnterKeyPressed() again and again
        MWBase::Environment::get().getWindowManager()->injectKeyRelease(MyGUI::KeyCode::None);
    }

    void CountDialog::onEditValueChanged(int value)
    {
        mSlider->setScrollPosition(value - 1);
    }

    void CountDialog::onSliderMoved(MyGUI::ScrollBar* sender, size_t position)
    {
        mItemEdit->setValue(static_cast<int>(position + 1));
    }

    bool CountDialog::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
    {
        if (arg.button == SDL_CONTROLLER_BUTTON_A)
            onOkButtonClicked(mOkButton);
        else if (arg.button == SDL_CONTROLLER_BUTTON_B)
            onCancelButtonClicked(mCancelButton);
        else if (arg.button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER)
            setCount(1);
        else if (arg.button == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)
            setCount(static_cast<int>(mSlider->getScrollRange()));
        else if (mPanel != nullptr)
        {
            // The panel reads the d-pad every frame in onFrame, so it can repeat while held.
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT)
            MWBase::Environment::get().getWindowManager()->injectKeyPress(MyGUI::KeyCode::ArrowDown, 0, false);
        else if (arg.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT)
            MWBase::Environment::get().getWindowManager()->injectKeyPress(MyGUI::KeyCode::ArrowUp, 0, false);

        if (mPanel != nullptr && mPanel->getVisible())
            drawPanel();
        return true;
    }
}
