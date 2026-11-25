#include "apps/openmw/mwgui/questreview.hpp"

#include <components/esm3/loadinfo.hpp>

#include <gtest/gtest.h>

namespace
{
    using namespace MWGui::QuestReview;
    using MWGui::QuestTargets::Index;
    using MWGui::QuestTargets::Rule;

    const ESM::RefId quest = ESM::RefId::stringRefId("A1_1_FindSpymaster");

    ESM::DialInfo line(std::string_view speaker, std::string_view result)
    {
        ESM::DialInfo info;
        info.mActor = ESM::RefId::stringRefId(speaker);
        info.mResultScript = result;
        return info;
    }

    ESM::DialInfo lineInCell(std::string_view cell, std::string_view result)
    {
        ESM::DialInfo info;
        info.mCell = ESM::RefId::stringRefId(cell);
        info.mResultScript = result;
        return info;
    }

    // The ways forward from the main quest's first stage: Elone gives directions, anyone in the South Wall
    // Cornerclub can say where Caius lives, and Caius is where the player is sent.
    Index firstQuest()
    {
        Index index;
        index.addDialogue(line("elone", "Journal A1_1_FindSpymaster 2"));
        index.addDialogue(lineInCell("Balmora, South Wall Cornerclub", "Journal A1_1_FindSpymaster 5"));
        index.addDialogue(line("caius cosades", "Journal A1_1_FindSpymaster 14"));
        return index;
    }

