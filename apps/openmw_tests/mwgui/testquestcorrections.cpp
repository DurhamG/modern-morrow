#include "apps/openmw/mwgui/questcorrections.hpp"

#include <gtest/gtest.h>

namespace
{
    using namespace MWGui::QuestCorrections;

    const ESM::RefId quest = ESM::RefId::stringRefId("A1_1_FindSpymaster");

    Corrections read(const std::string& text, std::vector<std::string>& problems)
    {
        return parse("version: 1\nquests:\n" + text, problems);
    }

    TEST(MWGuiQuestCorrectionsTest, anEntryShouldBeReadWithItsStages)
    {
        std::vector<std::string> problems;
        const Corrections corrections = read(R"(
  A1_1_FindSpymaster:
    - stages: 1-2
      next: [14]
      hide: 5
)",
            problems);
        EXPECT_TRUE(problems.empty());
        const Entry* entry = corrections.find(quest, 1);
        ASSERT_NE(entry, nullptr);
        EXPECT_EQ(entry->mStages, (std::vector<Range>{ { 1, 2 } }));
        EXPECT_EQ(entry->mNext, (std::vector<Range>{ { 14, 14 } }));
        EXPECT_EQ(entry->mHide, (std::vector<Range>{ { 5, 5 } }));
        EXPECT_EQ(corrections.find(quest, 2), entry);
        EXPECT_EQ(corrections.find(quest, 10), nullptr);
    }

    TEST(MWGuiQuestCorrectionsTest, questIdsShouldIgnoreCase)
    {
        std::vector<std::string> problems;
        const Corrections corrections = read("  a1_1_findspymaster:\n    - stages: 1\n", problems);
        EXPECT_NE(corrections.find(quest, 1), nullptr);
    }

    TEST(MWGuiQuestCorrectionsTest, stagesShouldTakeAListOfIndicesAndRanges)
    {
        std::vector<std::string> problems;
        const Corrections corrections = read("  A1_1_FindSpymaster:\n    - stages: [1, 10-12]\n", problems);
        EXPECT_TRUE(problems.empty());
        EXPECT_NE(corrections.find(quest, 1), nullptr);
        EXPECT_EQ(corrections.find(quest, 2), nullptr);
        EXPECT_NE(corrections.find(quest, 11), nullptr);
        EXPECT_NE(corrections.find(quest, 12), nullptr);
    }

    TEST(MWGuiQuestCorrectionsTest, laterEntriesShouldCoverOtherStages)
    {
        std::vector<std::string> problems;
        const Corrections corrections = read(R"(
  A1_1_FindSpymaster:
    - stages: 1
      next: 14
    - stages: 14
      next: 18
)",
            problems);
        EXPECT_TRUE(problems.empty());
        ASSERT_NE(corrections.find(quest, 14), nullptr);
        EXPECT_EQ(corrections.find(quest, 14)->mNext, (std::vector<Range>{ { 18, 18 } }));
    }

    TEST(MWGuiQuestCorrectionsTest, anEntryThatCantBeReadShouldBeLeftOutWithAProblem)
    {
        std::vector<std::string> problems;
        const Corrections corrections = read(R"(
  A1_1_FindSpymaster:
    - stages: 1
      nxet: 14
    - stages: 2
      next: soon
    - next: 14
    - stages: 20-10
    - stages: 14
)",
            problems);
        EXPECT_EQ(problems.size(), 4);
        EXPECT_EQ(corrections.find(quest, 1), nullptr);
        EXPECT_EQ(corrections.find(quest, 2), nullptr);
        EXPECT_EQ(corrections.find(quest, 15), nullptr);
        EXPECT_NE(corrections.find(quest, 14), nullptr);
    }

    TEST(MWGuiQuestCorrectionsTest, anEntryForStagesAlreadyCoveredShouldBeLeftOut)
    {
        std::vector<std::string> problems;
        const Corrections corrections = read(R"(
  A1_1_FindSpymaster:
    - stages: 1-5
      next: 14
    - stages: 5
      next: 10
)",
            problems);
        ASSERT_EQ(problems.size(), 1);
        EXPECT_NE(problems[0].find("earlier entry"), std::string::npos);
        EXPECT_EQ(corrections.find(quest, 5)->mNext, (std::vector<Range>{ { 14, 14 } }));
    }

    TEST(MWGuiQuestCorrectionsTest, aFileThatIsntVersionOneShouldGiveNoCorrections)
    {
        std::vector<std::string> problems;
        const Corrections corrections
            = parse("version: 2\nquests:\n  A1_1_FindSpymaster:\n    - stages: 1\n", problems);
        EXPECT_EQ(problems.size(), 1);
        EXPECT_TRUE(corrections.mQuests.empty());
    }

    TEST(MWGuiQuestCorrectionsTest, aFileThatIsntYamlShouldGiveNoCorrections)
    {
        std::vector<std::string> problems;
        const Corrections corrections = parse("version: 1\nquests: [\n", problems);
        EXPECT_EQ(problems.size(), 1);
        EXPECT_TRUE(corrections.mQuests.empty());
    }

    Content mainQuest()
    {
        Content content;
        content.mStages[quest] = { 1, 2, 5, 10, 11, 12, 14, 18, 20, 22 };
        return content;
    }

    TEST(MWGuiQuestCorrectionsTest, entriesThatMatchTheContentShouldBeKept)
    {
        std::vector<std::string> problems;
        Corrections corrections = read(R"(
  A1_1_FindSpymaster:
    - stages: 1-2
      next: 14
      hide: [5, 10-12]
)",
            problems);
        check(corrections, mainQuest(), problems);
        EXPECT_TRUE(problems.empty());
        EXPECT_NE(corrections.find(quest, 1), nullptr);
    }

    TEST(MWGuiQuestCorrectionsTest, aQuestTheContentDoesntHaveShouldBeLeftOut)
    {
        std::vector<std::string> problems;
        Corrections corrections = read("  MS_SomeoneElsesQuest:\n    - stages: 10\n", problems);
        check(corrections, mainQuest(), problems);
        EXPECT_EQ(problems.size(), 1);
        EXPECT_TRUE(corrections.mQuests.empty());
    }

    TEST(MWGuiQuestCorrectionsTest, anEntryNamingAStageTheContentDoesntHaveShouldBeLeftOut)
    {
        std::vector<std::string> problems;
        Corrections corrections = read(R"(
  A1_1_FindSpymaster:
    - stages: 3
    - stages: 1
      next: 15
    - stages: 2
      hide: 15-17
    - stages: 10
      next: 13-15
)",
            problems);
        check(corrections, mainQuest(), problems);
        EXPECT_EQ(problems.size(), 3);
        EXPECT_EQ(corrections.find(quest, 1), nullptr);
        EXPECT_EQ(corrections.find(quest, 2), nullptr);
        EXPECT_NE(corrections.find(quest, 10), nullptr);
    }

    using MWGui::QuestTargets::Rule;
    using MWGui::QuestTargets::Way;

    Way way(int toStage, std::string_view person)
    {
        Way result;
        result.mToStage = toStage;
        result.mObjectives.push_back({ MWGui::QuestTargets::Task::Talk, ESM::RefId::stringRefId(person) });
        return result;
    }

    // The ways forward from the main quest's first stage: Elone gives directions, the South Wall Cornerclub and
    // Bacola Closcius are on the way, and Caius Cosades is where the player is sent.
    std::vector<Way> firstStage()
    {
        return { way(2, "elone"), way(5, "south wall"), way(10, "bacola closcius"), way(14, "caius cosades") };
    }

    std::vector<int> stagesOf(const std::vector<Way>& ways)
    {
        std::vector<int> result;
        for (const Way& item : ways)
            result.push_back(item.mToStage);
        return result;
    }

    TEST(MWGuiQuestCorrectionsTest, withoutAnEntryTheRuleShouldChoose)
    {
        EXPECT_EQ(stagesOf(chooseWays(firstStage(), nullptr, Rule::LowestAndHighest)), (std::vector<int>{ 2, 14 }));
    }

    TEST(MWGuiQuestCorrectionsTest, anEntrysNextStagesShouldBeTheOnlyOnesMarked)
    {
        Entry entry;
        entry.mNext = { { 14, 14 } };
        EXPECT_EQ(stagesOf(chooseWays(firstStage(), &entry, Rule::LowestAndHighest)), (std::vector<int>{ 14 }));
        EXPECT_EQ(stagesOf(chooseWays(firstStage(), &entry, Rule::All)), (std::vector<int>{ 14 }));
    }

    TEST(MWGuiQuestCorrectionsTest, hiddenStagesShouldBeLeftOutBeforeTheRuleChooses)
    {
        Entry entry;
        entry.mHide = { { 2, 2 } };
        EXPECT_EQ(stagesOf(chooseWays(firstStage(), &entry, Rule::Lowest)), (std::vector<int>{ 5 }));
        EXPECT_EQ(stagesOf(chooseWays(firstStage(), &entry, Rule::All)), (std::vector<int>{ 5, 10, 14 }));
    }

    TEST(MWGuiQuestCorrectionsTest, theRuleShouldChooseWhenNoNextWayIsLeft)
    {
        // Caius's way isn't among the ways left after the game's checks, such as when he is dead.
        std::vector<Way> ways = firstStage();
        ways.pop_back();
        Entry entry;
        entry.mNext = { { 14, 14 } };
        entry.mHide = { { 2, 2 } };
        EXPECT_EQ(stagesOf(chooseWays(ways, &entry, Rule::LowestAndHighest)), (std::vector<int>{ 5, 10 }));
    }

    TEST(MWGuiQuestCorrectionsTest, aNextWayThatIsAFailureShouldNotBeMarked)
    {
        std::vector<Way> ways = firstStage();
        ways.back().mFailure = true;
        Entry entry;
        entry.mNext = { { 10, 14 } };
        EXPECT_EQ(stagesOf(chooseWays(ways, &entry, Rule::LowestAndHighest)), (std::vector<int>{ 10 }));
        ways[2].mOutOfReach = true;
        EXPECT_EQ(stagesOf(chooseWays(ways, &entry, Rule::LowestAndHighest)), (std::vector<int>{ 2, 5 }));
    }
}
