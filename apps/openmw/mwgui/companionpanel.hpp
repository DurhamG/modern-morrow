#ifndef MWGUI_COMPANIONPANEL_H
#define MWGUI_COMPANIONPANEL_H

#include <string>
#include <vector>

#include <SDL_events.h>

#include "controllerscreen.hpp"
#include "itemmodel.hpp"

namespace MyGUI
{
    class Widget;
}

namespace MWGui
{
    class CompanionWindow;

    /// Sharing items with a companion in controller menus mode: an opaque panel on the right side of the screen, like
    /// the loot panel. One tab lists the companion's items, which A takes, and the other the player's own items,
    /// which A gives. The companion window still owns the companion's items and the profit rules.
    class CompanionPanel
    {
    public:
        explicit CompanionPanel(CompanionWindow& window);
        ~CompanionPanel();

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

        CompanionWindow& mWindow;
        MyGUI::Widget* mRoot = nullptr;
        std::vector<ItemStack> mItems;
        // Whether the list shows the player's own items, to give to the companion
        bool mYourItems = false;
        ControllerList mList;

        void readItems();
        std::vector<Row> buildRows() const;
        int focusedItem() const;

        void drawList(int x, int y, int w, int h, const std::vector<Row>& rows);
        void move();
    };
}

#endif
