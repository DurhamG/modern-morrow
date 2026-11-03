#include "controllerscreen.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>

#include <MyGUI_Gui.h>
#include <MyGUI_ILayer.h>
#include <MyGUI_ImageBox.h>
#include <MyGUI_TextIterator.h>

#include <components/misc/resourcehelpers.hpp>
#include <components/resource/resourcesystem.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"

#include "../mwworld/ptr.hpp"

#include "itemwidget.hpp"

namespace MWGui
{
    namespace
    {
        constexpr float sRepeatDelay = 0.35f;
        constexpr float sRepeatInterval = 0.08f;
        constexpr float sStickThreshold = 0.5f;
    }

    ControllerScreen::ControllerScreen()
        : WindowBase("openmw_controller_screen.layout")
    {
        mDisableGamepadCursor = true;
    }

    void ControllerScreen::onResChange(int /*width*/, int /*height*/)
    {
        // The layer scales its contents, so the screen fills the layer's own size rather than the screen's.
        mMainWidget->setCoord(MyGUI::IntCoord(MyGUI::IntPoint(), mMainWidget->getLayer()->getSize()));
        if (isVisible())
            redraw();
    }

    void ControllerScreen::setActiveControllerWindow(bool active)
    {
        // Other tabs hide this screen by moving it out of view, because the window manager only cycles through
        // visible windows.
        const MyGUI::IntSize size = mMainWidget->getLayer()->getSize();
        mMainWidget->setCoord(0, active ? 0 : size.height + 1, size.width, size.height);
        WindowBase::setActiveControllerWindow(active);
        if (active)
            redraw();
    }

    MyGUI::Widget* ControllerScreen::clearContent()
    {
        if (mContent != nullptr)
            MyGUI::Gui::getInstance().destroyWidget(mContent);
        const MyGUI::IntSize size = mMainWidget->getSize();
        mContent = mMainWidget->createWidget<MyGUI::Widget>(
            "", MyGUI::IntCoord(0, 0, size.width, size.height), MyGUI::Align::Stretch);
        mContent->setNeedMouseFocus(false);
        ControllerUi::addRect(mContent, 0, ControllerUi::sTabBarHeight, size.width,
            size.height - ControllerUi::sTabBarHeight, MyGUI::Colour::Black, 0.97f);
        return mContent;
    }

