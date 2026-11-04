#ifndef OPENMW_MWGUI_MERCHANTREPAIR_H
#define OPENMW_MWGUI_MERCHANTREPAIR_H

#include <memory>

#include "../mwworld/ptr.hpp"
#include "servicepanel.hpp"
#include "windowbase.hpp"

namespace MWGui
{

    class MerchantRepair : public WindowBase
    {
    public:
        MerchantRepair();
        ~MerchantRepair() override;

        void onOpen() override;
        void onClose() override;
        void onFrame(float dt) override;

        void setPtr(const MWWorld::Ptr& actor) override;

        std::string_view getWindowIdForLua() const override { return "MerchantRepair"; }

    private:
        MyGUI::ScrollView* mList;
        MyGUI::Button* mOkButton;
        MyGUI::TextBox* mGoldLabel;
        /// List of enabled/repairable items and their index in the full list.
        std::vector<std::pair<MyGUI::Button*, size_t>> mButtons;

        MWWorld::Ptr mActor;

        size_t mControllerFocus = 0;

        // In controller menus mode, a panel on the right side of the screen shows the service instead of this window.
        std::unique_ptr<ServicePanel> mPanel;
        std::vector<ServicePanel::Entry> controllerEntries();

    protected:
        void onMouseWheel(MyGUI::Widget* sender, int rel);
        void onRepairButtonClick(MyGUI::Widget* sender);
        void onOkButtonClick(MyGUI::Widget* sender);
        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg) override;
    };

}

#endif
