#ifndef MWGUI_MAGICSCREEN_H
#define MWGUI_MAGICSCREEN_H

#include <array>
#include <string>
#include <vector>

#include <components/esm/refid.hpp>

#include "../mwworld/ptr.hpp"

#include "controllerscreen.hpp"
#include "quickkeyassigner.hpp"

namespace ESM
{
    struct EffectList;
}

namespace MWGui
{
    /// The magic menu in controller menus mode. The Active tab shows the effects on the player, the spell or item
    /// that is ready to cast, and the ones used recently, so switching between them takes one press. The other tabs
    /// list every spell, power, magic item and scroll, with their effects written out, so the player never needs a
    /// tooltip. See ui-rework/design/magic.md.
    class MagicScreen : public ControllerScreen
    {
    public:
        MagicScreen();

        void onOpen() override;
        void onClose() override;
        void onFrame(float duration) override;
        void clear() override;

    protected:
        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg) override;
        bool onControllerThumbstickEvent(const SDL_ControllerAxisEvent& arg) override;

    private:
        enum class Tab
        {
            Recent,
            Active,
            Spells,
            Powers,
            Items,
            Scrolls,
        };
        static constexpr int sTabCount = 6;

        // Something the player can ready to cast: a spell, a power, an enchanted item or a scroll.
        struct Castable
        {
            enum class Kind
            {
                Spell,
                Power,
                Item,
                Scroll,
            };
            Kind mKind = Kind::Spell;
            // The spell's ID, or the item's record ID
            ESM::RefId mId;
            MWWorld::Ptr mItem;
            std::string mName;
            const ESM::EffectList* mEffects = nullptr;
            // The first effect's icon, for spells and powers
            std::string mIcon;
            ESM::RefId mSchool;
            // Magicka for a spell, or charge for an item
            int mCost = 0;
            int mChance = 0;
            int mCharge = 0;
            int mMaxCharge = 0;
            int mCount = 1;
            bool mReady = false;
            bool mDeletable = false;
            bool mUsedToday = false;
        };

        // An effect on the player, and where it came from.
        struct ActiveEffect
        {
            std::string mIcon;
            std::string mText;
            std::string mSource;
            // Seconds left, or -1 for effects that last until something removes them
            float mTimeLeft = -1.f;
        };

        // A line in a list: a group header, or an index into mCastables.
        struct Row
        {
            std::string mHeader;
            int mItem = -1;
        };

        std::vector<Castable> mCastables;
        std::vector<ActiveEffect> mEffects;
        // A summary of what the screen last showed, to tell when it needs to redraw.
        std::string mShownState;
        float mRefreshTimer = 0.f;

        Tab mTab = Tab::Active;
        // Each tab's list. The Active tab has its own two lists instead.
        std::array<ControllerList, sTabCount> mLists;

        // The Active tab's two columns: the effects on the left, and the recently used spells on the right.
        bool mEffectsFocused = false;
        ControllerList mEffectList;
        ControllerList mRecentList;

        HeldDirection mHeldDirection;
        QuickKeyAssigner mQuickKeys;
        ESM::RefId mSpellToDelete;

        void update();
        std::string currentState() const;
        void redraw() override;

        std::vector<int> recentCastables(bool skipReady) const;
        int readyCastable() const;
        std::vector<Row> buildRows();
        int focusedCastable();

        void drawActive(int width);
        void drawEffects(int x, int y, int w, int h);
        void drawReadyCard(int x, int y, int w, int h, int item);
        void drawList(int x, int y, int w, int h, const std::vector<Row>& rows, ControllerList& list, bool focused);
        void coverList(int x, int y, int w, int h, int focusedRowY, int rowHeight, bool focused, bool effects);
        void drawEntry(int x, int y, int w, int h, int item, bool focused);
        void drawDetails(int x, int y, int w, int h, int item);
        int drawHints(int x, int y, int w, int h, int item);

        void move(int dx, int dy);
        void changeTab(int delta);
        void nextGroup();
        void ready(int item);
        void startQuickKey();
        void askDelete(int item);
        void onDeleteAccepted();
    };
}

#endif
