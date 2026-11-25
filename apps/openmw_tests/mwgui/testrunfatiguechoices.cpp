#include "apps/openmw/mwgui/runfatiguechoices.hpp"

#include <gtest/gtest.h>

namespace
{
    using namespace MWGui;

    TEST(MWGuiRunFatigueChoicesTest, aValueShouldShowAsTheNearestChoice)
    {
        EXPECT_EQ(nearestRunFatigueChoice(1.f), 0u);
        EXPECT_EQ(nearestRunFatigueChoice(0.5f), 1u);
        EXPECT_EQ(nearestRunFatigueChoice(0.3f), 2u);
        EXPECT_EQ(nearestRunFatigueChoice(0.05f), 3u);
    }

    TEST(MWGuiRunFatigueChoicesTest, steppingShouldMoveAlongTheChoices)
    {
        EXPECT_FLOAT_EQ(stepRunFatigueChoice(1.f, 1, false), 0.5f);
        EXPECT_FLOAT_EQ(stepRunFatigueChoice(0.5f, 1, false), 0.25f);
        EXPECT_FLOAT_EQ(stepRunFatigueChoice(0.25f, -1, false), 0.5f);
        // A value set by hand steps from its nearest choice.
        EXPECT_FLOAT_EQ(stepRunFatigueChoice(0.3f, 1, false), 0.f);
    }

    TEST(MWGuiRunFatigueChoicesTest, steppingPastAnEndShouldStopOrWrap)
    {
        EXPECT_FLOAT_EQ(stepRunFatigueChoice(0.f, 1, false), 0.f);
        EXPECT_FLOAT_EQ(stepRunFatigueChoice(1.f, -1, false), 1.f);
        EXPECT_FLOAT_EQ(stepRunFatigueChoice(0.f, 1, true), 1.f);
        EXPECT_FLOAT_EQ(stepRunFatigueChoice(1.f, -1, true), 0.f);
    }
}
