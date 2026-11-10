#include "textinput.hpp"

#include <algorithm>
#include <cctype>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"

#include <MyGUI_Button.h>
#include <MyGUI_EditBox.h>
#include <MyGUI_Gui.h>
#include <MyGUI_ImageBox.h>
#include <MyGUI_LayerManager.h>
#include <MyGUI_UString.h>

#include <components/esm/refid.hpp>

namespace MWGui
{
    namespace
    {
        // The on-screen keyboard's keys, row by row. The last row holds the keys that aren't characters.
        const std::vector<std::vector<std::string>> sKeyRows = {
            { "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M" },
            { "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z" },
            { "'", "-", ".", ",", "1", "2", "3", "4", "5", "6", "7", "8", "9" },
        };
        enum SpecialKey
        {
            Key_Space,
            Key_Delete,
            Key_Done,
            Key_Count,
        };
        constexpr int sKeySize = 46;
        constexpr int sKeyGap = 6;
    }

    TextInputDialog::TextInputDialog()
        : WindowModal("openmw_text_input.layout")
    {
        // Centre dialog
        center();

        getWidget(mTextEdit, "TextEdit");
        mTextEdit->eventEditSelectAccept += newDelegate(this, &TextInputDialog::onTextAccepted);

        MyGUI::Button* okButton;
        getWidget(okButton, "OKButton");
        okButton->eventMouseButtonClick += MyGUI::newDelegate(this, &TextInputDialog::onOkClicked);

        // Make sure the edit box has focus
        MWBase::Environment::get().getWindowManager()->setKeyFocusWidget(mTextEdit);

        mControllerButtons.mA = "#{Interface:OK}";

        mPanel = ControllerUi::createPanelRoot("ControllerModal");
        if (mPanel != nullptr)
            mTextEdit->eventEditTextChange += MyGUI::newDelegate(this, &TextInputDialog::onTextChanged);
    }

    TextInputDialog::~TextInputDialog()
    {
        if (mPanel != nullptr)
            MyGUI::Gui::getInstance().destroyWidget(mPanel);
    }

    void TextInputDialog::onClose()
    {
        if (mPanel != nullptr)
            mPanel->setVisible(false);
        WindowModal::onClose();
    }

    void TextInputDialog::onTextChanged(MyGUI::EditBox* /*sender*/)
    {
        drawPanel();
    }

    void TextInputDialog::onFrame(float duration)
    {
        int dx = 0;
        int dy = 0;
        if (mPanel != nullptr && mPanel->getVisible() && mHeldDirection.update(duration, dx, dy))
            moveKey(dx, dy);
    }

    void TextInputDialog::moveKey(int dx, int dy)
    {
        // The last row has fewer, wider keys, so moving into or out of it keeps roughly the same place across.
        const int rows = static_cast<int>(sKeyRows.size()) + 1;
        const auto rowLength = [&](int row) {
            return row < static_cast<int>(sKeyRows.size()) ? static_cast<int>(sKeyRows[row].size())
                                                           : static_cast<int>(Key_Count);
        };
        if (dy != 0)
        {
            const int newRow = (mKeyRow + dy + rows) % rows;
            const float across = (mKeyColumn + 0.5f) / rowLength(mKeyRow);
            mKeyColumn = std::min(static_cast<int>(across * rowLength(newRow)), rowLength(newRow) - 1);
            mKeyRow = newRow;
        }
        if (dx != 0)
            mKeyColumn = (mKeyColumn + dx + rowLength(mKeyRow)) % rowLength(mKeyRow);
        MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
        drawPanel();
    }

    void TextInputDialog::typeText(const std::string& text)
    {
        mTextEdit->setCaption(mTextEdit->getCaption() + MyGUI::UString(text));
        drawPanel();
    }

    void TextInputDialog::deleteLast()
    {
        MyGUI::UString text = mTextEdit->getCaption();
        if (!text.empty())
        {
            text.erase(text.size() - 1);
            mTextEdit->setCaption(text);
        }
        drawPanel();
    }

