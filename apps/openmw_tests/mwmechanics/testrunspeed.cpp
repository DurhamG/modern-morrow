#include "apps/openmw/mwmechanics/runspeed.hpp"

#include <gtest/gtest.h>

namespace MWMechanics
{
    namespace
    {
        TEST(MWMechanicsRunSpeedTest, theSlowestRunnerShouldRunHalfAgainAsFast)
        {
            // Speed 0, no Athletics and nothing carried: 100 * 1.75.
            EXPECT_FLOAT_EQ(runSpeedWithMinimum(175, 1, 100, 1.75f), 262.5f);
        }

        TEST(MWMechanicsRunSpeedTest, aLoadShouldSlowThePlayerByTheSameShare)
        {
            // A full load takes away 30% in Morrowind, from the raised speed here.
            EXPECT_FLOAT_EQ(runSpeedWithMinimum(175, 0.7f, 100, 1.75f), 183.75f);
            EXPECT_FLOAT_EQ(runSpeedWithMinimum(400, 0.7f, 100, 1.75f), 280);
        }

        TEST(MWMechanicsRunSpeedTest, fasterRunnersShouldBeUnchanged)
        {
            // Speed 100 and Athletics 100: 200 * 2.75.
            EXPECT_FLOAT_EQ(runSpeedWithMinimum(550, 1, 100, 1.75f), 550);
            EXPECT_FLOAT_EQ(runSpeedWithMinimum(270, 1, 100, 1.75f), 270);
        }

        TEST(MWMechanicsRunSpeedTest, theMinimumShouldFollowTheGameSettings)
        {
            EXPECT_FLOAT_EQ(runSpeedWithMinimum(0, 1, 120, 2), 360);
        }
    }
}