    MWGui::QuestCorrections::Corrections corrected()
    {
        std::vector<std::string> problems;
        return MWGui::QuestCorrections::parse(R"(
version: 1
quests:
  A1_1_FindSpymaster:
    - stages: 1
      next: 14
      optional: 5
      targets:
        - { task: talk, object: bacola closcius, done: { quest: A1_2_AntabolisInformant, started: true } }
)",
            problems);
    }

    TEST(MWGuiQuestReviewTest, whatAStageShowsShouldFollowTheRule)
    {
        const Shown shown = shownAt(firstQuest(), {}, quest, 1, Rule::LowestAndHighest);
        EXPECT_EQ(shown.mMarked, (std::set<std::string>{ "elone", "caius cosades" }));
        EXPECT_TRUE(shown.mOptional.empty());
    }

    TEST(MWGuiQuestReviewTest, whatAStageShowsShouldFollowItsCorrection)
    {
        const Shown shown = shownAt(firstQuest(), corrected(), quest, 1, Rule::LowestAndHighest);
        EXPECT_EQ(shown.mMarked, (std::set<std::string>{ "caius cosades", "bacola closcius" }));
        EXPECT_EQ(shown.mOptional, (std::set<std::string>{ "cell:balmora, south wall cornerclub" }));
    }

    TEST(MWGuiQuestReviewTest, aRecordShouldBeReadAsItIsWritten)
    {
        Record record;
        record.mQuest = quest;
        record.mStages.push_back(Stage{ 1, "corrected",
            Shown{ { "caius cosades" }, { "cell:balmora, south wall cornerclub", "a \"quoted\" name" } } });
        record.mStages.push_back(Stage{ 2, "right", Shown{ { "caius cosades", "faction:blades" }, {} } });
        std::vector<std::string> problems;
        const Record read = parseRecord(writeRecord(record), problems);
        EXPECT_TRUE(problems.empty());
        EXPECT_EQ(read.mQuest, quest);
        ASSERT_EQ(read.mStages.size(), 2);
        EXPECT_EQ(read.mStages[0].mStage, 1);
        EXPECT_EQ(read.mStages[0].mVerdict, "corrected");
        EXPECT_EQ(read.mStages[0].mShown, record.mStages[0].mShown);
        EXPECT_EQ(read.mStages[1].mShown, record.mStages[1].mShown);
    }

    TEST(MWGuiQuestReviewTest, targetsInARecordShouldIgnoreCase)
    {
        std::vector<std::string> problems;
        const Record read
            = parseRecord("quest: A1_1_FindSpymaster\nstages:\n  - stage: 1\n    marked: [Caius Cosades]\n", problems);
        ASSERT_EQ(read.mStages.size(), 1);
        EXPECT_EQ(read.mStages[0].mShown.mMarked, (std::set<std::string>{ "caius cosades" }));
    }

    TEST(MWGuiQuestReviewTest, aStageThatCantBeReadShouldBeLeftOutWithAProblem)
    {
        std::vector<std::string> problems;
        const Record read = parseRecord(R"(
quest: A1_1_FindSpymaster
stages:
  - stage: one
    marked: [caius cosades]
  - stage: 2
    verdict: fine
  - stage: 3
    marked: caius cosades
  - stage: 4
    colour: green
  - stage: 5
    verdict: right
    marked: [caius cosades]
)",
            problems);
        EXPECT_EQ(problems.size(), 4);
        ASSERT_EQ(read.mStages.size(), 1);
        EXPECT_EQ(read.mStages[0].mStage, 5);
    }

    TEST(MWGuiQuestReviewTest, aRecordWithoutAQuestShouldHaveNoStages)
    {
        std::vector<std::string> problems;
        EXPECT_TRUE(parseRecord("stages:\n  - stage: 1\n", problems).mStages.empty());
        EXPECT_EQ(problems.size(), 1);
    }

    TEST(MWGuiQuestReviewTest, aStageThatShowsWhatItsRecordSaysShouldNotBeAChange)
    {
        Record record;
        record.mQuest = quest;
        record.mStages.push_back(Stage{ 1, "corrected", shownAt(firstQuest(), corrected(), quest, 1, Rule::All) });
        EXPECT_TRUE(compare(record, firstQuest(), corrected(), Rule::All).empty());
    }

    TEST(MWGuiQuestReviewTest, aStageThatShowsSomethingElseShouldBeAChange)
    {
        Record record;
        record.mQuest = quest;
        record.mStages.push_back(Stage{ 1, "corrected", shownAt(firstQuest(), corrected(), quest, 1, Rule::All) });
        // Without its correction, the stage marks Elone again.
        const std::vector<Change> changes = compare(record, firstQuest(), {}, Rule::LowestAndHighest);
        ASSERT_EQ(changes.size(), 1);
        EXPECT_EQ(changes[0].mStage, 1);
        EXPECT_EQ(changes[0].mExpected, record.mStages[0].mShown);
        EXPECT_TRUE(changes[0].mActual.mMarked.contains("elone"));
    }

    TEST(MWGuiQuestReviewTest, anEntryThatChangesNothingShouldBeFound)
    {
        std::vector<std::string> problems;
        const MWGui::QuestCorrections::Corrections corrections = MWGui::QuestCorrections::parse(R"(
version: 1
quests:
  A1_1_FindSpymaster:
    - stages: 1
      next: 14
    - stages: 5
      hide: 2
)",
            problems);
        MWGui::QuestCorrections::Content content;
        content.mStages[quest] = { 1, 2, 5, 14 };
        // At stage 5, the way to 2 is behind the player anyway, so hiding it changes nothing.
        EXPECT_EQ(
            MWGui::QuestCorrections::entriesWithoutEffect(corrections, firstQuest(), content, Rule::LowestAndHighest),
            (std::vector<std::string>{ "A1_1_FindSpymaster's entry for stage 5" }));
    }

    TEST(MWGuiQuestReviewTest, anEntryOnAStageWithoutAJournalEntryCanHaveAnEffect)
    {
        std::vector<std::string> problems;
        const MWGui::QuestCorrections::Corrections corrections = MWGui::QuestCorrections::parse(R"(
version: 1
quests:
  A1_1_FindSpymaster:
    - stages: 3
      next: 14
)",
            problems);
        MWGui::QuestCorrections::Content content;
        content.mStages[quest] = { 1, 2, 5, 14 };
        content.mSetStages[quest] = { 2, 3, 5, 14 };
        EXPECT_TRUE(
            MWGui::QuestCorrections::entriesWithoutEffect(corrections, firstQuest(), content, Rule::LowestAndHighest)
                .empty());
    }
}
