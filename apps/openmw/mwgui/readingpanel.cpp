#include "readingpanel.hpp"

#include <algorithm>
#include <cmath>

#include <MyGUI_Gui.h>
#include <MyGUI_ILayer.h>
#include <MyGUI_ImageBox.h>
#include <MyGUI_LayerManager.h>

#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"

#include "controllerscreen.hpp"
#include "formatting.hpp"

namespace MWGui
{
    namespace
    {
        using namespace ControllerUi;

        constexpr int sPageWidth = 640;
        constexpr int sPadding = 32;
        constexpr float sScrollSpeed = 700.f;

        // The colour of the inventory screen's pages, so books look the same in both.
        const MyGUI::Colour sPageColour = MyGUI::Colour::parse("#d8c7a0");
        const MyGUI::Colour sPanelBackColour = MyGUI::Colour::parse("#0d0b08");
    }

    ReadingPanel::~ReadingPanel()
    {
        if (mRoot != nullptr)
            MyGUI::Gui::getInstance().destroyWidget(mRoot);
    }

    void ReadingPanel::show(const std::string& title, const std::string& text, bool shrinkTextAtLastTag, bool canTake)
    {
        if (mRoot == nullptr)
        {
            MyGUI::ILayer* layer = MyGUI::LayerManager::getInstance().getByName("ControllerMenus", false);
            if (layer == nullptr)
                return;
            mRoot = MyGUI::Gui::getInstance().createWidget<MyGUI::Widget>(
                "", MyGUI::IntCoord(MyGUI::IntPoint(), layer->getSize()), MyGUI::Align::Default, "ControllerMenus");
            mRoot->setNeedMouseFocus(false);
        }
        mRoot->setSize(mRoot->getLayer()->getSize());
        mRoot->setVisible(true);
        while (mRoot->getChildCount() > 0)
            MyGUI::Gui::getInstance().destroyWidget(mRoot->getChildAt(0));
        mPaper = nullptr;
        mScrollThumb = nullptr;
        mScroll = 0.f;
        mScrollMax = 0;

        const MyGUI::IntSize size = mRoot->getSize();
        addRect(mRoot, 0, 0, size.width, size.height, MyGUI::Colour::Black, 0.6f);
        const int w = std::min(sPageWidth, size.width - 2 * sMargin);
        const int x = (size.width - w) / 2;
        constexpr int top = 24;
        const int bottom = size.height - 24;

        // The title above the page, and the button hints below it.
        addRect(mRoot, x, top, w, bottom - top, sPanelBackColour);
        addRect(mRoot, x, top, w, 1, sAccentColour, 0.6f);
        addText(mRoot, x + 20, top + 8, w - 40, 34, escaped(title), sFontLarge, sBrightColour);
        const int pageY = top + 50;
        const int pageH = bottom - 52 - pageY;
        addRect(mRoot, x + 12, pageY, w - 24, pageH, sPageColour);

        // The text, laid out by the same code as the book and scroll windows. A page tall enough for the whole text
        // keeps it in one column.
        MyGUI::Widget* textArea = mRoot->createWidget<MyGUI::Widget>("",
            MyGUI::IntCoord(x + 12 + sPadding, pageY + sPadding / 2, w - 24 - 2 * sPadding, pageH - sPadding),
            MyGUI::Align::Default);
        textArea->setNeedMouseFocus(false);
        Formatting::BookFormatter formatter;
        constexpr int unlimitedHeight = 1 << 20;
        formatter.markupToWidget(textArea, text, textArea->getWidth(), unlimitedHeight, shrinkTextAtLastTag);
        if (textArea->getChildCount() > 0)
        {
            mPaper = textArea->getChildAt(0);
            mScrollMax = std::max(0, mPaper->getHeight() - textArea->getHeight());
        }
        // The wheel scrolls the page.
        addWheelTarget(mRoot, x + 12, pageY, w - 24, pageH, [this](int steps) {
            mScroll = std::clamp(mScroll - steps * sWheelScrollStep, 0.f, static_cast<float>(mScrollMax));
            place();
        });

        int hx = x + 20;
        const int hintY = bottom - 42;
        const auto hint = [&](SDL_GameControllerButton button, const std::string& label) {
            addGlyph(mRoot, hx, hintY + 4, 22, button);
            MyGUI::TextBox* label_ = addText(mRoot, hx + 30, hintY, 200, 30, label, sFontNormal, sBrightColour);
            const int hintW = 30 + label_->getTextSize().width;
            // Clicking a button's hint presses it. The stick's hint only says that it scrolls, so it isn't one.
            if (button != SDL_CONTROLLER_BUTTON_RIGHTSTICK)
                addPointerTarget(mRoot, hx, hintY, hintW, 30, nullptr, [button] { pressButton(button); });
            hx += hintW + 22;
        };
        if (canTake)
            hint(SDL_CONTROLLER_BUTTON_A, "#{Interface:Take}");

        // A scroll bar along the page's right edge, when the text is longer than the page.
        if (mPaper != nullptr && mScrollMax > 0)
        {
            hint(SDL_CONTROLLER_BUTTON_RIGHTSTICK, "#{Interface:InventoryScroll}");
            mTrack = MyGUI::IntCoord(x + w - 12 - sPadding / 2 - 2, pageY + sPadding / 2, 4, pageH - sPadding);
            addRect(mRoot, mTrack.left, mTrack.top, mTrack.width, mTrack.height, MyGUI::Colour::Black, 0.12f);
            const float visible = static_cast<float>(textArea->getHeight()) / mPaper->getHeight();
            mScrollThumb = addRect(mRoot, mTrack.left, mTrack.top, mTrack.width,
                std::max(16, static_cast<int>(mTrack.height * visible)), MyGUI::Colour::Black, 0.45f);
        }
        place();
    }

    void ReadingPanel::hide()
    {
        if (mRoot != nullptr)
            mRoot->setVisible(false);
    }

    void ReadingPanel::place()
    {
        if (mPaper == nullptr)
            return;
        const int offset = static_cast<int>(mScroll);
        mPaper->setPosition(0, -offset);
        if (mScrollThumb != nullptr && mScrollMax > 0)
        {
            const int travel = mTrack.height - mScrollThumb->getHeight();
            mScrollThumb->setPosition(mTrack.left, mTrack.top + travel * offset / mScrollMax);
        }
    }

    void ReadingPanel::onFrame(float duration)
    {
        if (mRoot == nullptr || !mRoot->getVisible() || mScrollMax <= 0)
            return;
        // Either stick scrolls, and so does the d-pad.
        const MWBase::InputManager* input = MWBase::Environment::get().getInputManager();
        float stick = input->getControllerAxisValue(SDL_CONTROLLER_AXIS_RIGHTY);
        const float leftStick = input->getControllerAxisValue(SDL_CONTROLLER_AXIS_LEFTY);
        if (std::abs(leftStick) > std::abs(stick))
            stick = leftStick;
        if (input->isControllerButtonPressed(SDL_CONTROLLER_BUTTON_DPAD_UP))
            stick = -0.8f;
        else if (input->isControllerButtonPressed(SDL_CONTROLLER_BUTTON_DPAD_DOWN))
            stick = 0.8f;
        // Squaring the deflection gives finer control near the centre of the stick.
        const float magnitude = std::abs(stick);
        if (magnitude < sStickDeadzone)
            return;
        const float scaled = (magnitude - sStickDeadzone) / (1.f - sStickDeadzone);
        mScroll = std::clamp(mScroll + std::copysign(scaled * scaled, stick) * sScrollSpeed * duration, 0.f,
            static_cast<float>(mScrollMax));
        place();
    }
}
