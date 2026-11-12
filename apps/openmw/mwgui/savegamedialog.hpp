#ifndef OPENMW_MWGUI_SAVEGAMEDIALOG_H
#define OPENMW_MWGUI_SAVEGAMEDIALOG_H

#include <memory>

#include "../mwbase/statemanager.hpp"

#include "controllerscreen.hpp"
#include "windowbase.hpp"

namespace MWState
{
    class Character;
    struct Slot;
}

namespace MWGui
{

    class SaveGameDialog : public MWGui::WindowModal
    {
    public:
        SaveGameDialog();
        ~SaveGameDialog() override;

        void onOpen() override;
        void onClose() override;

        void setLoadOrSave(bool load);

        ControllerButtons* getControllerButtons() override;

    private:
        void confirmDeleteSave();

        void onKeyButtonPressed(MyGUI::Widget* sender, MyGUI::KeyCode key, MyGUI::Char character);
        void onCancelButtonClicked(MyGUI::Widget* sender);
        void onOkButtonClicked(MyGUI::Widget* sender);
        void onDeleteButtonClicked(MyGUI::Widget* sender);
        void onCharacterSelected(MyGUI::ComboBox* sender, size_t pos);
        void onCharacterAccept(MyGUI::ComboBox* sender, size_t pos);
        // Slot selected (mouse click or arrow keys)
        void onSlotSelected(MyGUI::ListBox* sender, size_t pos);
        // Slot activated (double click or enter key)
        void onSlotActivated(MyGUI::ListBox* sender, size_t pos);
        // Slot clicked with mouse
        void onSlotMouseClick(MyGUI::ListBox* sender, size_t pos);

        void onDeleteSlotConfirmed();
        void onDeleteSlotCancel();

        void onEditSelectAccept(MyGUI::EditBox* sender);
        void onSaveNameChanged(MyGUI::EditBox* sender);
        void onConfirmationGiven();
        void onConfirmationCancel();

        void accept(bool reallySure = false);

        void fillSaveList();
        /// Lists the characters of the shown saves folder, and selects the one to show.
        void fillCharacters();

        std::unique_ptr<MyGUI::ITexture> mScreenshotTexture;
        MyGUI::ImageBox* mScreenshot;
        bool mSaving;

        MyGUI::ComboBox* mCharacterSelection;
        MyGUI::EditBox* mCellName;
        MyGUI::EditBox* mInfoText;
        MyGUI::Button* mOkButton;
        MyGUI::Button* mCancelButton;
        MyGUI::Button* mDeleteButton;
        MyGUI::ListBox* mSaveList;
        MyGUI::EditBox* mSaveNameEdit;

        const MWState::Character* mCurrentCharacter;
        const MWState::Slot* mCurrentSlot;

        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg) override;
        bool mOkButtonFocus = true;

        // In controller menus mode, a full screen panel shows the saves instead of this window. When saving, the
        // first entry is a new save with the name the dialog made up. When an OpenMW install was found, tabs switch
        // between this game's saves and OpenMW's, so a game can be loaded from OpenMW or saved into its folder.
        MyGUI::Widget* mPanel = nullptr;
        MWBase::StateManager::SaveFolder mFolder = MWBase::StateManager::SaveFolder::Own;
        ControllerList mList;
        std::string mNewSaveName;
        std::size_t panelEntryCount() const;
        void selectPanelEntry();
        void showPanel();
        void drawPanel();
        void changeCharacter(int delta);
        bool showFolderTabs() const;
        void changeFolder();
        bool onControllerTrigger(bool right) override;

    public:
        void onFrame(float duration) override;
    };

}

#endif
