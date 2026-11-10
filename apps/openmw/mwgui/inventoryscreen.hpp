#ifndef MWGUI_INVENTORYSCREEN_H
#define MWGUI_INVENTORYSCREEN_H

#include <array>
#include <memory>
#include <string>
#include <vector>

#include <MyGUI_Types.h>

#include <components/esm/refid.hpp>

#include "controllerscreen.hpp"
#include "itemmodel.hpp"
#include "quickkeyassigner.hpp"

namespace MyGUI
{
    class Widget;
}

namespace MWGui
{
    class InventoryItemModel;

    /// The inventory in controller menus mode. Items are sorted into tabs and listed with the stats that matter
    /// for them, and a details panel compares the highlighted item with the equipped one, so the player never
    /// needs a tooltip. See ui-rework/design/inventory.md.
    class InventoryScreen : public ControllerScreen
    {
    public:
        InventoryScreen();
        ~InventoryScreen() override;

        void onOpen() override;
        void onClose() override;
        void onFrame(float duration) override;
        void onInventoryUpdate(const MWWorld::Ptr& ptr) override;
        void clear() override;

    protected:
        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg) override;
        bool onControllerThumbstickEvent(const SDL_ControllerAxisEvent& arg) override;

    private:
        enum class Tab
        {
            Recent,
            Equipment,
            Consumables,
            Notes,
            Tools,
            Misc,
        };
        static constexpr int sTabCount = 6;
        static constexpr int sAreaCount = 12;

        // Which part of the Equipment tab has the selection.
        enum class Focus
        {
            Areas,
            Slots,
            Items,
        };

        // A line in an item list: a group header, or an item.
        struct Row
        {
            std::string mHeader;
            int mItem = -1;
        };

        std::unique_ptr<InventoryItemModel> mModel;
        std::vector<ItemStack> mItems;
        bool mNeedsUpdate = true;

        Tab mTab = Tab::Equipment;
        std::array<ControllerList, sTabCount> mLists;

        // Each list remembers its own sort order, as a position in the orders the sort button cycles through.
        std::array<int, sTabCount> mTabSort{};
        std::array<int, sAreaCount> mAreaSort{};

        // Equipment tab state
        Focus mFocus = Focus::Areas;
        int mArea = 0;
        int mSlotFocus = 0;
        int mSlotFilter = -1;
        int mPendingRing = -1;
        ControllerList mAreaList;

        // The item last highlighted, which is marked as seen once the selection moves off it.
        ESM::RefId mHighlighted;

        HeldDirection mHeldDirection;
        QuickKeyAssigner mQuickKeys;
        int mPendingDropItem = -1;

        // The book shown in the Notes tab, and how far its text is scrolled.
        ESM::RefId mBookItem;
        float mBookScroll = 0.f;
        int mBookScrollMax = 0;
        MyGUI::Widget* mBookPaper = nullptr;
        MyGUI::Widget* mBookScrollThumb = nullptr;
        MyGUI::IntCoord mBookTrack;

        void updateItems();
        void redraw() override;
        void setHighlighted(const ESM::RefId& item);

        std::vector<Row> buildRows();
        std::vector<int> areaItems(int area) const;
        int areaTopItem(int area) const;
        bool areaHasNew(int area) const;

        void drawHeader(int width);
        void drawEquipmentOverview(int width);
        void drawArea(int width);
        void drawList(int x, int y, int w, int h, const std::vector<Row>& rows, ControllerList& list, bool focused);
        void drawDetailsHeader(int x, int y, int w, int h, int item);
        int drawButtonHints(int x, int y, int w, int h, int item, const std::string& action, bool scroll = false);
        void drawDetails(int x, int y, int w, int h, int item, const std::string& action);
        void drawBook(int x, int y, int w, int h, int item);
        void placeBookText();
        void scrollBook(float stick, float duration);
        void drawEmptyDetails(int x, int y, int w, int h, const std::string& text);

        void move(int dx, int dy);
        void moveInList(ControllerList& list, const std::vector<Row>& rows, int delta);
        void changeTab(int delta);
        void activate();
        void back();
        void dropFocused();
        bool canQuickKey(int item) const;
        void startQuickKey();
        void cycleSort();

        int& sortIndex();
        int focusedItem();
        int compareSlot(int item) const;
        int compareItem(int item) const;
        std::string actionFor(int item) const;
        void useItem(int item);
        void equipInSlot(int item, int slot);
        void onDropCountChosen(MyGUI::Widget* sender, std::size_t count);
        void dropItem(int item, int count);
    };
}

#endif
