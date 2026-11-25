#include "apps/openmw/mwinput/touchgestures.hpp"

#include <gtest/gtest.h>

namespace MWInput::TouchGestures
{
    namespace
    {
        // A screen at 10 pixels to the millimetre, so distances in the tests read as millimetres times ten.
        constexpr float sMm = 10.f;
        const MyGUI::IntSize sScreen(2000, 1000);

        MyGUI::FloatPoint at(float x, float y)
        {
            return MyGUI::FloatPoint(x, y);
        }

        TEST(MWInputTouchGesturesTest, pixelsPerMmShouldComeFromTheDpi)
        {
            EXPECT_FLOAT_EQ(pixelsPerMm(254, 1000, 1000), 10);
        }

        TEST(MWInputTouchGesturesTest, pixelsPerMmShouldScaleWithTheDrawable)
        {
            EXPECT_FLOAT_EQ(pixelsPerMm(254, 2000, 1000), 20);
            EXPECT_FLOAT_EQ(pixelsPerMm(254, 2000, 0), 10);
        }

        TEST(MWInputTouchGesturesTest, unknownDpiShouldFallBackTo160)
        {
            EXPECT_FLOAT_EQ(pixelsPerMm(0, 1000, 1000), 160 / 25.4f);
            EXPECT_FLOAT_EQ(pixelsPerMm(-1, 1000, 1000), 160 / 25.4f);
        }

        TEST(MWInputTouchGesturesTest, topLeftCornerShouldQuickSave)
        {
            EXPECT_EQ(zoneAt(at(119, 119), sScreen, sMm), Zone::QuickSave);
            EXPECT_EQ(zoneAt(at(121, 119), sScreen, sMm), Zone::Move);
            EXPECT_EQ(zoneAt(at(119, 121), sScreen, sMm), Zone::Move);
        }

        TEST(MWInputTouchGesturesTest, topRightCornerShouldQuickLoad)
        {
            EXPECT_EQ(zoneAt(at(1881, 119), sScreen, sMm), Zone::QuickLoad);
            EXPECT_EQ(zoneAt(at(1879, 119), sScreen, sMm), Zone::Look);
        }

        TEST(MWInputTouchGesturesTest, bottomRightCornerShouldSneak)
        {
            EXPECT_EQ(zoneAt(at(1881, 881), sScreen, sMm), Zone::Sneak);
            EXPECT_EQ(zoneAt(at(1881, 879), sScreen, sMm), Zone::Look);
        }

        TEST(MWInputTouchGesturesTest, bottomLeftCornerShouldMove)
        {
            EXPECT_EQ(zoneAt(at(50, 950), sScreen, sMm), Zone::Move);
        }

        TEST(MWInputTouchGesturesTest, leftHalfShouldMoveAndRightHalfShouldLook)
        {
            EXPECT_EQ(zoneAt(at(999, 500), sScreen, sMm), Zone::Move);
            EXPECT_EQ(zoneAt(at(1000, 500), sScreen, sMm), Zone::Look);
            EXPECT_EQ(zoneAt(at(1500, 500), sScreen, sMm), Zone::Look);
        }

        TEST(MWInputTouchGesturesTest, edgeBandShouldBeTheMiddleHalfOfEachSide)
        {
            EXPECT_EQ(edgeAt(at(49, 500), sScreen, sMm), -1);
            EXPECT_EQ(edgeAt(at(51, 500), sScreen, sMm), 0);
            EXPECT_EQ(edgeAt(at(1951, 500), sScreen, sMm), 1);
            EXPECT_EQ(edgeAt(at(1949, 500), sScreen, sMm), 0);
            EXPECT_EQ(edgeAt(at(10, 249), sScreen, sMm), 0);
            EXPECT_EQ(edgeAt(at(10, 251), sScreen, sMm), -1);
            EXPECT_EQ(edgeAt(at(10, 751), sScreen, sMm), 0);
        }

        TEST(MWInputTouchGesturesTest, edgeSwipeShouldWaitUntilTheFingerMoves)
        {
            EXPECT_EQ(edgeSwipe(at(10, 500), at(39, 500), -1, 29, sMm), EdgeSwipe::Undecided);
        }

        TEST(MWInputTouchGesturesTest, edgeSwipeShouldWaitFor8mmInwards)
        {
            EXPECT_EQ(edgeSwipe(at(10, 500), at(89, 500), -1, 79, sMm), EdgeSwipe::Undecided);
            EXPECT_EQ(edgeSwipe(at(10, 500), at(91, 500), -1, 81, sMm), EdgeSwipe::SwipedIn);
        }

