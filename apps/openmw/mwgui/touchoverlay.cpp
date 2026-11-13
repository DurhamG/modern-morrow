#include "touchoverlay.hpp"

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

#include <MyGUI_Gui.h>
#include <MyGUI_LanguageManager.h>
#include <MyGUI_TextBox.h>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"

#include "controllerscreen.hpp"
#include "quickkeysmenu.hpp"

namespace MWGui
{
    namespace
    {
        using namespace ControllerUi;

        std::string tutorialText(std::string_view key)
        {
            return MyGUI::LanguageManager::getInstance().replaceTags("#{Interface:" + std::string(key) + "}");
        }

        // An arrow along a row, from one point to another, with its head at the second.
        void addArrow(MyGUI::Widget* root, int from, int to, int y, int thickness)
        {
            const int direction = to > from ? 1 : -1;
            const int head = thickness * 3;
            const int base = to - direction * 2 * head;
            addRect(root, std::min(from, base), y - thickness / 2, std::abs(base - from), thickness, sAccentColour);
            // The head is drawn in thin rows from its base, each shorter the farther it is from the middle, so the
            // middle row reaches the tip.
            for (int dy = -head; dy <= head; dy += 2)
            {
                const int length = (head - std::abs(dy)) * 2;
                if (length > 0)
                    addRect(root, direction > 0 ? base : base - length, y + dy - 1, length, 2, sAccentColour);
            }
        }

        // A zone where a gesture starts, filled lightly and outlined.
        void addZone(MyGUI::Widget* root, const MyGUI::IntCoord& zone)
        {
            addRect(root, zone.left, zone.top, zone.width, zone.height, sAccentColour, 0.25f);
            addRect(root, zone.left, zone.top, zone.width, 2, sAccentColour, 0.9f);
            addRect(root, zone.left, zone.bottom() - 2, zone.width, 2, sAccentColour, 0.9f);
            addRect(root, zone.left, zone.top, 2, zone.height, sAccentColour, 0.9f);
            addRect(root, zone.right() - 2, zone.top, 2, zone.height, sAccentColour, 0.9f);
        }

        // What a gesture does, and below it how, in lines that fit the width. Returns the label's height.
        int addLabel(MyGUI::Widget* root, int x, int y, int w, std::string_view title, std::string_view detail,
            MyGUI::Align align)
        {
            addText(root, x, y, w, 26, tutorialText(title), sFontLarge, sBrightColour, align | MyGUI::Align::Top);
            int bottom = y + 28;
            for (const std::string& line : wrapText(root, tutorialText(detail), w, sFontNormal, 4))
            {
                addText(root, x, bottom, w, 22, line, sFontNormal, sTextColour, align | MyGUI::Align::Top);
                bottom += 22;
            }
            return bottom - y;
        }

        // The label of a zone in a corner, beside the corner, with the zone's edge as its edge.
        void addCornerLabel(MyGUI::Widget* root, const MyGUI::IntCoord& zone, bool left, bool above,
            std::string_view title, std::string_view detail)
        {
            constexpr int width = 260;
            constexpr int height = 50;
            const int x = left ? zone.left + 8 : zone.right() - 8 - width;
            const int y = above ? zone.top - 8 - height : zone.bottom() + 8;
            addLabel(root, x, y, width, title, detail, left ? MyGUI::Align::Left : MyGUI::Align::Right);
        }

        void addContinue(MyGUI::Widget* root)
        {
            addText(root, 0, root->getHeight() - 56, root->getWidth(), 30, tutorialText("TouchTutorialContinue"),
                sFontLarge, sActiveColour, MyGUI::Align::Center);
        }
    }

    TouchOverlay::TouchOverlay() = default;

    TouchOverlay::~TouchOverlay()
    {
        if (mRoot != nullptr)
            MyGUI::Gui::getInstance().destroyWidget(mRoot);
    }

