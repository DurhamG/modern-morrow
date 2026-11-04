#ifndef MWGUI_SPELLWINDOW_H
#define MWGUI_SPELLWINDOW_H

#include <chrono>
#include <memory>
#include <vector>

#include "spellicons.hpp"
#include "spellmodel.hpp"
#include "windowpinnablebase.hpp"

namespace MWGui
{
    class SpellView;

    class SpellWindow : public WindowPinnableBase, public NoDrop
    {
    public:
        SpellWindow(DragAndDrop* drag);

        void updateSpells();

        void onFrame(float dt) override;

        /// Cycle to next/previous spell
        void cycle(bool next);

        std::string_view getWindowIdForLua() const override { return "Magic"; }

    protected:
        MyGUI::Widget* mEffectBox;

        ESM::RefId mSpellToDelete;

        void onEnchantedItemSelected(MWWorld::Ptr item, bool alreadyEquipped);
        void onSpellSelected(const ESM::RefId& spellId);
        void onModelIndexSelected(SpellModel::ModelIndex index);
        void onFilterChanged(MyGUI::EditBox* sender);
        void onDeleteClicked(MyGUI::Widget* widget);
        void onDeleteSpellAccept();
        void askDeleteSpell(const ESM::RefId& spellId);

        void onPinToggled() override;
        void onTitleDoubleClicked() override;
        void onOpen() override;
        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg) override;
        void setActiveControllerWindow(bool active) override;

        SpellView* mSpellView;
        std::unique_ptr<SpellIcons> mSpellIcons;
        MyGUI::EditBox* mFilterEdit;

    private:
        /// In controller menus mode, steps through the recently used spells and items instead of the full list.
        /// Returns false when there are too few recent ones, so the full list is used instead.
        bool cycleRecent(bool next);

        // Presses in quick succession step through the recent spells as they were at the first press, like
        // switching windows on a desktop. Readying a spell moves it to the front of the recent spells, so without
        // this the presses would switch between the first two.
        std::vector<ESM::RefId> mCycleOrder;
        std::size_t mCyclePosition = 0;
        std::chrono::steady_clock::time_point mLastCycle;
        float mUpdateTimer;
    };
}

#endif
