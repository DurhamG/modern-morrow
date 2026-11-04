#ifndef MWGUI_QUICKKEYS_H
#define MWGUI_QUICKKEYS_H

#include <memory>

#include "components/esm3/quickkeys.hpp"

#include "../mwworld/manualref.hpp"

#include "itemselection.hpp"
#include "spellmodel.hpp"
#include "windowbase.hpp"

namespace MWGui
{

    class QuickKeysMenuAssign;
    class MagicSelectionDialog;
    class ItemWidget;
    class SpellView;

    class QuickKeysMenu : public WindowBase
    {
    public:
        QuickKeysMenu();

        void onResChange(int, int) override { center(); }

        void onItemButtonClicked(MyGUI::Widget* sender);
        void onMagicButtonClicked(MyGUI::Widget* sender);
        void onUnassignButtonClicked(MyGUI::Widget* sender);
        void onCancelButtonClicked(MyGUI::Widget* sender);

        void onAssignItem(MWWorld::Ptr item);
        void onAssignItemCancel();
        void onAssignMagicItem(MWWorld::Ptr item);
        void onAssignMagic(const ESM::RefId& spellId);
        void onAssignMagicCancel();
        void onOpen() override;
        void onClose() override;

        void activateQuickKey(int index);
        void updateActivatedQuickKey();

        /// What a quick key slot holds.
        struct Slot
        {
            ESM::QuickKeys::Type mType = ESM::QuickKeys::Type::Unassigned;
            // The spell's ID, or the item's record ID
            ESM::RefId mId;
            MWWorld::Ptr mItem;
            std::string mName;
            // The icon of a spell's first effect, or of hand-to-hand. Items have their own icons.
            std::string mIcon;
        };

        /// Returns what a slot holds. The slots are numbered from 1 to 10, like the number keys that use them.
        Slot getSlot(int index);

        /// Puts an item, an enchanted item or a spell on a slot from 1 to 9, in place of what the slot held. The
        /// item or spell leaves any other slot it was on, so each one is on one slot at most.
        void assignSlot(int index, ESM::QuickKeys::Type type, const MWWorld::Ptr& item, const ESM::RefId& spell);
        void unassignSlot(int index);

        /// Returns the slot from 1 to 9 that holds the item, or -1 when none does.
        int findItemSlot(const MWWorld::Ptr& item);
        /// Returns the slot from 1 to 9 that holds the spell, or -1 when none does.
        int findSpellSlot(const ESM::RefId& spell);

        void write(ESM::ESMWriter& writer);
        void readRecord(ESM::ESMReader& reader, uint32_t type);
        void clear() override;

        std::string_view getWindowIdForLua() const override { return "QuickKeys"; }

    private:
        struct keyData
        {
            int index = -1;
            ItemWidget* button = nullptr;
            ESM::QuickKeys::Type type = ESM::QuickKeys::Type::Unassigned;
            ESM::RefId id;
            std::string name;
        };

        std::vector<keyData> mKey;
        std::vector<MWWorld::ManualRef> mTemp;
        keyData* mSelected;
        keyData* mActivated;

        MyGUI::EditBox* mInstructionLabel;
        MyGUI::Button* mOkButton;

        std::unique_ptr<QuickKeysMenuAssign> mAssignDialog;
        std::unique_ptr<ItemSelectionDialog> mItemSelectionDialog;
        std::unique_ptr<MagicSelectionDialog> mMagicSelectionDialog;

        void onQuickKeyButtonClicked(MyGUI::Widget* sender);
        void onOkButtonClicked(MyGUI::Widget* sender);
        // Check if quick key is still valid
        inline void validate(int index);
        void unassign(keyData* key);
        void assignItem(MWWorld::Ptr item);

        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg) override;
        size_t mControllerFocus = 0;
    };

    class QuickKeysMenuAssign : public WindowModal
    {
    public:
        QuickKeysMenuAssign(QuickKeysMenu* parent);

    private:
        MyGUI::TextBox* mLabel;
        MyGUI::Button* mItemButton;
        MyGUI::Button* mMagicButton;
        MyGUI::Button* mUnassignButton;
        MyGUI::Button* mCancelButton;

        QuickKeysMenu* mParent;

        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg) override;
        size_t mControllerFocus = 0;
    };

    class MagicSelectionDialog : public WindowModal
    {
    public:
        MagicSelectionDialog(QuickKeysMenu* parent);

        void onOpen() override;
        bool exit() override;

        void setActiveControllerWindow(bool active) override;

    private:
        MyGUI::Button* mCancelButton;
        SpellView* mMagicList;

        QuickKeysMenu* mParent;

        void onCancelButtonClicked(MyGUI::Widget* sender);
        void onModelIndexSelected(SpellModel::ModelIndex index);

        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg) override;
    };
}

#endif
