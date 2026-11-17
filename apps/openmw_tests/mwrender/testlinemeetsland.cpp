#include "apps/openmw/mwrender/linemeetsland.hpp"

#include <gtest/gtest.h>

namespace
{
    using MWRender::findWhereLineMeetsLand;

    float flatAt100(float /*x*/, float /*y*/)
    {
        return 100.f;
    }

    TEST(MWRenderFindWhereLineMeetsLandTest, meetsFlatLandWhereTheLineReachesItsHeight)
    {
        const auto hit = findWhereLineMeetsLand({ 0, 0, 1000 }, { 0, 1000, 0 }, 16.f, flatAt100);
        ASSERT_TRUE(hit.has_value());
        EXPECT_NEAR(hit->y(), 900.f, 0.1f);
        EXPECT_NEAR(hit->z(), 100.f, 0.1f);
    }

    TEST(MWRenderFindWhereLineMeetsLandTest, stopsAtAHillBeforeTheLowGroundBehindIt)
    {
        // A line looking north and down meets a hill before it would reach the low ground past it.
        const auto hill = [](float /*x*/, float y) { return y >= 400.f && y <= 600.f ? 500.f : 0.f; };
        const auto hit = findWhereLineMeetsLand({ 0, 0, 1000 }, { 0, 1000, 0 }, 16.f, hill);
        ASSERT_TRUE(hit.has_value());
        EXPECT_NEAR(hit->y(), 500.f, 0.1f);
        EXPECT_NEAR(hit->z(), 500.f, 0.1f);
    }

    TEST(MWRenderFindWhereLineMeetsLandTest, findsNothingWhenTheLineStaysAboveTheLand)
    {
        EXPECT_FALSE(findWhereLineMeetsLand({ 0, 0, 1000 }, { 0, 1000, 200 }, 16.f, flatAt100).has_value());
    }

    TEST(MWRenderFindWhereLineMeetsLandTest, givesTheStartWhenItIsUnderTheLand)
    {
        const auto hit = findWhereLineMeetsLand({ 5, 6, 50 }, { 0, 1000, 0 }, 16.f, flatAt100);
        ASSERT_TRUE(hit.has_value());
        EXPECT_EQ(*hit, osg::Vec3f(5, 6, 50));
    }

    TEST(MWRenderFindWhereLineMeetsLandTest, findsTheLandAtTheVeryEndOfTheLine)
    {
        const auto hit = findWhereLineMeetsLand({ 0, 0, 1000 }, { 0, 90, 100 }, 1000.f, flatAt100);
        ASSERT_TRUE(hit.has_value());
        EXPECT_NEAR(hit->z(), 100.f, 0.1f);
    }
}
