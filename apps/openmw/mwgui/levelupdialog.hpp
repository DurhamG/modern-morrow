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

        void onOpen() override;

        void setVisible(bool visible) override;

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

        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg) override;
        std::vector<MyGUI::Button*> mAttributeButtons;
        size_t mControllerFocus = 0;
    };

}

#endif
