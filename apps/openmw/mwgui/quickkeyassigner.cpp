#include "quickkeyassigner.hpp"

#include <array>
#include <utility>

#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/windowmanager.hpp"

#include "controllerscreen.hpp"
#include "quickkeyslots.hpp"
#include "quickkeysmenu.hpp"

namespace MWGui
{
    namespace
    {
        using namespace ControllerUi;
        using namespace QuickKeySlots;

        QuickKeysMenu& quickKeys()
        {
            return *MWBase::Environment::get().getWindowManager()->getQuickKeysMenu();
        }

        bool isDown(int direction)
        {
            return MWBase::Environment::get().getInputManager()->isControllerButtonPressed(sDirections[direction]);
        }
    }

    void QuickKeyAssigner::start(const Target& target)
    {
        mTarget = target;
        mState = State::Choosing;
        mDirection = -1;
        mHeldTime = 0.f;
    }

    void QuickKeyAssigner::cancel()
    {
        mState = State::Idle;
        mTarget = {};
    }

    bool QuickKeyAssigner::onButton(int button)
    {
        if (mState != State::Choosing)
            return false;
        if (const int direction = directionOf(button); direction >= 0)
        {
            mState = State::Pressed;
            mDirection = direction;
            mHeldTime = 0.f;
            return true;
        }
        if (button == SDL_CONTROLLER_BUTTON_B || button == SDL_CONTROLLER_BUTTON_LEFTSTICK)
        {
            cancel();
            return true;
        }
        return false;
    }

    bool QuickKeyAssigner::update(float duration)
    {
        if (mClickedSlot >= 0)
        {
            const int slot = mClickedSlot;
            mClickedSlot = -1;
            if (mState == State::Choosing)
            {
                assign(slot);
                cancel();
                return true;
            }
        }
        if (mState == State::Pressed)
        {
            if (!isDown(mDirection))
            {
                assign(slotFor(mDirection, false));
                cancel();
                return true;
            }
            mHeldTime += duration;
            if (mHeldTime >= sHoldTime)
            {
                assign(slotFor(mDirection, true));
                mState = State::WaitingForRelease;
                return true;
            }
        }
        else if (mState == State::WaitingForRelease && !isDown(mDirection))
            cancel();
        return false;
    }

    int QuickKeyAssigner::targetSlot() const
    {
        return dpadSlotOf(mTarget.mItem, mTarget.mSpell);
    }

    void QuickKeyAssigner::assign(int slot)
    {
        if (targetSlot() == slot)
            quickKeys().unassignSlot(slot);
        else
            quickKeys().assignSlot(slot, mTarget.mType, mTarget.mItem, mTarget.mSpell);
        MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
    }

    void QuickKeyAssigner::draw(MyGUI::Widget* parent, int x, int y, int w, int h)
    {
        // The wheel works as the d-pad elsewhere, which here would choose a slot by accident.
        addWheelTarget(parent, 0, 0, parent->getWidth(), parent->getHeight(), [](int) {});

        int ry = y;
        for (const std::string& line :
            wrapText(parent, localised("#{Interface:QuickKeyInstructions}"), w, sFontSmall, 3))
        {
            addText(parent, x, ry, w, 22, escaped(line), sFontSmall, sTextColour);
            ry += 22;
        }
        ry += 12;

        // Each direction's press slot, with its hold slot below it.
        constexpr int rowH = 32;
        constexpr int labelW = 64;
        const int current = targetSlot();
        for (int direction = 0; direction < static_cast<int>(sDirections.size()); ++direction)
        {
            if (ry + 2 * rowH > y + h)
                break;
            if (mState == State::Pressed && mDirection == direction)
                addRect(parent, x - 6, ry - 2, w + 12, 2 * rowH + 4, sAccentColour, 0.16f);
            for (const bool held : { false, true })
            {
                const int slot = slotFor(direction, held);
                addPointerTarget(parent, x - 6, ry, w + 12, rowH, nullptr, [this, slot] { mClickedSlot = slot; });
                drawQuickKeyMarker(parent, x, ry + 6, 4, slot);
                addText(parent, x + 30, ry, labelW, rowH,
                    held ? "#{Interface:QuickKeyHold}" : "#{Interface:QuickKeyPress}", sFontSmall, sDimColour);
                const QuickKeysMenu::Slot contents = quickKeys().getSlot(slot);
                const int ix = x + 30 + labelW;
                if (contents.mType == ESM::QuickKeys::Type::Unassigned)
                    addText(parent, ix, ry, x + w - ix, rowH, "#{Interface:QuickKeyEmpty}", sFontNormal, sDimColour);
                else
                {
                    if (!contents.mItem.isEmpty())
                        addIcon(parent, ix, ry + 3, rowH - 6, contents.mItem, false);
                    else
                        addImage(parent, ix, ry + 3, rowH - 6, contents.mIcon);
                    const int tx = ix + rowH + 4;
                    addText(parent, tx, ry, x + w - tx, rowH, escaped(contents.mName), sFontNormal,
                        slot == current ? sActiveColour : sBrightColour);
                }
                ry += rowH;
            }
            ry += 8;
        }
    }

    int dpadSlotOf(const MWWorld::Ptr& item, const ESM::RefId& spell)
    {
        QuickKeysMenu* menu = MWBase::Environment::get().getWindowManager()->getQuickKeysMenu();
        const int slot = !item.isEmpty() ? menu->findItemSlot(item) : menu->findSpellSlot(spell);
        const int dpadSlots = 2 * static_cast<int>(sDirections.size());
        return slot >= 1 && slot <= dpadSlots ? slot : -1;
    }

    void drawQuickKeyMarker(MyGUI::Widget* parent, int x, int y, int cell, int slot)
    {
        // Steps from the middle cell toward each direction in sDirections: up, right, down and left.
        constexpr std::array<std::pair<int, int>, 4> steps = { { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 } } };
        const int litDirection = (slot - 1) % static_cast<int>(sDirections.size());
        const bool held = slot > static_cast<int>(sDirections.size());
        addRect(parent, x + 2 * cell, y + 2 * cell, cell, cell, sDimColour, 0.6f);
        for (int direction = 0; direction < static_cast<int>(steps.size()); ++direction)
        {
            for (int distance = 1; distance <= 2; ++distance)
            {
                const bool lit = direction == litDirection && (distance == 1 || held);
                const int cx = x + (2 + steps[direction].first * distance) * cell;
                const int cy = y + (2 + steps[direction].second * distance) * cell;
                addRect(parent, cx, cy, cell, cell, lit ? sActiveColour : sDimColour, lit ? 1.f : 0.6f);
            }
        }
    }
}
