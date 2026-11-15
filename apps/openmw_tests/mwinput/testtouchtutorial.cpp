#include "apps/openmw/mwinput/touchtutorial.hpp"

#include <components/testing/util.hpp>

#include <gtest/gtest.h>

#include <fstream>

namespace MWInput
{
    namespace
    {
        constexpr std::int64_t sNow = 1800000000;
        constexpr std::int64_t sDay = 24 * 60 * 60;

        std::filesystem::path recordFile()
        {
            return TestingOpenMW::currentTestDirPath() / "touch-tutorial.txt";
        }

        TEST(MWInputTouchTutorialTest, missingFileShouldReadAsNeverPlayed)
        {
            const TutorialRecord record = readTutorialRecord(recordFile());
            EXPECT_EQ(record.mLastPlayed, 0);
            EXPECT_TRUE(record.mCharacters.empty());
        }

        TEST(MWInputTouchTutorialTest, recordShouldRoundTrip)
        {
            TutorialRecord record;
            record.mLastPlayed = sNow;
            record.mCharacters = { "Lendias", "Vickor" };
            writeTutorialRecord(recordFile(), record);
            const TutorialRecord read = readTutorialRecord(recordFile());
            EXPECT_EQ(read.mLastPlayed, sNow);
            EXPECT_EQ(read.mCharacters, record.mCharacters);
        }

        TEST(MWInputTouchTutorialTest, characterFolderWithSpacesShouldRoundTrip)
        {
            TutorialRecord record;
            record.mCharacters = { "J. Doe - 1" };
            writeTutorialRecord(recordFile(), record);
            EXPECT_EQ(readTutorialRecord(recordFile()).mCharacters, record.mCharacters);
        }

        TEST(MWInputTouchTutorialTest, unknownLinesShouldBeIgnored)
        {
            {
                std::ofstream file(recordFile());
                file << "version 2\nplayed 1234\n\nseen Lendias\nsomething else\n";
            }
            const TutorialRecord record = readTutorialRecord(recordFile());
            EXPECT_EQ(record.mLastPlayed, 1234);
            EXPECT_EQ(record.mCharacters, std::set<std::string>{ "Lendias" });
        }

        TEST(MWInputTouchTutorialTest, newGameShouldShowTheTutorial)
        {
            TutorialRecord record;
            record.mLastPlayed = sNow - sDay;
            record.mCharacters = { "Lendias" };
            EXPECT_TRUE(tutorialDue(record, "", sNow));
        }

        TEST(MWInputTouchTutorialTest, characterThatHasNotSeenItShouldShowIt)
        {
            TutorialRecord record;
            record.mLastPlayed = sNow - sDay;
            record.mCharacters = { "Lendias" };
            EXPECT_TRUE(tutorialDue(record, "Vickor", sNow));
        }

        TEST(MWInputTouchTutorialTest, characterThatHasSeenItShouldNotShowIt)
        {
            TutorialRecord record;
            record.mLastPlayed = sNow - sDay;
            record.mCharacters = { "Lendias" };
            EXPECT_FALSE(tutorialDue(record, "Lendias", sNow));
        }

        TEST(MWInputTouchTutorialTest, fourteenDaysAwayShouldShowItAgain)
        {
            TutorialRecord record;
            record.mCharacters = { "Lendias" };
            record.mLastPlayed = sNow - 14 * sDay;
            EXPECT_TRUE(tutorialDue(record, "Lendias", sNow));
            record.mLastPlayed = sNow - 14 * sDay + 1;
            EXPECT_FALSE(tutorialDue(record, "Lendias", sNow));
        }

        TEST(MWInputTouchTutorialTest, firstEverPlayShouldNotCountAsAway)
        {
            TutorialRecord record;
            record.mCharacters = { "Lendias" };
            EXPECT_FALSE(tutorialDue(record, "Lendias", sNow));
        }
    }
}
