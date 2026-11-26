#include "apps/openmw/mwdialogue/lockouts.hpp"

#include <gtest/gtest.h>

namespace MWDialogue::Lockouts
{
    namespace
    {
        ESM::RefId id(std::string_view text)
        {
            return ESM::RefId::stringRefId(text);
        }

        const Data sData = parse(R"(
factionGroups:
  - factions: [Hlaalu, Redoran, Telvanni]
    warning: LockoutGreatHouses
questStages:
  - { quest: FG_KillBosses, stage: 10, warning: LockoutKillThievesBosses }
)");

        bool memberOfNone(const ESM::RefId&)
        {
            return false;
        }

        int notStarted(const ESM::RefId&)
        {
            return 0;
        }

        TEST(MWDialogueLockoutsTest, shouldReadFactionJoins)
        {
            const Effects effects = readEffects("PCJoinFaction \"Redoran\"\r\nPCJoinFaction\n");
            ASSERT_EQ(effects.mJoins.size(), 2u);
            EXPECT_EQ(effects.mJoins[0], id("redoran"));
            EXPECT_TRUE(effects.mJoins[1].empty());
        }

        TEST(MWDialogueLockoutsTest, shouldReadJournalStagesInEitherForm)
        {
            const Effects effects
                = readEffects("Journal FG_KillBosses 10\nSetJournalIndex, \"CO_Choice\", 40 ; sides with Falco\n");
            ASSERT_EQ(effects.mStages.size(), 2u);
            EXPECT_EQ(effects.mStages[0], std::make_pair(id("fg_killbosses"), 10));
            EXPECT_EQ(effects.mStages[1], std::make_pair(id("co_choice"), 40));
        }

        TEST(MWDialogueLockoutsTest, shouldReadCommandsOnAnObject)
        {
            EXPECT_EQ(readEffects("player->PCJoinFaction \"Hlaalu\"").mJoins.size(), 1u);
        }

        TEST(MWDialogueLockoutsTest, shouldIgnoreOtherCommandsAndComments)
        {
            const Effects effects = readEffects("; Journal FG_KillBosses 10\nChoice \"Join House Redoran.\" 2\n"
                                                "Set ColonySide to 1\nJournal FG_KillBosses\n");
            EXPECT_TRUE(effects.mJoins.empty());
            EXPECT_TRUE(effects.mStages.empty());
        }

        TEST(MWDialogueLockoutsTest, shouldLeaveOutIncompleteEntries)
        {
            const Data data = parse(R"(
factionGroups:
  - factions: [Hlaalu]
questStages:
  - { quest: FG_KillBosses, warning: LockoutKillThievesBosses }
)");
            EXPECT_TRUE(data.mFactionGroups.empty());
            EXPECT_TRUE(data.mQuestStages.empty());
        }

        TEST(MWDialogueLockoutsTest, joiningAGreatHouseShouldWarn)
        {
            EXPECT_EQ(warning(sData, readEffects("PCJoinFaction \"Redoran\""), {}, memberOfNone, notStarted),
                "LockoutGreatHouses");
        }

        TEST(MWDialogueLockoutsTest, joiningTheSpeakersGreatHouseShouldWarn)
        {
            EXPECT_EQ(warning(sData, readEffects("PCJoinFaction"), id("telvanni"), memberOfNone, notStarted),
                "LockoutGreatHouses");
        }

        TEST(MWDialogueLockoutsTest, joiningWhenAlreadyInAGreatHouseShouldNotWarn)
        {
            const auto inHlaalu = [](const ESM::RefId& faction) { return faction == ESM::RefId::stringRefId("hlaalu"); };
            EXPECT_FALSE(warning(sData, readEffects("PCJoinFaction \"Redoran\""), {}, inHlaalu, notStarted));
        }

        TEST(MWDialogueLockoutsTest, joiningAnotherFactionShouldNotWarn)
        {
            EXPECT_FALSE(warning(sData, readEffects("PCJoinFaction \"Mages Guild\""), {}, memberOfNone, notStarted));
        }

        TEST(MWDialogueLockoutsTest, startingAListedQuestStageShouldWarn)
        {
            EXPECT_EQ(warning(sData, readEffects("Journal FG_KillBosses 10"), {}, memberOfNone, notStarted),
                "LockoutKillThievesBosses");
        }

        TEST(MWDialogueLockoutsTest, aQuestStageAlreadyReachedShouldNotWarn)
        {
            const auto atTen = [](const ESM::RefId&) { return 10; };
            EXPECT_FALSE(warning(sData, readEffects("Journal FG_KillBosses 10"), {}, memberOfNone, atTen));
        }

        TEST(MWDialogueLockoutsTest, anotherStageOfAListedQuestShouldNotWarn)
        {
            EXPECT_FALSE(warning(sData, readEffects("Journal FG_KillBosses 100"), {}, memberOfNone, notStarted));
        }
    }
}
