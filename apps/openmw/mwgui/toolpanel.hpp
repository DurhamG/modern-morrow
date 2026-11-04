#ifndef MWGUI_TOOLPANEL_H
#define MWGUI_TOOLPANEL_H

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
    class Recharge;
    class Repair;

    /// Repairing gear with the player's own repair tools, or recharging magic items with a soul gem, in controller
    /// menus mode, as a full screen. The items that need it are listed on the left with their condition or charge,
    /// and A uses the tool on the highlighted one. The tool is on the right, with what is left of it, and Y switches
    /// to the next tool of its kind. When a tool runs out, the next one takes its place. The repair or recharge
    /// window stays hidden and still does the work. See ui-rework/design/crafting.md.
    class ToolPanel
    {
    public:
        explicit ToolPanel(Repair& window);
        explicit ToolPanel(Recharge& window);
        ~ToolPanel();

        void show();
        void hide();
        void update();

        void onFrame(float duration);
        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg);

    private:
        // The window the panel stands in for. One of the two is set.
        Repair* mRepair = nullptr;
        Recharge* mRecharge = nullptr;
        MyGUI::Widget* mRoot = nullptr;
        std::vector<MWWorld::Ptr> mItems;
        int mFocus = 0;
        int mScroll = 0;
        HeldDirection mHeldDirection;

        MWWorld::Ptr tool() const;
        std::vector<MWWorld::Ptr> tools() const;
        void setTool(const MWWorld::Ptr& tool);
        // The item's condition or charge now, and at most.
        std::pair<int, int> itemValues(const MWWorld::Ptr& item) const;

        void drawList(int x, int y, int w, int h);
        void drawTool(int x, int y, int w, int h);

        void useTool();
        void nextTool();
    };
}

#endif
