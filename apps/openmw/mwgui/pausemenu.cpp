#include "pausemenu.hpp"

#include <algorithm>

#include <MyGUI_Gui.h>
#include <MyGUI_ILayer.h>
#include <MyGUI_LayerManager.h>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/creaturestats.hpp"

#include "../mwworld/class.hpp"

namespace MWGui
{
    namespace
    {
        using namespace ControllerUi;

        constexpr int sPanelWidth = 420;
        constexpr int sRowHeight = 52;
        constexpr int sTitleHeight = 96;
        constexpr int sHintsHeight = 56;

        const MyGUI::Colour sPanelBackColour = MyGUI::Colour::parse("#0d0b08");

        std::string itemLabel(const std::string& item)
        {
            if (item == "savegame")
                return "#{Interface:PauseSave}";
            if (item == "loadgame")
                return "#{Interface:PauseLoad}";
            if (item == "options")
                return "#{Interface:PauseOptions}";
            return "#{Interface:PauseExit}";
        }
    }

    void PauseMenu::show(const std::vector<std::string>& items)
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
        if (!mRoot->getVisible() || items != mItems)
            mFocus = 0;
        mItems = items;
        mRoot->setSize(mRoot->getLayer()->getSize());
        mRoot->setVisible(true);
        mHeldDirection.reset();
        redraw();
    }

    void PauseMenu::hide()
    {
        if (mRoot != nullptr)
            mRoot->setVisible(false);
    }

    bool PauseMenu::isVisible() const
    {
        return mRoot != nullptr && mRoot->getVisible();
    }

    void PauseMenu::redraw()
    {
        while (mRoot->getChildCount() > 0)
            MyGUI::Gui::getInstance().destroyWidget(mRoot->getChildAt(0));
        const MyGUI::IntSize size = mRoot->getSize();

        // The game stays faintly visible behind the menu, but too dark to get in the way of reading it.
        addRect(mRoot, 0, 0, size.width, size.height, MyGUI::Colour::Black, 0.85f);

        const int panelH = sTitleHeight + static_cast<int>(mItems.size()) * sRowHeight + sHintsHeight;
        const int x = (size.width - sPanelWidth) / 2;
        const int y = (size.height - panelH) / 2;
        addRect(mRoot, x, y, sPanelWidth, panelH, sPanelBackColour);
        addRect(mRoot, x, y, sPanelWidth, 1, sAccentColour, 0.6f);

        // The character and where they are, so the player knows which game they're in.
        addText(mRoot, x + 28, y + 16, sPanelWidth - 56, 36, "#{Interface:PausePaused}", sFontTitle, sBrightColour);
        const MWWorld::Ptr player = MWMechanics::getPlayer();
        if (!player.isEmpty())
        {
            std::string subtitle = escaped(player.getClass().getName(player)) + ",  #{sLevel} "
                + std::to_string(player.getClass().getCreatureStats(player).getLevel());
            const std::string_view cell = MWBase::Environment::get().getWorld()->getCellName();
            if (!cell.empty())
                subtitle += ",  " + escaped(cell);
            addText(mRoot, x + 28, y + 52, sPanelWidth - 56, 26, subtitle, sFontSmall, sDimColour);
        }
        addRect(mRoot, x + 28, y + sTitleHeight - 12, sPanelWidth - 56, 1, sAccentColour, 0.2f);

        int rowY = y + sTitleHeight;
        for (int i = 0; i < static_cast<int>(mItems.size()); ++i)
        {
            const bool focused = i == mFocus;
            if (focused)
            {
                addRect(mRoot, x + 16, rowY + 4, sPanelWidth - 32, sRowHeight - 8, sAccentColour, 0.16f);
                addRect(mRoot, x + 16, rowY + 4, 3, sRowHeight - 8, sAccentColour);
            }
            addText(mRoot, x + 36, rowY, sPanelWidth - 72, sRowHeight, itemLabel(mItems[i]), sFontLarge,
                focused ? sActiveColour : sBrightColour);
            rowY += sRowHeight;
        }

        // What the buttons do.
        int hx = x + 28;
        const int hy = y + panelH - 42;
        for (const auto& [button, label] : { std::pair{ SDL_CONTROLLER_BUTTON_A, "#{Interface:PauseSelect}" },
                 std::pair{ SDL_CONTROLLER_BUTTON_B, "#{Interface:PauseReturn}" } })
        {
            addGlyph(mRoot, hx, hy + 1, 24, button);
            MyGUI::TextBox* text = addText(mRoot, hx + 32, hy - 3, 200, 30, label, sFontNormal, sBrightColour);
            hx += 32 + text->getTextSize().width + 26;
        }
    }

    std::string PauseMenu::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
    {
        if (arg.button == SDL_CONTROLLER_BUTTON_A && mFocus < static_cast<int>(mItems.size()))
            return mItems[mFocus];
        if (arg.button == SDL_CONTROLLER_BUTTON_B || arg.button == SDL_CONTROLLER_BUTTON_START)
            return "return";
        // The d-pad is read every frame in onFrame, so it can repeat while held.
        return {};
    }

    void PauseMenu::onFrame(float duration)
    {
        if (!isVisible() || mItems.empty())
            return;
        int dx = 0;
        int dy = 0;
        if (!mHeldDirection.update(duration, dx, dy) || dy == 0)
            return;
        const int focus = std::clamp(mFocus + dy, 0, static_cast<int>(mItems.size()) - 1);
        if (focus == mFocus)
            return;
        mFocus = focus;
        MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
        redraw();
    }
}
