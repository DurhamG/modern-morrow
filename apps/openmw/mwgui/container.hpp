#ifndef MGUI_CONTAINER_H
#define MGUI_CONTAINER_H

#include <memory>

#include "itemmodel.hpp"
#include "referenceinterface.hpp"
#include "windowbase.hpp"

#include <components/misc/notnullptr.hpp>

namespace MyGUI
{
    class Gui;
    class Widget;
}

namespace MWGui
{
    class ContainerWindow;
    class ItemView;
    class SortFilterItemModel;
    class ItemTransfer;
    class LootPanel;

    class ContainerWindow : public WindowBase, public ReferenceInterface
    {
    public:
        explicit ContainerWindow(DragAndDrop& dragAndDrop, ItemTransfer& itemTransfer);
        ~ContainerWindow() override;

        void setPtr(const MWWorld::Ptr& container) override;

        void onOpen() override;

        void onClose() override;

        void clear() override { resetReference(); }

        void onFrame(float dt) override;

        void resetReference() override;

        void onDeleteCustomData(const MWWorld::Ptr& ptr) override;

        void treatNextOpenAsLoot() { mTreatNextOpenAsLoot = true; }

        void onInventoryUpdate(const MWWorld::Ptr& ptr) override;

        std::string_view getWindowIdForLua() const override { return "Container"; }

        ControllerButtons* getControllerButtons() override;
        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg) override;
        bool onControllerThumbstickEvent(const SDL_ControllerAxisEvent& arg) override;
        void setActiveControllerWindow(bool active) override;

        MWGui::ItemView* getItemView() { return mItemView; }
        ItemModel* getModel() { return mModel; }

    private:
        Misc::NotNullPtr<DragAndDrop> mDragAndDrop;
        Misc::NotNullPtr<ItemTransfer> mItemTransfer;

        MWGui::ItemView* mItemView;
        SortFilterItemModel* mSortModel;
        ItemModel* mModel;
        int mSelectedItem;
        bool mUpdateNextFrame;
        bool mTreatNextOpenAsLoot;
        MyGUI::Button* mDisposeCorpseButton;
        MyGUI::Button* mTakeButton;
        MyGUI::Button* mCloseButton;

        void onItemSelected(int index);
        void onBackgroundSelected();
        void dragItem(MyGUI::Widget* sender, std::size_t count);
        void transferItem(MyGUI::Widget* sender, std::size_t count);
        void dropItem();
        void onCloseButtonClicked(MyGUI::Widget* sender);
        void onTakeAllButtonClicked(MyGUI::Widget* sender);
        void onDisposeCorpseButtonClicked(MyGUI::Widget* sender);

        void onReferenceUnavailable() override;

        // In controller menus mode, a panel on the right side of the screen shows the container instead of this
        // window. It reads the items from here.
        friend class LootPanel;
        std::unique_ptr<LootPanel> mPanel;
    };
}
#endif // CONTAINER_H
