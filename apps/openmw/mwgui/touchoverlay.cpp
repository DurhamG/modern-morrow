#include "touchoverlay.hpp"

#include <algorithm>

#include <MyGUI_Gui.h>
#include <MyGUI_TextBox.h>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"

#include "controllerscreen.hpp"
#include "quickkeysmenu.hpp"

namespace MWGui
{
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
