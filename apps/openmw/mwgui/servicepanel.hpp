#ifndef MWGUI_SERVICEPANEL_H
#define MWGUI_SERVICEPANEL_H

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <SDL_events.h>

#include "../mwworld/ptr.hpp"

#include "controllerscreen.hpp"

namespace MyGUI
{
    class Widget;
}

namespace MWGui
{
    /// A merchant's service in controller menus mode, such as buying spells, training, travel or repair. It is an
    /// opaque panel on the right side of the screen, like the barter panel, with what the merchant offers and its
    /// price, and details beside the list. The service's own window still does the work and stays hidden: each entry
    /// stands for one of the window's buttons, and A clicks it. See ui-rework/design/services.md.
    class ServicePanel
    {
    public:
        struct Entry
        {
            // The window's button for this entry
            MyGUI::Widget* mButton = nullptr;
            std::string mName;
            std::string mSubtitle;
            int mPrice = 0;
            // Why the player can't pick the entry, such as not having enough gold. Empty when they can.
            std::string mUnavailable;
            // The entry's picture: an item's icon, or an image such as a skill's icon.
            MWWorld::Ptr mItem;
            std::string mIcon;
            // Labelled values and paragraphs for the details panel
            std::vector<std::pair<std::string, std::string>> mFacts;
            std::vector<std::string> mParagraphs;
        };

        struct Service
        {
            // The service's name, such as "Spells", and what A does, such as "Buy"
            std::string mTitle;
            std::string mAction;
            std::string mEmpty;
            std::function<MWWorld::Ptr()> mMerchant;
            std::function<std::vector<Entry>()> mEntries;
            std::function<void(MyGUI::Widget*)> mChoose;
            std::function<void()> mBack;
        };

        explicit ServicePanel(Service service);
        ~ServicePanel();

        void show();
        void hide();
        /// Reads the entries again and redraws, such as after the window rebuilt its buttons.
        void update();

        void onFrame(float duration);
        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg);

    private:
        Service mService;
        MyGUI::Widget* mRoot = nullptr;
        std::vector<Entry> mEntries;
        int mFocus = 0;
        int mScroll = 0;
        HeldDirection mHeldDirection;

        void draw();
        void drawList(int x, int y, int w, int h);
        void drawDetails(int x, int y, int w, int h);
    };
}

#endif
