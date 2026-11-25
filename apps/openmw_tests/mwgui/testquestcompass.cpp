#include "apps/openmw/mwgui/questcompass.hpp"

#include <gtest/gtest.h>

namespace
{
    using namespace MWGui::Compass;

    TEST(MWGuiCompassBearingTest, northIsPlusYAndEastIsPlusX)
    {
        const osg::Vec3f from(100, 100, 0);
        EXPECT_FLOAT_EQ(bearing(from, osg::Vec3f(100, 200, 0)), 0.f);
        EXPECT_FLOAT_EQ(bearing(from, osg::Vec3f(200, 100, 0)), 90.f);
        EXPECT_FLOAT_EQ(bearing(from, osg::Vec3f(100, 0, 0)), 180.f);
        EXPECT_FLOAT_EQ(bearing(from, osg::Vec3f(0, 100, 50)), 270.f);
    }

    TEST(MWGuiCompassPlaceTest, theHeadingIsInTheMiddle)
    {
        const Place where = place(30.f, 30.f, 90.f);
        EXPECT_FLOAT_EQ(where.mPosition, 0.f);
        EXPECT_FALSE(where.mPinned);
    }

    TEST(MWGuiCompassPlaceTest, directionsToTheRightAreRightOfTheMiddle)
    {
        EXPECT_FLOAT_EQ(place(45.f, 0.f, 90.f).mPosition, 0.5f);
        EXPECT_FLOAT_EQ(place(315.f, 0.f, 90.f).mPosition, -0.5f);
    }

    TEST(MWGuiCompassPlaceTest, wrapsAroundNorth)
    {
        EXPECT_FLOAT_EQ(place(10.f, 350.f, 90.f).mPosition, 20.f / 90.f);
        EXPECT_FLOAT_EQ(place(350.f, 10.f, 90.f).mPosition, -20.f / 90.f);
    }

    TEST(MWGuiCompassPlaceTest, directionsPastTheEndsStayAtTheNearerEnd)
    {
        const Place right = place(100.f, 0.f, 90.f);
        EXPECT_FLOAT_EQ(right.mPosition, 1.f);
        EXPECT_TRUE(right.mPinned);
        const Place left = place(200.f, 0.f, 90.f);
        EXPECT_FLOAT_EQ(left.mPosition, -1.f);
        EXPECT_TRUE(left.mPinned);
    }

    TEST(MWGuiCompassLevelTest, aTargetWithinAStoreyIsLevel)
    {
        EXPECT_EQ(level(0.f, Level::Same), Level::Same);
        EXPECT_EQ(level(200.f, Level::Same), Level::Same);
        EXPECT_EQ(level(-200.f, Level::Same), Level::Same);
    }

    TEST(MWGuiCompassLevelTest, aTargetMoreThanAStoreyAwayIsAboveOrBelow)
    {
        EXPECT_EQ(level(300.f, Level::Same), Level::Above);
        EXPECT_EQ(level(-300.f, Level::Same), Level::Below);
    }

    // Walking on uneven ground moves the player up and down a little. A target near a storey away shouldn't make the
    // arrow come and go with each step, as it did north of the South Wall Cornerclub in Balmora.
    TEST(MWGuiCompassLevelTest, anArrowStaysUntilTheTargetIsWellWithinAStorey)
    {
        EXPECT_EQ(level(250.f, Level::Above), Level::Above);
        EXPECT_EQ(level(-250.f, Level::Below), Level::Below);
        EXPECT_EQ(level(150.f, Level::Above), Level::Same);
        EXPECT_EQ(level(-150.f, Level::Below), Level::Same);
    }

    TEST(MWGuiCompassLevelTest, noArrowUntilTheTargetIsMoreThanAStoreyAway)
    {
        EXPECT_EQ(level(250.f, Level::Same), Level::Same);
        EXPECT_EQ(level(-250.f, Level::Same), Level::Same);
    }

    TEST(MWGuiCompassLevelTest, anArrowTurnsOverAtOnce)
    {
        EXPECT_EQ(level(-300.f, Level::Above), Level::Below);
        EXPECT_EQ(level(300.f, Level::Below), Level::Above);
    }
}
