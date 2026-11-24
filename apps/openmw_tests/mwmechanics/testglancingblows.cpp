#include "apps/openmw/mwmechanics/glancingblows.hpp"

#include <gtest/gtest.h>

namespace MWMechanics
{
    namespace
    {
        TEST(MWMechanicsGlancingBlowsTest, thePlayersMissShouldLandWithTheSetting)
        {
            EXPECT_TRUE(isGlancingBlow(false, true, true));
        }

        TEST(MWMechanicsGlancingBlowsTest, thePlayersMissShouldMissWithoutTheSetting)
        {
            EXPECT_FALSE(isGlancingBlow(false, true, false));
        }

        TEST(MWMechanicsGlancingBlowsTest, otherActorsShouldStillMiss)
        {
            EXPECT_FALSE(isGlancingBlow(false, false, true));
        }

        TEST(MWMechanicsGlancingBlowsTest, aHitShouldNotBeAGlancingBlow)
        {
            EXPECT_FALSE(isGlancingBlow(true, true, true));
        }

        TEST(MWMechanicsGlancingBlowsTest, aGlancingBlowShouldDoATenthOfTheDamage)
        {
            EXPECT_FLOAT_EQ(sGlancingBlowDamageShare, 0.1f);
        }
    }
}