        TEST(MWInputTouchGesturesTest, edgeSwipeInwardsFromTheRightShouldSwipeIn)
        {
            EXPECT_EQ(edgeSwipe(at(1990, 500), at(1900, 520), 1, 92, sMm), EdgeSwipe::SwipedIn);
        }

        TEST(MWInputTouchGesturesTest, edgeFingerMovingAlongTheEdgeShouldNotBeASwipe)
        {
            EXPECT_EQ(edgeSwipe(at(10, 500), at(20, 560), -1, 61, sMm), EdgeSwipe::NotASwipe);
        }

        TEST(MWInputTouchGesturesTest, edgeFingerMovingOutwardsShouldNotBeASwipe)
        {
            EXPECT_EQ(edgeSwipe(at(40, 500), at(0, 500), -1, 40, sMm), EdgeSwipe::NotASwipe);
        }

        TEST(MWInputTouchGesturesTest, quickStillTouchShouldBeATap)
        {
            EXPECT_TRUE(isTap(1000, 1250, 29, sMm));
        }

        TEST(MWInputTouchGesturesTest, slowTouchShouldNotBeATap)
        {
            EXPECT_FALSE(isTap(1000, 1251, 0, sMm));
        }

        TEST(MWInputTouchGesturesTest, touchThatMoved3mmShouldNotBeATap)
        {
            // Farthest counts how far it went, even if it came back to where it started.
            EXPECT_FALSE(isTap(1000, 1100, 30, sMm));
        }

        TEST(MWInputTouchGesturesTest, quickMoveUp10mmShouldBeAFlick)
        {
            EXPECT_TRUE(isFlickUp(at(500, 800), at(500, 700), 1000, 1300, sMm));
            EXPECT_TRUE(isFlickUp(at(500, 800), at(560, 650), 1000, 1150, sMm));
        }

        TEST(MWInputTouchGesturesTest, slowMoveUpShouldNotBeAFlick)
        {
            EXPECT_FALSE(isFlickUp(at(500, 800), at(500, 600), 1000, 1301, sMm));
        }

        TEST(MWInputTouchGesturesTest, shortMoveUpShouldNotBeAFlick)
        {
            EXPECT_FALSE(isFlickUp(at(500, 800), at(500, 701), 1000, 1100, sMm));
        }

        TEST(MWInputTouchGesturesTest, flickMoreSidewaysThanUpShouldNotCount)
        {
            EXPECT_FALSE(isFlickUp(at(500, 800), at(650, 680), 1000, 1100, sMm));
        }

        TEST(MWInputTouchGesturesTest, flickDownShouldNotCount)
        {
            EXPECT_FALSE(isFlickUp(at(500, 700), at(500, 850), 1000, 1100, sMm));
        }

        TEST(MWInputTouchGesturesTest, movementInsideTheDeadZoneShouldBeNone)
        {
            const Movement moved = movement(at(500, 500), at(510, 490), sMm);
            EXPECT_FLOAT_EQ(moved.mForwardBackward, 0.5f);
            EXPECT_FLOAT_EQ(moved.mLeftRight, 0.5f);
        }

        TEST(MWInputTouchGesturesTest, movementAt15mmShouldBeFull)
        {
            const Movement up = movement(at(500, 500), at(500, 350), sMm);
            EXPECT_FLOAT_EQ(up.mForwardBackward, 0);
            EXPECT_FLOAT_EQ(up.mLeftRight, 0.5f);
            const Movement right = movement(at(500, 500), at(650, 500), sMm);
            EXPECT_FLOAT_EQ(right.mLeftRight, 1);
        }

        TEST(MWInputTouchGesturesTest, movementPast15mmShouldStayFull)
        {
            EXPECT_FLOAT_EQ(movement(at(500, 500), at(500, 200), sMm).mForwardBackward, 0);
            EXPECT_FLOAT_EQ(movement(at(500, 500), at(500, 900), sMm).mForwardBackward, 1);
        }

        TEST(MWInputTouchGesturesTest, movementHalfwayShouldBeProportional)
        {
            // Half way from the dead zone's edge to full speed, 8.5 mm up.
            EXPECT_NEAR(movement(at(500, 500), at(500, 415), sMm).mForwardBackward, 0.25f, 1e-5f);
        }

        TEST(MWInputTouchGesturesTest, diagonalMovementShouldKeepItsDirection)
        {
            const Movement moved = movement(at(500, 500), at(600, 400), sMm);
            EXPECT_LT(moved.mForwardBackward, 0.5f);
            EXPECT_GT(moved.mLeftRight, 0.5f);
            EXPECT_NEAR(0.5f - moved.mForwardBackward, moved.mLeftRight - 0.5f, 1e-5f);
        }

