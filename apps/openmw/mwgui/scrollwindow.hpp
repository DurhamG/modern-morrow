#ifndef MWGUI_SCROLLWINDOW_H
#define MWGUI_SCROLLWINDOW_H

#include <memory>

#include "readingpanel.hpp"
#include "windowbase.hpp"

#include "../mwworld/ptr.hpp"

namespace Gui
{
    class ImageButton;
}

namespace MWGui
{
    class ScrollWindow : public BookWindowBase
    {
    public:
        ScrollWindow();
        ~ScrollWindow() override;

        void onOpen() override;
        void onFrame(float duration) override;

        void setPtr(const MWWorld::Ptr& scroll) override;
        void setInventoryAllowed(bool allowed);

        void onClose() override;
        void onResChange(int, int) override { center(); }

        std::string_view getWindowIdForLua() const override { return "Scroll"; }

        ControllerButtons* getControllerButtons() override;

    protected:
        void onCloseButtonClicked(MyGUI::Widget* sender);
        void onTakeButtonClicked(MyGUI::Widget* sender);
        void setTakeButtonShow(bool show);
        void onKeyButtonPressed(MyGUI::Widget* sender, MyGUI::KeyCode key, MyGUI::Char character);
        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg) override;

    private:
        Gui::ImageButton* mCloseButton;
        Gui::ImageButton* mTakeButton;
        MyGUI::ScrollView* mTextView;

        MWWorld::Ptr mScroll;

        bool mTakeButtonShow;
        bool mTakeButtonAllowed;

        // In controller menus mode, the text shows in a panel in the style of the controller screens instead.
        std::unique_ptr<ReadingPanel> mPanel;
        void showPanel();
    };

}

#endif
