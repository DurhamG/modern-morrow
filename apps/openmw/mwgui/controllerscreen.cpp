#include "controllerscreen.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <memory>
#include <optional>
#include <sstream>

#include <MyGUI_Gui.h>
#include <MyGUI_ILayer.h>
#include <MyGUI_ImageBox.h>
#include <MyGUI_InputManager.h>
#include <MyGUI_LanguageManager.h>
#include <MyGUI_LayerManager.h>
#include <MyGUI_TextIterator.h>

#include <components/misc/resourcehelpers.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwworld/ptr.hpp"

#include "itemwidget.hpp"

namespace MWGui
{
    namespace
    {
        // What the mouse asked for since the last frame, run at the start of the next one.
        std::vector<std::function<void()>> sPointerActions;
        // The target the mouse last pointed at.
        MyGUI::Widget* sPointedAt = nullptr;
        // Where the mouse last moved to, and where it was when the wheel last turned.
        std::optional<MyGUI::IntPoint> sMovedTo;
        MyGUI::IntPoint sWheelPoint;

        // What a pointer target does, kept in its widget's user data.
        struct PointerTarget
        {
            std::function<void()> mOnPoint;
            std::function<void()> mOnClick;
            std::function<void(int)> mOnWheel;
        };

        // The pointer target at a point. The targets are found here rather than through MyGUI's mouse focus, because
        // MyGUI keeps the mouse within a modal window, and the controller menus draw a modal window's panel apart
        // from the window.
        const PointerTarget* findPointerTarget(int x, int y, MyGUI::Widget*& widget)
        {
            widget = MyGUI::LayerManager::getInstance().getWidgetFromPoint(x, y);
            for (; widget != nullptr; widget = widget->getParent())
            {
                if (const PointerTarget* target = widget->getUserData<PointerTarget>(false))
                    return target;
                if (widget->getNeedMouseFocus())
                    return nullptr;
            }
            return nullptr;
        }

        // A target the wheel scrolls at a point, inside a widget, whatever else is over it there, such as a link on a
        // page the wheel scrolls.
        const PointerTarget* findWheelTarget(MyGUI::Widget* widget, const MyGUI::IntPoint& point)
        {
            for (std::size_t i = 0; i < widget->getChildCount(); ++i)
            {
                MyGUI::Widget* child = widget->getChildAt(i);
                if (!child->getVisible())
                    continue;
                if (const PointerTarget* found = findWheelTarget(child, point))
                    return found;
                const PointerTarget* target = child->getUserData<PointerTarget>(false);
                if (target != nullptr && target->mOnWheel && child->getAbsoluteCoord().inside(point))
                    return target;
            }
            return nullptr;
        }

        // A panel on the layer of modal panels keeps the mouse from the screens under it, as its window would. It
        // gets a target with no actions, under its own, that covers the screen.
        void blockPointerUnder(MyGUI::Widget* parent)
        {
            MyGUI::Widget* root = parent;
            while (root->getParent() != nullptr)
                root = root->getParent();
            if (root->getLayer() == nullptr || root->getLayer()->getName() != "ControllerModal")
                return;
            constexpr std::string_view blockerName = "PointerBlocker";
            for (std::size_t i = 0; i < root->getChildCount(); ++i)
                if (root->getChildAt(i)->getName() == blockerName)
                    return;
            MyGUI::Widget* blocker = root->createWidget<MyGUI::Widget>("",
                MyGUI::IntCoord(MyGUI::IntPoint(), root->getSize()), MyGUI::Align::Default, std::string(blockerName));
            blocker->setUserData(PointerTarget{});
        }

        // Lets the mouse reach the widgets a parent holds, up to the first widget that takes the mouse itself. The
        // panels' roots and containers ignore the mouse, and by default that hides their children from it too.
        void passPointerThrough(MyGUI::Widget* widget)
        {
            for (; widget != nullptr && !widget->getNeedMouseFocus(); widget = widget->getParent())
                widget->setInheritsPick(true);
        }

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

