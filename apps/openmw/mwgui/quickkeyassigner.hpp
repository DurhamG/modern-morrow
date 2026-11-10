#ifndef MWGUI_QUICKKEYASSIGNER_H
#define MWGUI_QUICKKEYASSIGNER_H

#include <components/esm/refid.hpp>
#include <components/esm3/quickkeys.hpp>

#include "../mwworld/ptr.hpp"

namespace MyGUI
{
    class Widget;
}

namespace MWGui
{
    /// Puts an item or a spell on a d-pad quick key, from the inventory and magic screens in controller menus mode.
    /// The player clicks the left stick on the item or spell, then presses a d-pad direction to put it on that
    /// direction's press slot, or holds the direction to put it on the hold slot. Choosing the slot it is already on
    /// takes it off.
    class QuickKeyAssigner
    {
    public:
        struct Target
        {
            ESM::QuickKeys::Type mType = ESM::QuickKeys::Type::Item;
            MWWorld::Ptr mItem;
            ESM::RefId mSpell;
        };

        /// Whether the player is choosing a direction, and the screen shows the slots.
        bool isChoosing() const { return mState == State::Choosing || mState == State::Pressed; }

        /// Whether the assigner is reading the d-pad. The d-pad then doesn't move the screen's selection.
        bool hasDpad() const { return mState != State::Idle; }

        void start(const Target& target);
        void cancel();

        /// Handles a button press while choosing. Returns true when the screen needs to redraw.
        bool onButton(int button);

        /// Tells a press of a direction from a hold, and assigns the slot once it knows which. Returns true when the
        /// screen needs to redraw.
        bool update(float duration);

        /// Draws the eight d-pad slots, and what is on each, in the area. Clicking a slot chooses it, as its direction
        /// does. While choosing, the mouse wheel does nothing, since it would otherwise press the d-pad.
        void draw(MyGUI::Widget* parent, int x, int y, int w, int h);

    private:
        enum class State
        {
            Idle,
            Choosing,
            // A direction is down, and it isn't known yet whether it is a press or a hold.
            Pressed,
            // The direction was held long enough and its hold slot is assigned. The d-pad stays with the assigner
            // until the direction is let go, so the held direction doesn't then move the selection.
            WaitingForRelease,
        };

        void assign(int slot);
        int targetSlot() const;

        Target mTarget;
        State mState = State::Idle;
        int mDirection = -1;
        float mHeldTime = 0.f;
        // The slot the mouse clicked, which update assigns.
        int mClickedSlot = -1;
    };

    /// Returns the d-pad slot, from 1 to 8, that holds the item, or the spell when there is no item. Returns -1 when
    /// no d-pad slot does.
    int dpadSlotOf(const MWWorld::Ptr& item, const ESM::RefId& spell);

    /// Draws a small d-pad, five cells across, with one arm lit. A short lit arm is the direction's press slot, and a
    /// long lit arm is its hold slot.
    void drawQuickKeyMarker(MyGUI::Widget* parent, int x, int y, int cell, int slot);
}

#endif