        TEST(MWInputTouchGesturesTest, lookInsideTheCircleShouldTurnWithTheFinger)
        {
            const Turn turned = lookTurn(at(500, 500), at(500, 500), at(510, 500), sMm);
            EXPECT_FLOAT_EQ(turned.mYaw, 0.026f);
            EXPECT_FLOAT_EQ(turned.mPitch, 0);
        }

        TEST(MWInputTouchGesturesTest, lookPastTheCircleShouldNotTurnDirectly)
        {
            EXPECT_FLOAT_EQ(lookTurn(at(500, 500), at(620, 500), at(650, 500), sMm).mYaw, 0);
            EXPECT_FLOAT_EQ(lookTurn(at(500, 500), at(380, 500), at(350, 500), sMm).mYaw, 0);
        }

        TEST(MWInputTouchGesturesTest, lookCrossingTheCirclesEdgeShouldTurnOnlyForThePartInside)
        {
            EXPECT_FLOAT_EQ(lookTurn(at(500, 500), at(590, 500), at(620, 500), sMm).mYaw, 0.026f);
        }

        TEST(MWInputTouchGesturesTest, pitchShouldNotBeLimitedByTheCircle)
        {
            EXPECT_FLOAT_EQ(lookTurn(at(500, 500), at(500, 500), at(500, 800), sMm).mPitch, 30 * 0.026f);
        }

        TEST(MWInputTouchGesturesTest, turnSpeedShouldBeZeroInsideTheCircle)
        {
            EXPECT_FLOAT_EQ(turnSpeed(500, 590, sMm), 0);
            EXPECT_FLOAT_EQ(turnSpeed(500, 600, sMm), 0);
        }

        TEST(MWInputTouchGesturesTest, turnSpeedShouldGrowOver20mm)
        {
            EXPECT_FLOAT_EQ(turnSpeed(500, 700, sMm), sLookFullSpeed / 2);
        }

        TEST(MWInputTouchGesturesTest, turnSpeedShouldStopGrowingAtFullSpeed)
        {
            EXPECT_FLOAT_EQ(turnSpeed(500, 800, sMm), sLookFullSpeed);
            EXPECT_FLOAT_EQ(turnSpeed(500, 1200, sMm), sLookFullSpeed);
            EXPECT_NEAR(sLookFullSpeed, 2.2689f, 1e-4f);
        }

        TEST(MWInputTouchGesturesTest, turnSpeedShouldTakeTheFingersSide)
        {
            EXPECT_FLOAT_EQ(turnSpeed(500, 300, sMm), -sLookFullSpeed / 2);
        }

        TEST(MWInputTouchGesturesTest, tabsShouldSwitchEvery12mm)
        {
            EXPECT_EQ(tabsSwiped(500, 610, sMm), 0);
            EXPECT_EQ(tabsSwiped(500, 620, sMm), 1);
            EXPECT_EQ(tabsSwiped(500, 750, sMm), 2);
        }

        TEST(MWInputTouchGesturesTest, tabsShouldSwitchBackWhenTheFingerReturns)
        {
            EXPECT_EQ(tabsSwiped(500, 380, sMm), -1);
            EXPECT_EQ(tabsSwiped(500, 390, sMm), 0);
        }

        TEST(MWInputTouchGesturesTest, scrollShouldStepEvery6mm)
        {
            EXPECT_EQ(scrollSteps(500, 450, sMm), 0);
            EXPECT_EQ(scrollSteps(500, 440, sMm), 1);
            EXPECT_EQ(scrollSteps(500, 380, sMm), 2);
            EXPECT_EQ(scrollSteps(500, 560, sMm), -1);
        }

        TEST(MWInputTouchGesturesTest, pickerShouldStartOnUse)
        {
            EXPECT_EQ(pickerItem(at(500, 500), at(500, 500), sMm), 0);
            EXPECT_EQ(pickerItem(at(500, 500), at(550, 530), sMm), 0);
        }

        TEST(MWInputTouchGesturesTest, pickerShouldChooseTheNearestItem)
        {
            EXPECT_EQ(pickerItem(at(500, 500), at(310, 500), sMm), 1);
            EXPECT_EQ(pickerItem(at(500, 500), at(120, 500), sMm), 2);
            EXPECT_EQ(pickerItem(at(500, 500), at(500, 370), sMm), 3);
            EXPECT_EQ(pickerItem(at(500, 500), at(500, 630), sMm), 4);
        }
    }
}
