#include "inventorytabsoverlay.hpp"

#include <MyGUI_ILayer.h>
#include <MyGUI_ImageBox.h>
#include <MyGUI_RenderManager.h>
#include <MyGUI_TextBox.h>

#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/windowmanager.hpp"

namespace MWGui
{
    namespace
    {
        constexpr int sTabWidth = 120;
        constexpr int sTouchButtonWidth = 90;
        constexpr int sFontHeight = 22;
        const MyGUI::Colour sSelectedColour = MyGUI::Colour::White;
        const MyGUI::Colour sOtherColour = MyGUI::Colour::parse("#8c7a55");
    }

    InventoryTabsOverlay::InventoryTabsOverlay()
        : WindowBase("openmw_inventory_tabs.layout")
    {
        constexpr std::string_view kTabIds[] = { "TabMap", "TabInventory", "TabSpells", "TabStats", "TabJournal" };

        for (const std::string_view id : kTabIds)
        {
            MyGUI::TextBox* tab;
            getWidget(tab, id);
            tab->setFontHeight(sFontHeight);
            tab->setTextAlign(MyGUI::Align::Center);
            tab->setTextShadow(true);
            tab->setTextShadowColour(MyGUI::Colour::Black);
            tab->eventMouseButtonClick += MyGUI::newDelegate(this, &InventoryTabsOverlay::onTabClicked);
            mTabs.push_back(tab);
        }
        getWidget(mUnderline, "TabUnderline");

        for (MyGUI::TextBox** button : { &mRest, &mMenu })
        {
            *button = mMainWidget->createWidget<MyGUI::TextBox>(
                "NormalText", MyGUI::IntCoord(0, 0, sTouchButtonWidth, 44), MyGUI::Align::Default);
            (*button)->setFontHeight(sFontHeight);
            (*button)->setTextAlign(MyGUI::Align::Center);
            (*button)->setTextColour(sOtherColour);
            (*button)->setTextShadow(true);
            (*button)->setTextShadowColour(MyGUI::Colour::Black);
        }
        mRest->setCaptionWithReplacing("#{Interface:Rest}");
        mRest->eventMouseButtonClick += MyGUI::newDelegate(this, &InventoryTabsOverlay::onRestClicked);
        mMenu->setCaptionWithReplacing("#{Interface:TouchMenu}");
        mMenu->eventMouseButtonClick += MyGUI::newDelegate(this, &InventoryTabsOverlay::onMenuClicked);

        getWidget(mL2Image, "BtnL2Image");
        mL2Image->setImageTexture(
            MWBase::Environment::get().getInputManager()->getControllerAxisIcon(SDL_CONTROLLER_AXIS_TRIGGERLEFT));

        getWidget(mR2Image, "BtnR2Image");
        mR2Image->setImageTexture(
            MWBase::Environment::get().getInputManager()->getControllerAxisIcon(SDL_CONTROLLER_AXIS_TRIGGERRIGHT));

        layoutTabs();
        setTab(0);
    }

    int InventoryTabsOverlay::getHeight()
    {
        // The overlay's layer keeps a constant size relative to the screen height, so convert its height to the
        // units of the Windows layer.
        const int layerHeight = mMainWidget->getLayer()->getSize().height;
        const int viewHeight = MyGUI::RenderManager::getInstance().getViewSize().height;
        if (layerHeight <= 0)
            return mMainWidget->getHeight();
        return mMainWidget->getHeight() * viewHeight / layerHeight;
    }

    void InventoryTabsOverlay::onResChange(int /*width*/, int /*height*/)
    {
        const MyGUI::IntSize layerSize = mMainWidget->getLayer()->getSize();
        mMainWidget->setCoord(0, 0, layerSize.width, mMainWidget->getHeight());
        layoutTabs();
    }

    void InventoryTabsOverlay::layoutTabs()
    {
        const int width = mMainWidget->getWidth();
        // With touch controls, Rest and Menu take the right end, and the tabs centre in the rest of the bar.
        const bool touch = Settings::input().mTouchControls;
        const int buttons = touch ? 2 * sTouchButtonWidth + 16 : 0;
        int x = (width - buttons - sTabWidth * static_cast<int>(mTabs.size())) / 2;
        mL2Image->setPosition(x - 34, 11);
        for (MyGUI::TextBox* tab : mTabs)
        {
            tab->setCoord(x, 0, sTabWidth, mMainWidget->getHeight());
            x += sTabWidth;
        }
        mR2Image->setPosition(x + 12, 11);

        mMenu->setCoord(width - 16 - sTouchButtonWidth, 0, sTouchButtonWidth, mMainWidget->getHeight());
        mRest->setCoord(mMenu->getLeft() - sTouchButtonWidth, 0, sTouchButtonWidth, mMainWidget->getHeight());
        mRest->setVisible(touch);
        mMenu->setVisible(touch);
        setTab(mTab);
    }

    void InventoryTabsOverlay::onOpen()
    {
        layoutTabs();
    }

    void InventoryTabsOverlay::onRestClicked(MyGUI::Widget* /*sender*/)
    {
        MWBase::WindowManager* winMgr = MWBase::Environment::get().getWindowManager();
        if (!MWBase::Environment::get().getInputManager()->getControlSwitch("playercontrols")
            || !winMgr->getRestEnabled())
            return;
        winMgr->removeGuiMode(GM_Inventory);
        winMgr->pushGuiMode(GM_Rest);
    }

    void InventoryTabsOverlay::onMenuClicked(MyGUI::Widget* /*sender*/)
    {
        MWBase::WindowManager* winMgr = MWBase::Environment::get().getWindowManager();
        winMgr->removeGuiMode(GM_Inventory);
        winMgr->pushGuiMode(GM_MainMenu);
    }

    void InventoryTabsOverlay::onTabClicked(MyGUI::Widget* sender)
    {
        if (!MWBase::Environment::get().getWindowManager()->getJournalAllowed())
            return;

        for (size_t i = 0; i < mTabs.size(); ++i)
        {
            if (mTabs[i] == sender)
            {
                MWBase::Environment::get().getWindowManager()->setActiveControllerWindow(GM_Inventory, i);
                setTab(i);
                break;
            }
        }
    }

    void InventoryTabsOverlay::setTab(size_t index)
    {
        mTab = index;
        for (size_t i = 0; i < mTabs.size(); ++i)
            mTabs[i]->setTextColour(i == index ? sSelectedColour : sOtherColour);
        if (index < mTabs.size())
        {
            const MyGUI::IntCoord coord = mTabs[index]->getCoord();
            mUnderline->setCoord(coord.left + 20, 38, coord.width - 40, 2);
        }
    }
}
