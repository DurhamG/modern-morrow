#include "controllerdialog.hpp"

#include <algorithm>

#include <MyGUI_Gui.h>
#include <MyGUI_ILayer.h>
#include <MyGUI_ImageBox.h>
#include <MyGUI_LayerManager.h>

#include <components/esm/refid.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"

#include "controllerscreen.hpp"

namespace MWGui
{
    namespace
    {
        using namespace ControllerUi;

        constexpr int sPanelWidth = 760;
        constexpr int sAnswerHeight = 46;
        constexpr std::size_t sMaxMessageLines = 14;

        const MyGUI::Colour sPanelBackColour = MyGUI::Colour::parse("#0d0b08");
    }

    ControllerDialog::~ControllerDialog()
    {
        if (mRoot != nullptr)
            MyGUI::Gui::getInstance().destroyWidget(mRoot);
    }

    void ControllerDialog::show(const std::string& message, std::vector<std::string> answers, std::size_t focus,
        std::optional<std::size_t> cancel)
    {
        if (mRoot == nullptr)
        {
            MyGUI::ILayer* layer = MyGUI::LayerManager::getInstance().getByName("ControllerModal", false);
            if (layer == nullptr)
                return;
            mRoot = MyGUI::Gui::getInstance().createWidget<MyGUI::Widget>(
                "", MyGUI::IntCoord(MyGUI::IntPoint(), layer->getSize()), MyGUI::Align::Default, "ControllerModal");
        }
        mMessage = message;
        mAnswers = std::move(answers);
        mFocus = std::min(focus, mAnswers.empty() ? 0 : mAnswers.size() - 1);
        mCancel = cancel;
        mRoot->setSize(mRoot->getLayer()->getSize());
        mRoot->setVisible(true);
        MyGUI::LayerManager::getInstance().upLayerItem(mRoot);
        draw();
    }

    void ControllerDialog::hide()
    {
        if (mRoot != nullptr)
            mRoot->setVisible(false);
    }

    void ControllerDialog::draw()
    {
        while (mRoot->getChildCount() > 0)
            MyGUI::Gui::getInstance().destroyWidget(mRoot->getChildAt(0));
        const MyGUI::IntSize size = mRoot->getSize();
        // The darkened backdrop also keeps the mouse from reaching what is behind the dialog.
        addRect(mRoot, 0, 0, size.width, size.height, MyGUI::Colour::Black, 0.55f)->setNeedMouseFocus(true);

        const int w = std::min(sPanelWidth, size.width - 2 * sMargin);
        const int innerW = w - 48;
        const std::vector<std::string> lines = mMessage.empty()
            ? std::vector<std::string>()
            : wrapText(mRoot, localised(mMessage), innerW, sFontNormal, sMaxMessageLines);
        constexpr int lineHeight = sFontNormal + 8;
        const int messageH = static_cast<int>(lines.size()) * lineHeight;
        // Long answers, such as those to the questions that work out a class, wrap onto more lines.
        std::vector<std::vector<std::string>> answerLines;
        int answersH = 0;
        for (const std::string& answer : mAnswers)
        {
            answerLines.push_back(wrapText(mRoot, localised(answer), w - 64, sFontNormal, 3));
            answersH += sAnswerHeight + (static_cast<int>(answerLines.back().size()) - 1) * lineHeight;
        }
        const int h = 24 + messageH + (lines.empty() ? 0 : 18) + answersH + 18 + 40;
        const int x = (size.width - w) / 2;
        const int y = std::max(sMargin, (size.height - h) / 2);
        addRect(mRoot, x, y, w, h, sPanelBackColour);
        addRect(mRoot, x, y, w, 1, sAccentColour, 0.6f);

        int ty = y + 24;
        for (const std::string& line : lines)
        {
            addText(mRoot, x + 24, ty, innerW, lineHeight, escaped(line), sFontNormal, sBrightColour);
            ty += lineHeight;
        }
        if (!lines.empty())
        {
            ty += 8;
            addRect(mRoot, x + 24, ty, innerW, 1, sAccentColour, 0.2f);
            ty += 10;
        }

        for (std::size_t i = 0; i < mAnswers.size(); ++i)
        {
            const bool focused = i == mFocus;
            const int rowH = sAnswerHeight + (static_cast<int>(answerLines[i].size()) - 1) * lineHeight;
            if (focused)
            {
                addRect(mRoot, x + 16, ty, w - 32, rowH - 4, sAccentColour, 0.16f);
                addRect(mRoot, x + 16, ty, 3, rowH - 4, sAccentColour);
            }
            int lineY = ty + (sAnswerHeight - 4 - lineHeight) / 2;
            for (const std::string& line : answerLines[i])
            {
                addText(mRoot, x + 32, lineY, w - 64, lineHeight, escaped(line), sFontNormal,
                    focused ? sActiveColour : sTextColour);
                lineY += lineHeight;
            }
            // Pointing at an answer highlights it, and clicking it picks it, as A does.
            addPointerTarget(
                mRoot, x + 16, ty, w - 32, rowH - 4,
                [this, i] {
                    if (mFocus == i)
                        return;
                    mFocus = i;
                    draw();
                },
                [this, i] {
                    mFocus = i;
                    draw();
                    pressButton(SDL_CONTROLLER_BUTTON_A);
                });
            ty += rowH;
        }

        // The button hints along the bottom.
        int hx = x + 24;
        const int hintY = y + h - 40;
        const auto hint = [&](SDL_GameControllerButton button, const std::string& label) {
            addGlyph(mRoot, hx, hintY + 4, 22, button);
            MyGUI::TextBox* text = addText(mRoot, hx + 30, hintY, 200, 30, label, sFontNormal, sBrightColour);
            // Clicking a hint presses its button.
            addPointerTarget(
                mRoot, hx, hintY, 30 + text->getTextSize().width, 30, nullptr, [button] { pressButton(button); });
            hx += 30 + text->getTextSize().width + 22;
        };
        if (!mAnswers.empty())
            hint(SDL_CONTROLLER_BUTTON_A, "#{Interface:Select}");
    }

    std::optional<std::size_t> ControllerDialog::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
    {
        if (mRoot == nullptr || !mRoot->getVisible() || mAnswers.empty())
            return std::nullopt;
        switch (arg.button)
        {
            case SDL_CONTROLLER_BUTTON_A:
                return mFocus;
            case SDL_CONTROLLER_BUTTON_B:
                return mCancel;
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            {
                const std::size_t next = arg.button == SDL_CONTROLLER_BUTTON_DPAD_UP
                    ? (mFocus > 0 ? mFocus - 1 : 0)
                    : std::min(mFocus + 1, mAnswers.size() - 1);
                if (next != mFocus)
                {
                    mFocus = next;
                    MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
                    draw();
                }
                break;
            }
            default:
                break;
        }
        return std::nullopt;
    }
}
