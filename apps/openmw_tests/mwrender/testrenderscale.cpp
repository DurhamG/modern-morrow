#include "apps/openmw/mwrender/renderscale.hpp"

#include <gtest/gtest.h>

namespace
{
    using namespace MWRender;

    TEST(MWRenderScaledRenderSizeTest, fullScaleKeepsTheScreenSize)
    {
        EXPECT_EQ(scaledRenderSize(2400, 1.f), 2400);
        EXPECT_EQ(scaledRenderSize(1080, 1.f), 1080);
    }

    TEST(MWRenderScaledRenderSizeTest, halfScaleHalvesEachSide)
    {
        EXPECT_EQ(scaledRenderSize(2400, 0.5f), 1200);
        EXPECT_EQ(scaledRenderSize(1080, 0.5f), 540);
    }

    TEST(MWRenderScaledRenderSizeTest, roundsToTheNearestPixel)
    {
        EXPECT_EQ(scaledRenderSize(1081, 0.5f), 541);
        EXPECT_EQ(scaledRenderSize(1000, 0.333f), 333);
        EXPECT_EQ(scaledRenderSize(1000, 0.3336f), 334);
    }

    TEST(MWRenderScaledRenderSizeTest, keepsTheScaleBetweenZeroAndOne)
    {
        EXPECT_EQ(scaledRenderSize(1080, 2.f), 1080);
        EXPECT_EQ(scaledRenderSize(1080, -1.f), 1);
    }

    TEST(MWRenderScaledRenderSizeTest, isAtLeastOnePixel)
    {
        EXPECT_EQ(scaledRenderSize(1, 0.25f), 1);
        EXPECT_EQ(scaledRenderSize(0, 1.f), 1);
    }
}
