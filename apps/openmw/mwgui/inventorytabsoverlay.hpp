#ifndef MWGUI_INVENTORYTABSSOVERLAY_H
#define MWGUI_INVENTORYTABSSOVERLAY_H

#include "windowbase.hpp"

namespace MyGUI
{
    class ImageBox;
    class TextBox;
}

namespace MWGui
{
    class InventoryTabsOverlay : public WindowBase
    {
    public:
        InventoryTabsOverlay();

        /// The tab bar's height in the units of the Windows layer, for the windows that sit below it.
        int getHeight();
        void setTab(size_t index);
        void onResChange(int width, int height) override;
        void onOpen() override;

    private:
        std::vector<MyGUI::TextBox*> mTabs;
        MyGUI::ImageBox* mUnderline;
        MyGUI::ImageBox* mL2Image;
        MyGUI::ImageBox* mR2Image;
        // Rest and the pause menu, which touch controls have no gesture for. Only shown with touch controls on.
        MyGUI::TextBox* mRest;
        MyGUI::TextBox* mMenu;
        size_t mTab = 0;

        void layoutTabs();
        void onTabClicked(MyGUI::Widget* sender);
        void onRestClicked(MyGUI::Widget* sender);
        void onMenuClicked(MyGUI::Widget* sender);
    };
}

#endif