        void drawProgressPanel(
            MyGUI::Widget* root, const std::string& label, float fraction, ProgressPlacement placement)
        {
            clearPanel(root);
            const MyGUI::IntSize size = root->getSize();
            const bool corner = placement == ProgressPlacement::BottomRight;
            const int h = corner ? 68 : 96;
            const int padding = corner ? 18 : 24;
            const int barHeight = corner ? 6 : 10;
            MyGUI::TextBox* text = addText(root, 0, 0, size.width, 30, label, sFontNormal, sBrightColour);
            const int w
                = std::clamp(text->getTextSize().width + 2 * padding, corner ? 260 : 420, size.width - 2 * sMargin);
            MyGUI::Gui::getInstance().destroyWidget(text);
            int x = (size.width - w) / 2;
            int y = (size.height - h) / 2;
            if (placement != ProgressPlacement::Middle)
                y = size.height - h - sMargin;
            if (corner)
                x = size.width - w - sMargin;
            addRect(root, x, y, w, h, MyGUI::Colour::parse("#0d0b08"));
            addRect(root, x, y, w, 1, sAccentColour, 0.6f);
            addText(root, x + padding, y + (corner ? 8 : 14), w - 2 * padding, 30, label, sFontNormal, sBrightColour);
            const int barY = y + h - padding - barHeight + (corner ? 4 : 0);
            addRect(root, x + padding, barY, w - 2 * padding, barHeight, MyGUI::Colour::Black, 0.6f);
            const int filled = static_cast<int>((w - 2 * padding) * std::clamp(fraction, 0.f, 1.f));
            if (filled > 0)
                addRect(root, x + padding, barY, filled, barHeight, sAccentColour);
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

        namespace
        {
            void addGlyphImage(MyGUI::Widget* parent, int x, int y, int size, const std::string& texture)
            {
                auto* image = parent->createWidget<MyGUI::ImageBox>(
                    "ImageBox", MyGUI::IntCoord(x, y, size, size), MyGUI::Align::Default);
                image->setImageTexture(texture);
                image->setColour(MyGUI::Colour::parse("#ccb589"));
                image->setAlpha(0.85f);
                image->setNeedMouseFocus(false);
            }
        }

        void addGlyph(MyGUI::Widget* parent, int x, int y, int size, SDL_GameControllerButton button)
        {
            addGlyphImage(
                parent, x, y, size, MWBase::Environment::get().getInputManager()->getControllerButtonIcon(button));
        }

        void addGlyph(MyGUI::Widget* parent, int x, int y, int size, SDL_GameControllerAxis trigger)
        {
            addGlyphImage(
                parent, x, y, size, MWBase::Environment::get().getInputManager()->getControllerAxisIcon(trigger));
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

        constexpr int sHintGlyphSize = 26;
        constexpr int sHintLabelX = sHintGlyphSize + 8;
        constexpr int sHintGap = 24;
        constexpr int sHintRowHeight = 40;

        // Each hint's index and width, in the rows they break into to fit the width. The labels are measured in a
        // hidden text box.
        std::vector<std::vector<std::pair<std::size_t, int>>> layoutHints(
            MyGUI::Widget* parent, int width, const std::vector<Hint>& hints)
        {
            MyGUI::TextBox* probe = addText(parent, 0, 0, width, 34, {}, sFontNormal, sBrightColour);
            probe->setVisible(false);
            std::vector<std::vector<std::pair<std::size_t, int>>> rows(1);
            int rowWidth = 0;
            for (std::size_t i = 0; i < hints.size(); ++i)
            {
                probe->setCaptionWithReplacing(hints[i].second);
                const int hintWidth = sHintLabelX + probe->getTextSize().width;
                if (!rows.back().empty() && rowWidth + sHintGap + hintWidth > width)
                {
                    rows.emplace_back();
                    rowWidth = 0;
                }
                rowWidth += (rows.back().empty() ? 0 : sHintGap) + hintWidth;
                rows.back().emplace_back(i, hintWidth);
            }
            MyGUI::Gui::getInstance().destroyWidget(probe);
            return rows;
        }

        int hintsHeight(MyGUI::Widget* parent, int width, const std::vector<Hint>& hints)
        {
            if (hints.empty())
                return 0;
            return sHintRowHeight * static_cast<int>(layoutHints(parent, width, hints).size());
        }

        int drawHints(MyGUI::Widget* parent, int x, int width, int bottom, const std::vector<Hint>& hints)
        {
            if (hints.empty())
                return bottom;
            const std::vector<std::vector<std::pair<std::size_t, int>>> rows = layoutHints(parent, width, hints);
            const int top = bottom - sHintRowHeight * static_cast<int>(rows.size());
            for (std::size_t row = 0; row < rows.size(); ++row)
            {
                const int y = top + static_cast<int>(row) * sHintRowHeight;
                int hx = x;
                for (const auto& [index, hintWidth] : rows[row])
                {
                    const auto& [button, caption] = hints[index];
                    addGlyph(parent, hx, y, sHintGlyphSize, button);
                    addText(parent, hx + sHintLabelX, y - 4, hintWidth - sHintLabelX + 8, 34, caption, sFontNormal,
                        sBrightColour);
                    // Clicking a hint presses its button. The d-pad's hints stand for two directions, so they don't.
                    if (button != SDL_CONTROLLER_BUTTON_DPAD_UP && button != SDL_CONTROLLER_BUTTON_DPAD_DOWN
                        && button != SDL_CONTROLLER_BUTTON_DPAD_LEFT && button != SDL_CONTROLLER_BUTTON_DPAD_RIGHT)
                    {
                        const SDL_GameControllerButton pressed = button;
                        addPointerTarget(
                            parent, hx, y - 4, hintWidth, 34, nullptr, [pressed] { pressButton(pressed); });
                    }
                    hx += hintWidth + sHintGap;
                }
            }
            return top;
        }

        void drawTabRow(MyGUI::Widget* parent, int width, const std::vector<std::string>& captions, int selected,
            const std::string& status)
        {
            int x = sMargin;
            addGlyph(parent, x, 59, 22, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
            addPointerTarget(parent, x, 54, 30, 32, nullptr, [] { pressButton(SDL_CONTROLLER_BUTTON_LEFTSHOULDER); });
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
                // The shoulder buttons step through the tabs, so clicking a tab steps to it.
                const int steps = tab - selected;
                if (steps != 0)
                    addPointerTarget(parent, x, 50, labelW, 40, nullptr, [steps] {
                        for (int i = 0; i < std::abs(steps); ++i)
                            pressButton(
                                steps > 0 ? SDL_CONTROLLER_BUTTON_RIGHTSHOULDER : SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
                    });
                x += labelW + 6;
            }
            addGlyph(parent, x + 4, 59, 22, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
            addPointerTarget(
                parent, x + 4, 54, 30, 32, nullptr, [] { pressButton(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER); });

            addText(parent, width - sMargin - 420, 50, 420, 40, status, sFontSmall, sDimColour,
                MyGUI::Align::Right | MyGUI::Align::VCenter);
            addRect(parent, 0, 94, width, 1, sAccentColour, 0.2f);
        }

        void addPointerTarget(MyGUI::Widget* parent, int x, int y, int w, int h, std::function<void()> onPoint,
            std::function<void()> onClick, std::function<void(int steps)> onWheel)
        {
            passPointerThrough(parent);
            blockPointerUnder(parent);
            MyGUI::Widget* area
                = parent->createWidget<MyGUI::Widget>("", MyGUI::IntCoord(x, y, w, h), MyGUI::Align::Default);
            area->setNeedMouseFocus(true);
            area->setUserData(PointerTarget{ std::move(onPoint), std::move(onClick), std::move(onWheel) });
        }

        void pointerMoved(int x, int y)
        {
            // Only a pointer that moves points at things, so a resting one doesn't fight the d-pad when the panel
            // draws again under it.
            sMovedTo = MyGUI::IntPoint(x, y);
            MyGUI::Widget* widget = nullptr;
            const PointerTarget* target = findPointerTarget(x, y, widget);
            if (widget == sPointedAt)
                return;
            sPointedAt = widget;
            if (target != nullptr && target->mOnPoint)
                sPointerActions.emplace_back(target->mOnPoint);
        }

        MyGUI::Widget* addWheelTarget(
            MyGUI::Widget* parent, int x, int y, int w, int h, std::function<void(int steps)> onWheel)
        {
            passPointerThrough(parent);
            blockPointerUnder(parent);
            MyGUI::Widget* area
                = parent->createWidget<MyGUI::Widget>("", MyGUI::IntCoord(x, y, w, h), MyGUI::Align::Default);
            area->setNeedMouseFocus(true);
            area->setUserData(PointerTarget{ nullptr, nullptr, std::move(onWheel) });
            return area;
        }

        bool pointerWheel(int x, int y, int steps)
        {
            MyGUI::Widget* widget = nullptr;
            const PointerTarget* target = findPointerTarget(x, y, widget);
            if ((target == nullptr || !target->mOnWheel) && widget != nullptr)
            {
                // Something the wheel doesn't scroll, such as a link, can be on a page that it does.
                MyGUI::Widget* root = widget;
                while (root->getParent() != nullptr)
                    root = root->getParent();
                target = findWheelTarget(root, MyGUI::IntPoint(x, y));
            }
            if (target == nullptr || !target->mOnWheel)
                return false;
            sWheelPoint = MyGUI::IntPoint(x, y);
            sPointerActions.emplace_back([onWheel = target->mOnWheel, steps] { onWheel(steps); });
            return true;
        }

        MyGUI::IntPoint pointerPoint()
        {
            return sMovedTo.value_or(sWheelPoint);
        }

        bool pointerClicked(int x, int y)
        {
            MyGUI::Widget* widget = nullptr;
            const PointerTarget* target = findPointerTarget(x, y, widget);
            if (target == nullptr)
                return false;
            if (target->mOnClick)
                sPointerActions.emplace_back(target->mOnClick);
            return true;
        }

        void pressButton(SDL_GameControllerButton button)
        {
            MWBase::Environment::get().getInputManager()->pressControllerMenuButton(button);
        }

        void runPointerActions()
        {
            std::vector<std::function<void()>> actions;
            actions.swap(sPointerActions);
            for (const std::function<void()>& action : actions)
                action();
        }
    }

    void ControllerList::reset()
    {
        mFocus = 0;
        mScroll = 0;
    }

    bool ControllerList::updateHeld(float duration, int itemCount)
    {
        int dx = 0;
        int dy = 0;
        if (!mHeld.update(duration, dx, dy) || dy == 0 || itemCount <= 0)
            return false;
        const int focus = std::clamp(mFocus + dy, 0, itemCount - 1);
        if (focus == mFocus)
            return false;
        mFocus = focus;
        return true;
    }

    bool ControllerList::draw(MyGUI::Widget* parent, int x, int y, int w, int h, const std::vector<Row>& rows,
        const std::function<void(std::size_t row, int y, bool focused)>& drawRow, std::function<void()> onChange,
        std::function<void()> onActivate)
    {
        int itemCount = 0;
        for (const Row& row : rows)
            itemCount += row.mItem ? 1 : 0;
        if (itemCount == 0)
            return false;
        mFocus = std::clamp(mFocus, 0, itemCount - 1);

        // The row of the focused item.
        int focusRow = 0;
        for (int i = 0, item = 0; i < static_cast<int>(rows.size()); ++i)
        {
            if (!rows[i].mItem)
                continue;
            if (item++ == mFocus)
            {
                focusRow = i;
                break;
            }
        }

        // Scroll so the focused row is in view. Scrolling up also shows the focused item's group header.
        mScroll = std::clamp(mScroll, 0, static_cast<int>(rows.size()) - 1);
        if (focusRow < mScroll)
            mScroll = focusRow > 0 && !rows[focusRow - 1].mItem ? focusRow - 1 : focusRow;
        while (mScroll < focusRow)
        {
            int bottomOfFocus = y;
            for (int i = mScroll; i <= focusRow; ++i)
                bottomOfFocus += rows[i].mHeight;
            if (bottomOfFocus <= y + h)
                break;
            ++mScroll;
        }
        // The list doesn't scroll past its end, which would leave empty space below it, as when it gets shorter.
        int maxScroll = static_cast<int>(rows.size()) - 1;
        for (int below = rows.back().mHeight; maxScroll > 0 && below + rows[maxScroll - 1].mHeight <= h;)
            below += rows[--maxScroll].mHeight;
        mScroll = std::min(mScroll, maxScroll);

        // The wheel scrolls the list a row at a time, and the item under the mouse then takes the focus, so moving the
        // mouse afterwards doesn't pull the focus back. It goes behind the rows, so a header or the space below the
        // last row takes the wheel too.
        std::function<void(int)> onWheel;
        if (onChange)
        {
            // The mouse's position is in screen pixels, and the layer converts it to the units the rows are drawn in.
            MyGUI::Widget* root = parent;
            while (root->getParent() != nullptr)
                root = root->getParent();
            onWheel = [this, shared = std::make_shared<const std::vector<Row>>(rows), h, maxScroll, onChange,
                          layer = root->getLayer(), top = parent->getAbsoluteTop() + y](int steps) {
                const std::vector<Row>& all = *shared;
                const int scroll = std::clamp(mScroll - steps, 0, maxScroll);
                if (scroll == mScroll)
                    return;
                // The first item that ends below the mouse, among the rows that fit, so a header gives the item under
                // it. Below the last row, the last item.
                const MyGUI::IntPoint point = ControllerUi::pointerPoint();
                const int pointY = (layer != nullptr ? layer->getPosition(point.left, point.top).top : point.top) - top;
                int focusRow = -1;
                int lastItemRow = -1;
                int rowTop = 0;
                for (int i = scroll; i < static_cast<int>(all.size()) && rowTop + all[i].mHeight <= h; ++i)
                {
                    rowTop += all[i].mHeight;
                    if (!all[i].mItem)
                        continue;
                    lastItemRow = i;
                    if (focusRow < 0 && pointY < rowTop)
                        focusRow = i;
                }
                if (focusRow < 0)
                    focusRow = lastItemRow;
                if (focusRow < 0)
                    return;
                int item = 0;
                for (int i = 0; i < focusRow; ++i)
                    item += all[i].mItem ? 1 : 0;
                mScroll = scroll;
                mFocus = item;
                onChange();
            };
            ControllerUi::addWheelTarget(parent, x, y, w, h, onWheel);
        }

        int item = 0;
        for (int i = 0; i < mScroll; ++i)
            item += rows[i].mItem ? 1 : 0;
        int rowY = y;
        for (std::size_t i = static_cast<std::size_t>(mScroll); i < rows.size(); ++i)
        {
            const Row& row = rows[i];
            if (rowY + row.mHeight > y + h)
                break;
            drawRow(i, rowY, row.mItem && item == mFocus);
            if (row.mItem)
            {
                const int thisItem = item;
                ControllerUi::addPointerTarget(
                    parent, x, rowY, w, row.mHeight,
                    [this, thisItem, onChange] {
                        if (mFocus == thisItem)
                            return;
                        mFocus = thisItem;
                        if (onChange)
                            onChange();
                    },
                    [this, thisItem, onChange, onActivate] {
                        // The screen first catches up with the clicked item, so what A does acts on it.
                        const bool changed = mFocus != thisItem;
                        mFocus = thisItem;
                        if (changed && onChange)
                            onChange();
                        if (onActivate)
                            onActivate();
                    },
                    onWheel);
                ++item;
            }
            rowY += row.mHeight;
        }
        return true;
    }
}
