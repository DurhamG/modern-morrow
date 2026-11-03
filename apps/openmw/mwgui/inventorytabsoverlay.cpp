#include "inventorytabsoverlay.hpp"

#include <MyGUI_ILayer.h>
#include <MyGUI_ImageBox.h>
#include <MyGUI_RenderManager.h>
#include <MyGUI_TextBox.h>

#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/windowmanager.hpp"

namespace MWGui
{
    namespace
    {
        constexpr int sTabWidth = 120;
        constexpr int sFontHeight = 22;
        const MyGUI::Colour sSelectedColour = MyGUI::Colour::White;
        const MyGUI::Colour sOtherColour = MyGUI::Colour::parse("#8c7a55");
    }

    InventoryTabsOverlay::InventoryTabsOverlay()
        : WindowBase("openmw_inventory_tabs.layout")
    {
        constexpr std::string_view kTabIds[] = { "TabMap", "TabInventory", "TabSpells", "TabStats" };

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
        int x = (width - sTabWidth * static_cast<int>(mTabs.size())) / 2;
        mL2Image->setPosition(x - 34, 11);
        for (MyGUI::TextBox* tab : mTabs)
        {
            tab->setCoord(x, 0, sTabWidth, mMainWidget->getHeight());
            x += sTabWidth;
        }
        mR2Image->setPosition(x + 12, 11);
        setTab(mTab);
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
