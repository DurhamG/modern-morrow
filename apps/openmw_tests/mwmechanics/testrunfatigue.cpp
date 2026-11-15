#include "apps/openmw/mwmechanics/runfatigue.hpp"

#include <gtest/gtest.h>

namespace MWMechanics
{
    namespace
    {
        TEST(MWMechanicsRunFatigueTest, fullScaleShouldKeepMorrowindsLoss)
        {
            EXPECT_FLOAT_EQ(scaleRunFatigueLoss(6, 4, 1), 6);
        }

        TEST(MWMechanicsRunFatigueTest, zeroScaleShouldLoseOnlyWhatComesBack)
        {
            EXPECT_FLOAT_EQ(scaleRunFatigueLoss(6, 4, 0), 4);
        }

        TEST(MWMechanicsRunFatigueTest, halfScaleShouldHalveTheLossBeyondRecovery)
        {
            EXPECT_FLOAT_EQ(scaleRunFatigueLoss(6, 4, 0.5f), 5);
        }

        TEST(MWMechanicsRunFatigueTest, lossBelowRecoveryShouldBeUnchanged)
        {
            EXPECT_FLOAT_EQ(scaleRunFatigueLoss(3, 4, 0), 3);
            EXPECT_FLOAT_EQ(scaleRunFatigueLoss(3, 4, 0.5f), 3);
            EXPECT_FLOAT_EQ(scaleRunFatigueLoss(4, 4, 0), 4);
        }

        TEST(MWMechanicsRunFatigueTest, noRecoveryShouldScaleTheWholeLoss)
        {
            // Full fatigue recovers nothing.
            EXPECT_FLOAT_EQ(scaleRunFatigueLoss(6, 0, 0.5f), 3);
            EXPECT_FLOAT_EQ(scaleRunFatigueLoss(6, 0, 0), 0);
        }
    }
}