    bool HeldDirection::update(float duration, int& dx, int& dy)
    {
        const MWBase::InputManager* input = MWBase::Environment::get().getInputManager();
        const float stickX = input->getControllerAxisValue(SDL_CONTROLLER_AXIS_LEFTX);
        const float stickY = input->getControllerAxisValue(SDL_CONTROLLER_AXIS_LEFTY);
        dx = 0;
        dy = 0;
        if (input->isControllerButtonPressed(SDL_CONTROLLER_BUTTON_DPAD_LEFT) || stickX < -sStickThreshold)
            dx = -1;
        else if (input->isControllerButtonPressed(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || stickX > sStickThreshold)
            dx = 1;
        if (input->isControllerButtonPressed(SDL_CONTROLLER_BUTTON_DPAD_UP) || stickY < -sStickThreshold)
            dy = -1;
        else if (input->isControllerButtonPressed(SDL_CONTROLLER_BUTTON_DPAD_DOWN) || stickY > sStickThreshold)
            dy = 1;

        if (dx != mX || dy != mY)
        {
            mX = dx;
            mY = dy;
            mRepeatTimer = sRepeatDelay;
            return dx != 0 || dy != 0;
        }
        if (dx == 0 && dy == 0)
            return false;
        mRepeatTimer -= duration;
        if (mRepeatTimer > 0)
            return false;
        mRepeatTimer = sRepeatInterval;
        return true;
    }

    void HeldDirection::reset()
    {
        mX = 0;
        mY = 0;
    }

    namespace ControllerUi
    {
        std::string escaped(std::string_view text)
        {
            return MyGUI::TextIterator::toTagsString(MyGUI::UString(std::string(text))).asUTF8();
        }

        std::string formatNumber(float value)
        {
            if (std::abs(value - std::round(value)) < 0.005f)
                return std::to_string(static_cast<int>(std::round(value)));
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(2) << value;
            std::string text = stream.str();
            while (!text.empty() && text.back() == '0')
                text.pop_back();
            if (!text.empty() && text.back() == '.')
                text.pop_back();
            return text;
        }

        MyGUI::ImageBox* addRect(
            MyGUI::Widget* parent, int x, int y, int w, int h, const MyGUI::Colour& colour, float alpha)
        {
            auto* box = parent->createWidget<MyGUI::ImageBox>(
                "ImageBox", MyGUI::IntCoord(x, y, w, h), MyGUI::Align::Default);
            box->setImageTexture("white");
            box->setColour(colour);
            box->setAlpha(alpha);
            box->setNeedMouseFocus(false);
            return box;
        }

        MyGUI::TextBox* addText(MyGUI::Widget* parent, int x, int y, int w, int h, std::string_view caption,
            int fontHeight, const MyGUI::Colour& colour, MyGUI::Align align)
        {
            auto* text = parent->createWidget<MyGUI::TextBox>(
                "NormalText", MyGUI::IntCoord(x, y, w, h), MyGUI::Align::Default);
            text->setFontHeight(fontHeight);
            text->setTextColour(colour);
            text->setTextAlign(align);
            text->setTextShadow(true);
            text->setTextShadowColour(MyGUI::Colour::Black);
            text->setCaptionWithReplacing(std::string(caption));
            text->setNeedMouseFocus(false);
            return text;
        }

        void addGlyph(MyGUI::Widget* parent, int x, int y, int size, SDL_GameControllerButton button)
        {
            auto* image = parent->createWidget<MyGUI::ImageBox>(
                "ImageBox", MyGUI::IntCoord(x, y, size, size), MyGUI::Align::Default);
            image->setImageTexture(MWBase::Environment::get().getInputManager()->getControllerButtonIcon(button));
            image->setColour(MyGUI::Colour::parse("#ccb589"));
            image->setAlpha(0.85f);
            image->setNeedMouseFocus(false);
        }

        void addIcon(MyGUI::Widget* parent, int x, int y, int size, const MWWorld::Ptr& ptr, bool equipped)
        {
            addRect(parent, x, y, size, size, sIconBackColour, 0.9f);
            auto* icon = parent->createWidget<ItemWidget>(
                "MW_ItemIcon", MyGUI::IntCoord(x, y, size, size), MyGUI::Align::Default);
            icon->setItem(ptr, equipped ? ItemWidget::Equip : ItemWidget::None);
            icon->setNeedMouseFocus(false);
        }

        void addImage(MyGUI::Widget* parent, int x, int y, int size, const std::string& texture)
        {
            addRect(parent, x, y, size, size, sIconBackColour, 0.9f);
            const VFS::Manager& vfs = *MWBase::Environment::get().getResourceSystem()->getVFS();
            auto* image = parent->createWidget<MyGUI::ImageBox>(
                "ImageBox", MyGUI::IntCoord(x + 2, y + 2, size - 4, size - 4), MyGUI::Align::Default);
            image->setImageTexture(Misc::ResourceHelpers::correctIconPath(VFS::Path::toNormalized(texture), vfs));
            image->setNeedMouseFocus(false);
        }

        std::vector<std::string> wrapText(const std::string& text, int width, int fontHeight)
        {
            const std::size_t perLine = std::max<std::size_t>(1, static_cast<std::size_t>(width / (fontHeight * 0.5f)));
            std::vector<std::string> lines(1);
            std::istringstream words(text);
            std::string word;
            while (words >> word)
            {
                std::string& line = lines.back();
                if (!line.empty() && line.size() + 1 + word.size() > perLine && lines.size() < 2)
                    lines.emplace_back(word);
                else
                    line += (line.empty() ? "" : " ") + word;
            }
            return lines;
        }

        void drawTabRow(MyGUI::Widget* parent, int width, const std::vector<std::string>& captions, int selected,
            const std::string& status)
        {
            int x = sMargin;
            addGlyph(parent, x, 59, 22, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
            x += 34;
            for (int tab = 0; tab < static_cast<int>(captions.size()); ++tab)
            {
                const bool isSelected = tab == selected;
                MyGUI::TextBox* label = addText(parent, x, 50, 200, 40, captions[tab], sFontNormal,
                    isSelected ? sBrightColour : sDimColour, MyGUI::Align::Left | MyGUI::Align::VCenter);
                const int labelW = label->getTextSize().width + 24;
                label->setCoord(x, 50, labelW, 40);
                label->setTextAlign(MyGUI::Align::Center);
                if (isSelected)
                    addRect(parent, x + 8, 84, labelW - 16, 2, sAccentColour);
                x += labelW + 6;
            }
            addGlyph(parent, x + 4, 59, 22, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);

            addText(parent, width - sMargin - 420, 50, 420, 40, status, sFontSmall, sDimColour,
                MyGUI::Align::Right | MyGUI::Align::VCenter);
            addRect(parent, 0, 94, width, 1, sAccentColour, 0.2f);
        }
    }
}