    MyGUI::Widget* TouchOverlay::prepare()
    {
        if (mRoot == nullptr)
            mRoot = ControllerUi::createPanelRoot("ControllerNotification");
        if (mRoot == nullptr)
            return nullptr;
        ControllerUi::clearPanel(mRoot);
        mRoot->setVisible(true);
        return mRoot;
    }

    MyGUI::IntCoord TouchOverlay::toCanvas(const MyGUI::IntCoord& pixels, const MyGUI::IntSize& screen) const
    {
        // The layer keeps a canvas 720 units tall, so a pixel is the same fraction of either.
        const float scale = static_cast<float>(mRoot->getHeight()) / std::max(1, screen.height);
        return MyGUI::IntCoord(static_cast<int>(pixels.left * scale), static_cast<int>(pixels.top * scale),
            static_cast<int>(pixels.width * scale), static_cast<int>(pixels.height * scale));
    }

    void TouchOverlay::showModes(const std::vector<MyGUI::IntCoord>& boxes, const std::vector<std::string>& captions,
        int current, int highlighted, const MyGUI::IntSize& screen)
    {
        using namespace ControllerUi;
        MyGUI::Widget* root = prepare();
        if (root == nullptr)
            return;

        for (std::size_t i = 0; i < boxes.size() && i < captions.size(); ++i)
        {
            const MyGUI::IntCoord box = toCanvas(boxes[i], screen);
            const bool lit = static_cast<int>(i) == highlighted;
            addRect(root, box.left, box.top, box.width, box.height, sPanelColour, 0.9f);
            addRect(root, box.left, box.top, box.width, 1, sAccentColour, 0.6f);
            if (lit)
                addRect(root, box.left + 2, box.top + 2, box.width - 4, box.height - 4, sAccentColour, 0.35f);
            if (static_cast<int>(i) == current)
                addRect(root, box.left + box.width / 4, box.bottom() - 6, box.width / 2, 3, sAccentColour);
            addText(root, box.left, box.top, box.width, box.height, captions[i], sFontNormal,
                lit ? sActiveColour : sTextColour, MyGUI::Align::Center);
        }
    }

    void TouchOverlay::showRadial(
        const std::vector<MyGUI::IntCoord>& slots, const MyGUI::IntCoord& centre, const MyGUI::IntSize& screen)
    {
        using namespace ControllerUi;
        MyGUI::Widget* root = prepare();
        if (root == nullptr)
            return;

        // Darkens the game behind it, since the game is paused and only the radial responds.
        addRect(root, 0, 0, root->getWidth(), root->getHeight(), MyGUI::Colour::Black, 0.45f);
        const MyGUI::IntCoord middle = toCanvas(centre, screen);
        addText(root, middle.left - 60, middle.top, middle.width + 120, middle.height, "#{Interface:TouchQuickKeys}",
            sFontSmall, sDimColour, MyGUI::Align::Center);

        QuickKeysMenu* menu = MWBase::Environment::get().getWindowManager()->getQuickKeysMenu();
        for (std::size_t i = 0; i < slots.size(); ++i)
        {
            const MyGUI::IntCoord box = toCanvas(slots[i], screen);
            const QuickKeysMenu::Slot slot = menu->getSlot(static_cast<int>(i) + 1);
            const int icon = box.width;
            if (!slot.mItem.isEmpty())
                addIcon(root, box.left, box.top, icon, slot.mItem, false);
            else if (!slot.mIcon.empty())
                addImage(root, box.left, box.top, icon, slot.mIcon);
            else
                addRect(root, box.left, box.top, icon, icon, sIconBackColour, 0.9f);
            addRect(root, box.left, box.top, icon, 1, sAccentColour, 0.5f);

            const std::string name = slot.mName.empty() ? "#{Interface:TouchQuickKeyEmpty}" : escaped(slot.mName);
            addText(root, box.left - icon / 2, box.bottom() + 2, icon * 2, 20, name, sFontSmall,
                slot.mName.empty() ? sDimColour : sBrightColour, MyGUI::Align::HCenter | MyGUI::Align::Top);
        }
    }

