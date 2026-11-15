#include "apps/openmw/mwgui/quickkeyslots.hpp"

#include <gtest/gtest.h>

namespace MWGui::QuickKeySlots
{
    namespace
    {
        constexpr int sUp = 0;
        constexpr int sLeft = 3;
        constexpr float sFrame = 0.05f;

        // Runs frames of a direction held down, and returns the slots used, in order.
        std::vector<int> hold(DpadPress& press, int direction, int frames, bool playing = true, bool holdAssigned = true)
        {
            std::vector<int> used;
            for (int i = 0; i < frames; ++i)
                if (const int slot = updatePress(press, direction, true, playing, [=] { return holdAssigned; }, sFrame))
                    used.push_back(slot);
            return used;
        }

        int release(DpadPress& press, int direction, bool playing = true)
        {
            return updatePress(press, direction, false, playing, [] { return true; }, sFrame);
        }

        TEST(MWGuiQuickKeySlotsTest, pressesShouldUseSlots1To4AndHoldsSlots5To8)
        {
            EXPECT_EQ(slotFor(0, false), 1);
            EXPECT_EQ(slotFor(3, false), 4);
            EXPECT_EQ(slotFor(0, true), 5);
            EXPECT_EQ(slotFor(3, true), 8);
        }

        TEST(MWGuiQuickKeySlotsTest, directionOfShouldFindOnlyTheDpad)
        {
            EXPECT_EQ(directionOf(SDL_CONTROLLER_BUTTON_DPAD_UP), 0);
            EXPECT_EQ(directionOf(SDL_CONTROLLER_BUTTON_DPAD_RIGHT), 1);
            EXPECT_EQ(directionOf(SDL_CONTROLLER_BUTTON_DPAD_DOWN), 2);
            EXPECT_EQ(directionOf(SDL_CONTROLLER_BUTTON_DPAD_LEFT), 3);
            EXPECT_EQ(directionOf(SDL_CONTROLLER_BUTTON_A), -1);
        }

        TEST(MWGuiQuickKeySlotsTest, pressWithNothingOnTheHoldShouldActAtOnce)
        {
            DpadPress press;
            EXPECT_EQ(hold(press, sUp, 1, true, false), std::vector<int>{ 1 });
            EXPECT_TRUE(hold(press, sUp, 20, true, false).empty());
            EXPECT_EQ(release(press, sUp), 0);
        }

        TEST(MWGuiQuickKeySlotsTest, pressWithAHoldAssignedShouldWaitForTheRelease)
        {
            DpadPress press;
            EXPECT_TRUE(hold(press, sUp, 3).empty());
        }

        TEST(MWGuiQuickKeySlotsTest, shortPressShouldUseThePressSlotOnRelease)
        {
            DpadPress press;
            hold(press, sLeft, 3);
            EXPECT_EQ(release(press, sLeft), 4);
        }

        TEST(MWGuiQuickKeySlotsTest, holdShouldUseTheHoldSlotAfter350msOnlyOnce)
        {
            DpadPress press;
            // The first frame starts the press, and the time counts from the next. 0.35 seconds is 7 frames of 0.05.
            EXPECT_TRUE(hold(press, sUp, 7).empty());
            EXPECT_EQ(hold(press, sUp, 1), std::vector<int>{ 5 });
            EXPECT_TRUE(hold(press, sUp, 20).empty());
        }

        TEST(MWGuiQuickKeySlotsTest, releaseAfterAHoldShouldDoNothing)
        {
            DpadPress press;
            hold(press, sUp, 10);
            EXPECT_EQ(release(press, sUp), 0);
        }

        TEST(MWGuiQuickKeySlotsTest, pressThatStartsInAMenuShouldDoNothing)
        {
            DpadPress press;
            EXPECT_TRUE(hold(press, sUp, 1, false).empty());
            // The menu closes while the direction is still down.
            EXPECT_TRUE(hold(press, sUp, 20).empty());
            EXPECT_EQ(release(press, sUp), 0);
        }

        TEST(MWGuiQuickKeySlotsTest, menuOpeningDuringAPressShouldCancelIt)
        {
            DpadPress press;
            hold(press, sUp, 2);
            EXPECT_TRUE(hold(press, sUp, 1, false).empty());
            EXPECT_TRUE(hold(press, sUp, 20).empty());
            EXPECT_EQ(release(press, sUp), 0);
        }

        TEST(MWGuiQuickKeySlotsTest, releaseShouldStartTheNextPressAfresh)
        {
            DpadPress press;
            hold(press, sUp, 10);
            release(press, sUp);
            hold(press, sUp, 2);
            EXPECT_EQ(release(press, sUp), 1);
        }

        TEST(MWGuiQuickKeySlotsTest, holdSlotShouldNotBeLookedUpInAMenu)
        {
            DpadPress press;
            bool asked = false;
            updatePress(press, sUp, true, false, [&] { return asked = true; }, sFrame);
            EXPECT_FALSE(asked);
        }
    }
}
