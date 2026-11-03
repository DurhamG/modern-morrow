#ifndef MWGUI_BARTERPANEL_H
#define MWGUI_BARTERPANEL_H

#include <string>
#include <vector>

#include <SDL_events.h>

#include "../mwworld/ptr.hpp"

#include "controllerscreen.hpp"
#include "itemmodel.hpp"

namespace MyGUI
{
    class Widget;
}

namespace MWGui
{
    class TradeWindow;
    class TradeItemModel;

    /// The barter window's look in controller menus mode: an opaque panel on the right side of the screen, over the
    /// world, with Buy and Sell tabs. Items go into a deal and come back out of it, and the deal and the offer show
    /// beside the list. The trade window still keeps the deal and makes the trade, and the panel reads it from there.
    /// See ui-rework/design/barter.md.
    class BarterPanel
    {
    public:
        explicit BarterPanel(TradeWindow& window);
        ~BarterPanel();

        void show();
        void hide();
        void update();

        void onFrame(float duration);
        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg);
        bool onControllerThumbstickEvent(const SDL_ControllerAxisEvent& arg);

    private:
        // Something the merchant sells or the player could sell, and how many of it are in the deal.
        struct Entry
        {
            ItemStack mStack;
            // How many are left out of the deal, and how many are in it
            int mAvailable = 0;
            int mInDeal = 0;
        };

        // A line in the list: a group header, or an index into the entries.
        struct Row
        {
            std::string mHeader;
            int mEntry = -1;
        };

        TradeWindow& mWindow;
        MyGUI::Widget* mRoot = nullptr;
        bool mSelling = false;
        std::vector<Entry> mEntries;
        int mFocus = 0;
        int mScroll = 0;
        HeldDirection mHeldDirection;
        float mHaggleTimer = 0.f;

        TradeItemModel* playerModel() const;
        void readEntries();
        std::vector<Row> buildRows() const;
        int focusedEntry() const;
        int unitPrice(const MWWorld::Ptr& item) const;

        void drawList(int x, int y, int w, int h, const std::vector<Row>& rows);
        void drawDetails(int x, int y, int w, int h, int entry);

        /// Moves count of the entry into the deal, or out of it when count is negative.
        void changeDeal(int entry, int count);
        void haggle(int direction);
        void nextGroup();
        void switchTab();
    };
}

#endif
