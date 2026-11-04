#include "controllerscreen.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>

#include <MyGUI_Gui.h>
#include <MyGUI_ILayer.h>
#include <MyGUI_ImageBox.h>
#include <MyGUI_LanguageManager.h>
#include <MyGUI_LayerManager.h>
#include <MyGUI_TextIterator.h>

#include <components/misc/resourcehelpers.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/settings/values.hpp>

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
        void hideWindow(MyGUI::Widget* mainWidget)
        {
            mainWidget->setAlpha(0.f);
            if (MyGUI::Widget* client = mainWidget->getClientWidget())
                client->setVisible(false);
        }

        MyGUI::Widget* createPanelRoot(const std::string& layer)
        {
            if (!Settings::gui().mControllerMenus)
                return nullptr;
            MyGUI::ILayer* found = MyGUI::LayerManager::getInstance().getByName(layer, false);
            if (found == nullptr)
                return nullptr;
            MyGUI::Widget* root = MyGUI::Gui::getInstance().createWidget<MyGUI::Widget>(
                "", MyGUI::IntCoord(MyGUI::IntPoint(), found->getSize()), MyGUI::Align::Default, layer);
            root->setNeedMouseFocus(false);
            root->setVisible(false);
            return root;
        }

        void clearPanel(MyGUI::Widget* root)
        {
            root->setSize(root->getLayer()->getSize());
            while (root->getChildCount() > 0)
                MyGUI::Gui::getInstance().destroyWidget(root->getChildAt(0));
        }

        void drawProgressPanel(MyGUI::Widget* root, const std::string& label, float fraction, bool atBottom)
        {
            clearPanel(root);
            const MyGUI::IntSize size = root->getSize();
            constexpr int h = 96;
            MyGUI::TextBox* text = addText(root, 0, 0, size.width, 30, label, sFontNormal, sBrightColour);
            const int w = std::clamp(text->getTextSize().width + 48, 420, size.width - 2 * sMargin);
            const int x = (size.width - w) / 2;
            const int y = atBottom ? size.height - h - sMargin : (size.height - h) / 2;
            MyGUI::Gui::getInstance().destroyWidget(text);
            addRect(root, x, y, w, h, MyGUI::Colour::parse("#0d0b08"));
            addRect(root, x, y, w, 1, sAccentColour, 0.6f);
            addText(root, x + 24, y + 14, w - 48, 30, label, sFontNormal, sBrightColour);
            addRect(root, x + 24, y + 58, w - 48, 10, MyGUI::Colour::Black, 0.6f);
            const int filled = static_cast<int>((w - 48) * std::clamp(fraction, 0.f, 1.f));
            if (filled > 0)
                addRect(root, x + 24, y + 58, filled, 10, sAccentColour);
        }

        std::string escaped(std::string_view text)
        {
            return MyGUI::TextIterator::toTagsString(MyGUI::UString(std::string(text))).asUTF8();
        }

        std::string localised(const std::string& text)
        {
            return MyGUI::LanguageManager::getInstance().replaceTags(MyGUI::UString(text)).asUTF8();
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

        std::vector<std::string> wrapText(
            MyGUI::Widget* parent, const std::string& text, int width, int fontHeight, std::size_t maxLines)
        {
            MyGUI::TextBox* probe = addText(parent, 0, 0, width, fontHeight * 2, {}, fontHeight, sTextColour);
            probe->setVisible(false);
            std::vector<std::string> lines(1);
            std::istringstream words(text);
            std::string word;
            while (words >> word)
            {
                std::string& line = lines.back();
                const std::string longer = line.empty() ? word : line + " " + word;
                probe->setCaption(MyGUI::UString(longer));
                if (!line.empty() && probe->getTextSize().width > width && lines.size() < maxLines)
                    lines.emplace_back(word);
                else
                    line = longer;
            }
            MyGUI::Gui::getInstance().destroyWidget(probe);
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