    void TouchOverlay::showTutorialPlay(
        const TouchTutorialZones& zones, const MyGUI::IntSize& screen, float pixelsPerMm)
    {
        MyGUI::Widget* root = prepare();
        if (root == nullptr)
            return;
        const int width = root->getWidth();
        const int height = root->getHeight();
        const float mm = pixelsPerMm * height / std::max(1, screen.height);

        // The game is paused behind it, so it shows dimmed.
        addRect(root, 0, 0, width, height, MyGUI::Colour::Black, 0.6f);
        addRect(root, width / 2 - 1, height / 5, 2, height * 3 / 5, sAccentColour, 0.35f);
        addText(root, 0, 18, width, 34, tutorialText("TouchTutorialTitle"), sFontTitle, sBrightColour,
            MyGUI::Align::Center);

        const MyGUI::IntCoord quickSave = toCanvas(zones.mQuickSave, screen);
        const MyGUI::IntCoord quickLoad = toCanvas(zones.mQuickLoad, screen);
        const MyGUI::IntCoord sneak = toCanvas(zones.mSneak, screen);
        for (const MyGUI::IntCoord& corner : { quickSave, quickLoad, sneak })
            addZone(root, corner);
        addCornerLabel(root, quickSave, true, false, "TouchTutorialQuickSave", "TouchTutorialTap");
        addCornerLabel(root, quickLoad, false, false, "TouchTutorialQuickLoad", "TouchTutorialHold");
        addCornerLabel(root, sneak, false, true, "TouchTutorialSneak", "TouchTutorialTap");

        // The edges' swipes, each with an arrow in from its band.
        const int arrow = static_cast<int>(12 * mm);
        const int thickness = std::max(3, static_cast<int>(0.8f * mm));
        const int middle = height / 2;
        const MyGUI::IntCoord leftEdge = toCanvas(zones.mLeftEdge, screen);
        addZone(root, leftEdge);
        addArrow(root, leftEdge.right() + 6, leftEdge.right() + 6 + arrow, middle, thickness);
        addLabel(root, leftEdge.right() + arrow + 20, middle - 28, width / 2 - leftEdge.right() - arrow - 40,
            "TouchTutorialInventory", "TouchTutorialSwipeIn", MyGUI::Align::Left);

        const MyGUI::IntCoord rightEdge = toCanvas(zones.mRightEdge, screen);
        addZone(root, rightEdge);
        const int tip = rightEdge.left - 6 - arrow;
        addArrow(root, rightEdge.left - 6, tip, middle, thickness);

        // A small mode picker, as it opens under the thumb: use in the middle, attack and the quick keys to its left,
        // magic above and jump below.
        const int itemW = static_cast<int>(13 * mm);
        const int itemH = static_cast<int>(6 * mm);
        const int across = itemW + 6;
        const int upDown = itemH + 6;
        const int useX = tip - 12 - itemW;
        const int useY = middle - itemH / 2;
        const std::pair<int, int> offsets[] = { { 0, 0 }, { -1, 0 }, { -2, 0 }, { 0, -1 }, { 0, 1 } };
        const char* captions[]
            = { "TouchModeUse", "TouchModeAttack", "TouchQuickKeys", "TouchModeMagic", "TouchModeJump" };
        for (std::size_t i = 0; i < std::size(offsets); ++i)
        {
            const int x = useX + offsets[i].first * across;
            const int y = useY + offsets[i].second * upDown;
            addRect(root, x, y, itemW, itemH, sPanelColour, 0.95f);
            addRect(root, x, y, itemW, 1, sAccentColour, 0.6f);
            if (i == 0)
                addRect(root, x + 2, y + 2, itemW - 4, itemH - 4, sAccentColour, 0.35f);
            addText(root, x, y, itemW, itemH, tutorialText(captions[i]), sFontSmall,
                i == 0 ? sActiveColour : sTextColour, MyGUI::Align::Center);
        }
        const int labelW = std::min(360, width / 2 - 40);
        addLabel(root, tip - labelW, useY - upDown - 8 - 78, labelW, "TouchTutorialModes", "TouchTutorialSwipeInModes",
            MyGUI::Align::Right);

        // The thumbs, below the middle of each half, clear of the edges' bands.
        const int thumbsY = height * 2 / 3;
        const int outer = leftEdge.right() + 24;
        const int halfW = width / 2 - outer - sMargin;
        addLabel(
            root, outer, thumbsY, halfW, "TouchTutorialLeftThumb", "TouchTutorialLeftThumbText", MyGUI::Align::HCenter);
        addLabel(root, width / 2 + sMargin, thumbsY, halfW, "TouchTutorialRightThumb", "TouchTutorialRightThumbText",
            MyGUI::Align::HCenter);

        addContinue(root);
    }

