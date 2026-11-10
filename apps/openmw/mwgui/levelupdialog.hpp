#ifndef MWGUI_LEVELUPDIALOG_H
#define MWGUI_LEVELUPDIALOG_H

#include <components/esm/refid.hpp>

#include "windowbase.hpp"

namespace MWGui
{

    class LevelupDialog : public WindowBase
    {
    public:
        LevelupDialog();
        ~LevelupDialog() override;

        void onOpen() override;
        void onClose() override;

        std::string_view getWindowIdForLua() const override { return "LevelUpDialog"; }

    private:
        struct Widgets
        {
            MyGUI::Button* mButton;
            MyGUI::TextBox* mValue;
            MyGUI::TextBox* mMultiplier;
        };
        MyGUI::Button* mOkButton;
        MyGUI::ImageBox* mClassImage;
        MyGUI::TextBox* mLevelText;
        MyGUI::EditBox* mLevelDescription;

        MyGUI::Widget* mCoinBox;
        MyGUI::ScrollView* mAssignWidget;

        std::map<ESM::RefId, Widgets> mAttributeWidgets;
        std::vector<MyGUI::ImageBox*> mCoins;

        std::vector<ESM::RefId> mSpentAttributes;

        size_t mPerCol;
        unsigned int mCoinCount;

        void onOkButtonClicked(MyGUI::Widget* sender);
        void onAttributeClicked(MyGUI::Widget* sender);

        void assignCoins();
        void resetCoins();

        void setAttributeValues();

        std::string_view getLevelupClassImage(
            const int combatIncreases, const int magicIncreases, const int stealthIncreases);

        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg) override;
        std::vector<MyGUI::Button*> mAttributeButtons;
        size_t mControllerFocus = 0;

        // In controller menus mode, a full screen in the style of the stats screen shows the level up instead of this
        // window.
        MyGUI::Widget* mPanel = nullptr;
        ESM::RefId mClassImageId;
        void drawPanel();
        // Highlights the attribute tile that the mouse points at or clicks.
        void focusWithPointer(std::size_t index);
    };

}

#endif