    void TextInputDialog::pressKey()
    {
        if (mKeyRow < static_cast<int>(sKeyRows.size()))
        {
            // Letters start in upper case at the start of a word, and in lower case after that. Y swaps the case of
            // the next letter.
            const std::string text = mTextEdit->getCaption().asUTF8();
            bool upper = text.empty() || text.back() == ' ' || text.back() == '-';
            if (mShift)
                upper = !upper;
            mShift = false;
            std::string key = sKeyRows[mKeyRow][mKeyColumn];
            if (!upper)
                std::transform(
                    key.begin(), key.end(), key.begin(), [](char c) { return static_cast<char>(std::tolower(c)); });
            typeText(key);
            return;
        }
        switch (mKeyColumn)
        {
            case Key_Space:
                typeText(" ");
                break;
            case Key_Delete:
                deleteLast();
                break;
            default:
                onOkClicked(nullptr);
                break;
        }
    }

    void TextInputDialog::drawPanel()
    {
        using namespace ControllerUi;
        if (mPanel == nullptr || !mPanel->getVisible())
            return;
        clearPanel(mPanel);
        const MyGUI::IntSize size = mPanel->getSize();
        addRect(mPanel, 0, 0, size.width, size.height, MyGUI::Colour::Black, 0.6f)->setNeedMouseFocus(true);
        const int columns = static_cast<int>(sKeyRows.front().size());
        const int keysW = columns * sKeySize + (columns - 1) * sKeyGap;
        const int w = keysW + 64;
        const int h = 400;
        const int x = (size.width - w) / 2;
        const int y = (size.height - h) / 2;
        addRect(mPanel, x, y, w, h, MyGUI::Colour::parse("#0d0b08"));
        addRect(mPanel, x, y, w, 1, sAccentColour, 0.6f);

        // What the text is for, such as "Name", and the text so far with a cursor after it.
        addText(mPanel, x + 32, y + 16, keysW, 34, mLabel, sFontTitle, sBrightColour);
        addRect(mPanel, x + 32, y + 58, keysW, 48, MyGUI::Colour::Black, 0.6f);
        addText(mPanel, x + 44, y + 58, keysW - 24, 48, escaped(mTextEdit->getCaption().asUTF8()) + "_", sFontTitle,
            sActiveColour);

        // The keys.
        int ky = y + 126;
        for (int row = 0; row <= static_cast<int>(sKeyRows.size()); ++row)
        {
            const bool special = row == static_cast<int>(sKeyRows.size());
            const int count = special ? static_cast<int>(Key_Count) : static_cast<int>(sKeyRows[row].size());
            const int keyW = special ? (keysW - (count - 1) * sKeyGap) / count : sKeySize;
            for (int column = 0; column < count; ++column)
            {
                const int kx = x + 32 + column * (keyW + sKeyGap);
                const bool focused = row == mKeyRow && column == mKeyColumn;
                addRect(mPanel, kx, ky, keyW, sKeySize, sPanelColour, 0.9f);
                if (focused)
                {
                    addRect(mPanel, kx, ky, keyW, sKeySize, sAccentColour, 0.35f);
                    addRect(mPanel, kx, ky + sKeySize - 3, keyW, 3, sAccentColour);
                }
                std::string caption;
                if (!special)
                    caption = sKeyRows[row][column];
                else if (column == Key_Space)
                    caption = "#{Interface:KeyboardSpace}";
                else if (column == Key_Delete)
                    caption = "#{Interface:KeyboardDelete}";
                else
                    caption = "#{Interface:KeyboardDone}";
                addText(mPanel, kx, ky, keyW, sKeySize, caption, special ? sFontNormal : sFontLarge,
                    focused ? sActiveColour : sBrightColour, MyGUI::Align::Center);
                // Pointing at a key focuses it, and clicking it types it, as A does.
                addPointerTarget(
                    mPanel, kx, ky, keyW, sKeySize,
                    [this, row, column] {
                        if (mKeyRow == row && mKeyColumn == column)
                            return;
                        mKeyRow = row;
                        mKeyColumn = column;
                        drawPanel();
                    },
                    [this, row, column] {
                        mKeyRow = row;
                        mKeyColumn = column;
                        drawPanel();
                        pressButton(SDL_CONTROLLER_BUTTON_A);
                    });
            }
            ky += sKeySize + sKeyGap;
        }

        // The button hints along the bottom.
        int hx = x + 32;
        const int hintY = y + h - 44;
        const auto hint = [&](SDL_GameControllerButton button, const std::string& caption) {
            addGlyph(mPanel, hx, hintY + 4, 22, button);
            MyGUI::TextBox* text = addText(mPanel, hx + 30, hintY, 200, 30, caption, sFontNormal, sBrightColour);
            const int hintW = 30 + text->getTextSize().width;
            // Clicking a hint presses its button.
            addPointerTarget(mPanel, hx, hintY, hintW, 30, nullptr, [button] { pressButton(button); });
            hx += hintW + 22;
        };
        hint(SDL_CONTROLLER_BUTTON_A, "#{Interface:KeyboardType}");
        hint(SDL_CONTROLLER_BUTTON_B, "#{Interface:KeyboardDelete}");
        hint(SDL_CONTROLLER_BUTTON_X, "#{Interface:KeyboardSpace}");
        hint(SDL_CONTROLLER_BUTTON_Y, mShift ? "#{Interface:KeyboardShiftOn}" : "#{Interface:KeyboardShift}");
        hint(SDL_CONTROLLER_BUTTON_START, "#{Interface:KeyboardDone}");
    }