    void TouchOverlay::showTutorialMenus(const MyGUI::IntSize& screen, float pixelsPerMm)
    {
        MyGUI::Widget* root = prepare();
        if (root == nullptr)
            return;
        const int width = root->getWidth();
        const int height = root->getHeight();
        const float mm = pixelsPerMm * height / std::max(1, screen.height);
        const int thickness = std::max(3, static_cast<int>(0.8f * mm));

        // The inventory screens show dimmed behind it, more than the game does, since their text would get in the
        // way of the page's.
        addRect(root, 0, 0, width, height, MyGUI::Colour::Black, 0.8f);
        addText(root, 0, 112, width, 34, tutorialText("TouchTutorialMenus"), sFontTitle, sBrightColour,
            MyGUI::Align::Center);

        // Dragging sideways switches the big tabs on the left half, and the screen's own tabs on the right half.
        const int arrowsY = 200;
        const int halfW = width / 2 - 2 * sMargin;
        for (int half = 0; half < 2; ++half)
        {
            const int left = half == 0 ? sMargin : width / 2 + sMargin;
            const int centre = left + halfW / 2;
            const int reach = std::min(halfW / 2 - 20, static_cast<int>(25 * mm));
            addArrow(root, centre - 10, centre - reach, arrowsY, thickness);
            addArrow(root, centre + 10, centre + reach, arrowsY, thickness);
            addLabel(root, left, arrowsY + 22, halfW, half == 0 ? "TouchTutorialScreens" : "TouchTutorialTabs",
                half == 0 ? "TouchTutorialScreensText" : "TouchTutorialTabsText", MyGUI::Align::HCenter);
        }

        // Swiping in from either edge goes back.
        const int edge = static_cast<int>(8 * mm);
        const int arrow = static_cast<int>(12 * mm);
        const int backY = height * 3 / 5;
        const MyGUI::IntCoord leftEdge(0, height / 4, edge, height / 2);
        const MyGUI::IntCoord rightEdge(width - edge, height / 4, edge, height / 2);
        addZone(root, leftEdge);
        addZone(root, rightEdge);
        addArrow(root, leftEdge.right() + 6, leftEdge.right() + 6 + arrow, backY, thickness);
        addArrow(root, rightEdge.left - 6, rightEdge.left - 6 - arrow, backY, thickness);
        const int backX = leftEdge.right() + arrow + 20;
        addLabel(root, backX, backY - 14, width - 2 * backX, "TouchTutorialBack", "TouchTutorialBackText",
            MyGUI::Align::HCenter);

        // The button hints along the bottom can be tapped.
        addLabel(root, sMargin, height - 160, width - 2 * sMargin, "TouchTutorialHints", "TouchTutorialHintsText",
            MyGUI::Align::HCenter);

        addContinue(root);
    }

    void TouchOverlay::showProgress(const std::string& label, float fraction)
    {
        MyGUI::Widget* root = prepare();
        if (root != nullptr)
            ControllerUi::drawProgressPanel(root, label, fraction);
    }

    void TouchOverlay::hide()
    {
        if (mRoot != nullptr && mRoot->getVisible())
        {
            ControllerUi::clearPanel(mRoot);
            mRoot->setVisible(false);
        }
    }
}
