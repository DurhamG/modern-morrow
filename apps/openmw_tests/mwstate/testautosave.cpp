#include "apps/openmw/mwstate/autosave.hpp"

#include <gtest/gtest.h>

namespace MWState
{
    namespace
    {
        TEST(MWStateAutosaveTest, clockShouldRunDuringPlay)
        {
            EXPECT_TRUE(autosaveClockRuns(10, true, false));
        }

        TEST(MWStateAutosaveTest, clockShouldStopInMenus)
        {
            EXPECT_FALSE(autosaveClockRuns(10, true, true));
        }

        TEST(MWStateAutosaveTest, clockShouldStopWithoutAGame)
        {
            EXPECT_FALSE(autosaveClockRuns(10, false, false));
        }

        TEST(MWStateAutosaveTest, clockShouldStopWhenTheAutosaveIsOff)
        {
            EXPECT_FALSE(autosaveClockRuns(0, true, false));
            EXPECT_FALSE(autosaveClockRuns(-1, true, false));
        }

        TEST(MWStateAutosaveTest, autosaveShouldBeDueAfterTheIntervalInMinutes)
        {
            EXPECT_FALSE(autosaveDue(599.9, 10));
            EXPECT_TRUE(autosaveDue(600, 10));
            EXPECT_TRUE(autosaveDue(601, 10));
            EXPECT_TRUE(autosaveDue(60, 1));
        }

        TEST(MWStateAutosaveTest, autosaveShouldSaveDuringNormalPlay)
        {
            EXPECT_TRUE(canAutosave(true, true, false, false));
        }

        TEST(MWStateAutosaveTest, autosaveShouldWaitDuringCharacterCreation)
        {
            EXPECT_FALSE(canAutosave(false, true, false, false));
        }

        TEST(MWStateAutosaveTest, autosaveShouldWaitWhenSavingIsNotAllowed)
        {
            EXPECT_FALSE(canAutosave(true, false, false, false));
        }

        TEST(MWStateAutosaveTest, autosaveShouldNotSaveADeadPlayer)
        {
            EXPECT_FALSE(canAutosave(true, true, true, false));
        }

        TEST(MWStateAutosaveTest, autosaveShouldWaitDuringCombat)
        {
            EXPECT_FALSE(canAutosave(true, true, false, true));
        }
    }
}
