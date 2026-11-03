#ifndef MWGUI_LOOTPANEL_H
#define MWGUI_LOOTPANEL_H

#include <string>
#include <vector>

#include <SDL_events.h>

#include "controllerscreen.hpp"

namespace MyGUI
{
    class Widget;
}

namespace MWGui
{
    class ContainerWindow;
    struct ItemStack;

    /// The loot window's look in controller menus mode: an opaque panel on the right side of the screen, over the
    /// world, listing what a container or body holds, with the highlighted item's details beside the list. The
    /// container window still owns the container's items, and the panel reads them from there. See
    /// ui-rework/design/loot.md.
    class LootPanel
    {
    public:
        explicit LootPanel(ContainerWindow& window);
        ~LootPanel();

        void show();
        void hide();
        void update();

        void onFrame(float duration);
        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg);

    private:
        // A line in the list: a group header, or an index into the items.
        struct Row
        {
            std::string mHeader;
            int mItem = -1;
        };

        ContainerWindow& mWindow;
        MyGUI::Widget* mRoot = nullptr;
        std::vector<ItemStack> mItems;
        int mFocus = 0;
        int mScroll = 0;

        // Whether the panel shows the player's own items, to put into the container, under one of the inventory
        // screen's tabs. The selection in the container's items is kept for when the player comes back.
        bool mYourItems = false;
        int mYourTab = 0;
        int mContainerFocus = 0;
        std::string mShownState;
        float mRefreshTimer = 0.f;
        HeldDirection mHeldDirection;

        void readItems();
        std::string currentState() const;
        std::vector<Row> buildRows() const;
        int focusedItem() const;
        bool isStealing() const;
        bool isPickpocketing() const;
        bool canTakeAndEquip(const ItemStack& item) const;

        void drawList(int x, int y, int w, int h, const std::vector<Row>& rows);
        void drawDetails(int x, int y, int w, int h, int item);

        void take(bool equip);
        void takeAll();
        void onTakeAllConfirmed();
        void putIn();
        void switchView();
        bool canPutIn() const;
    };
}

#endif