    void TextInputDialog::setNextButtonShow(bool shown)
    {
        MyGUI::Button* okButton;
        getWidget(okButton, "OKButton");

        if (shown)
            okButton->setCaption(
                MyGUI::UString(MWBase::Environment::get().getWindowManager()->getGameSettingString("sNext", {})));
        else
            okButton->setCaption(
                MyGUI::UString(MWBase::Environment::get().getWindowManager()->getGameSettingString("sOK", {})));
    }

    void TextInputDialog::setTextLabel(std::string_view label)
    {
        setText("LabelT", label);
        mLabel = std::string(label);
    }

    void TextInputDialog::onOpen()
    {
        WindowModal::onOpen();
        // Make sure the edit box has focus
        MWBase::Environment::get().getWindowManager()->setKeyFocusWidget(mTextEdit);

        if (mPanel != nullptr)
        {
            ControllerUi::hideWindow(mMainWidget);
            mKeyRow = 0;
            mKeyColumn = 0;
            mShift = false;
            mHeldDirection.reset();
            mPanel->setVisible(true);
            MyGUI::LayerManager::getInstance().upLayerItem(mPanel);
            drawPanel();
        }
    }

    // widget controls

    void TextInputDialog::onOkClicked(MyGUI::Widget* /*sender*/)
    {
        if (mTextEdit->getCaption().empty())
        {
            MWBase::Environment::get().getWindowManager()->messageBox("#{sNotifyMessage37}");
            MWBase::Environment::get().getWindowManager()->setKeyFocusWidget(mTextEdit);
        }
        else
            eventDone(this);
    }

    void TextInputDialog::onTextAccepted(MyGUI::EditBox* sender)
    {
        onOkClicked(sender);

        // To do not spam onTextAccepted() again and again
        MWBase::Environment::get().getWindowManager()->injectKeyRelease(MyGUI::KeyCode::None);
    }

    std::string TextInputDialog::getTextInput() const
    {
        return mTextEdit->getCaption();
    }

    void TextInputDialog::setTextInput(const std::string& text)
    {
        mTextEdit->setCaption(text);
        drawPanel();
    }

    bool TextInputDialog::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
    {
        if (mPanel != nullptr)
        {
            MWBase::WindowManager* winMgr = MWBase::Environment::get().getWindowManager();
            switch (arg.button)
            {
                case SDL_CONTROLLER_BUTTON_A:
                    pressKey();
                    winMgr->playSound(ESM::RefId::stringRefId("Menu Click"));
                    break;
                case SDL_CONTROLLER_BUTTON_B:
                    deleteLast();
                    break;
                case SDL_CONTROLLER_BUTTON_X:
                    typeText(" ");
                    break;
                case SDL_CONTROLLER_BUTTON_Y:
                    mShift = !mShift;
                    drawPanel();
                    break;
                case SDL_CONTROLLER_BUTTON_START:
                    onOkClicked(nullptr);
                    break;
                default:
                    // The d-pad is read every frame in onFrame, so it can repeat while held.
                    break;
            }
            return true;
        }

        if (arg.button == SDL_CONTROLLER_BUTTON_A)
        {
            onOkClicked(nullptr);
            MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
            return true;
        }

        return false;
    }
}
