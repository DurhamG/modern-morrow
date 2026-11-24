#include "apps/openmw/mwgui/questcorrections.hpp"

#include <components/esm3/loadinfo.hpp>

#include <gtest/gtest.h>

#include <map>
#include <set>

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

    TEST(MWGuiQuestCorrectionsTest, aFailureNamedAsNextShouldBeMarkedWhenEveryNextWayIsOne)
    {
        // Such as a kill the journal asks for, of someone who talks in the quest.
        std::vector<Way> ways = firstStage();
        ways.back().mFailure = true;
        Entry entry;
        entry.mNext = { { 14, 14 } };
        EXPECT_EQ(stagesOf(chooseWays(ways, &entry, Rule::LowestAndHighest)), (std::vector<int>{ 14 }));
        entry.mNext = { { 10, 14 } };
        EXPECT_EQ(stagesOf(chooseWays(ways, &entry, Rule::LowestAndHighest)), (std::vector<int>{ 10 }));
    }

    TEST(MWGuiQuestCorrectionsTest, aNextWayOutOfReachShouldNotBeMarked)
    {
        std::vector<Way> ways = firstStage();
        ways.back().mOutOfReach = true;
        Entry entry;
        entry.mNext = { { 10, 14 } };
        EXPECT_EQ(stagesOf(chooseWays(ways, &entry, Rule::LowestAndHighest)), (std::vector<int>{ 10 }));
        ways[2].mOutOfReach = true;
        EXPECT_EQ(stagesOf(chooseWays(ways, &entry, Rule::LowestAndHighest)), (std::vector<int>{ 2, 5 }));
    }

    std::vector<int> optionalStagesOf(const std::vector<Way>& ways)
    {
        std::vector<int> result;
        for (const Way& item : ways)
            if (item.mOptional)
                result.push_back(item.mToStage);
        return result;
    }

    TEST(MWGuiQuestCorrectionsTest, optionalStagesShouldBeRead)
    {
        std::vector<std::string> problems;
        Corrections corrections = read("  A1_1_FindSpymaster:\n    - stages: 1\n      optional: [5, 10]\n", problems);
        EXPECT_TRUE(problems.empty());
        ASSERT_NE(corrections.find(quest, 1), nullptr);
        EXPECT_EQ(corrections.find(quest, 1)->mOptional, (std::vector<Range>{ { 5, 5 }, { 10, 10 } }));
        corrections = read("  A1_1_FindSpymaster:\n    - stages: 1\n      optional: 13\n", problems);
        check(corrections, mainQuest(), problems);
        EXPECT_EQ(corrections.find(quest, 1), nullptr);
    }

    TEST(MWGuiQuestCorrectionsTest, optionalWaysShouldBeMarkedBesideTheNextOnes)
    {
        Entry entry;
        entry.mNext = { { 14, 14 } };
        entry.mOptional = { { 5, 5 }, { 10, 10 } };
        const std::vector<Way> chosen = chooseWays(firstStage(), &entry, Rule::LowestAndHighest);
        EXPECT_EQ(stagesOf(chosen), (std::vector<int>{ 5, 10, 14 }));
        EXPECT_EQ(optionalStagesOf(chosen), (std::vector<int>{ 5, 10 }));
    }

    TEST(MWGuiQuestCorrectionsTest, theRuleShouldChooseAmongTheWaysThatArentOptional)
    {
        Entry entry;
        entry.mOptional = { { 2, 2 } };
        const std::vector<Way> chosen = chooseWays(firstStage(), &entry, Rule::Lowest);
        EXPECT_EQ(stagesOf(chosen), (std::vector<int>{ 2, 5 }));
        EXPECT_EQ(optionalStagesOf(chosen), (std::vector<int>{ 2 }));
    }

    TEST(MWGuiQuestCorrectionsTest, anOptionalWayThatIsAFailureShouldNotBeMarked)
    {
        std::vector<Way> ways = firstStage();
        ways[1].mOutOfReach = true;
        Entry entry;
        entry.mNext = { { 14, 14 } };
        entry.mOptional = { { 5, 5 } };
        EXPECT_EQ(stagesOf(chooseWays(ways, &entry, Rule::LowestAndHighest)), (std::vector<int>{ 14 }));
    }

    TEST(MWGuiQuestCorrectionsTest, waysShouldNotBeOptionalWithoutACorrection)
    {
        EXPECT_TRUE(optionalStagesOf(chooseWays(firstStage(), nullptr, Rule::All)).empty());
    }

    const ESM::RefId vivec = ESM::RefId::stringRefId("A1_V_VivecInformants");
    const ESM::RefId addhiranirrQuest = ESM::RefId::stringRefId("A1_6_AddhiranirrInformant");
    const ESM::RefId huleeyaQuest = ESM::RefId::stringRefId("A1_7HuleeyaInformant");

    const std::string vivecEntry = R"(
  A1_V_VivecInformants:
    - stages: 1
      targets:
        - { task: talk, object: addhiranirr, done: { quest: A1_6_AddhiranirrInformant, started: true } }
        - { task: talk, object: huleeya, done: { quest: A1_7HuleeyaInformant, atLeast: 50 } }
        - { task: visit, cell: "Vivec, Library of Vivec", done: { quest: A1_7HuleeyaInformant, finished: true } }
        - { task: fetch, object: bk_progressoftruth, count: 2 }
)";

    Content vivecContent()
    {
        Content content;
        content.mStages[vivec] = { 1, 50, 55 };
        content.mStages[addhiranirrQuest] = { 5, 10, 45, 60 };
        content.mStages[huleeyaQuest] = { 1, 2, 20, 50 };
        content.mHasObject = [](const ESM::RefId& id) { return id != ESM::RefId::stringRefId("nobody"); };
        content.mHasCell
            = [](const ESM::RefId& id) { return id == ESM::RefId::stringRefId("Vivec, Library of Vivec"); };
        content.mHasGlobal = [](std::string_view name) { return name == "HortatorVotes"; };
        return content;
    }

    const Progress* ptr(const Progress& progress)
    {
        return &progress;
    }

    Progress progressOf(std::map<ESM::RefId, int> stages, std::set<ESM::RefId> finished = {})
    {
        Progress progress;
        progress.mStage = [stages](const ESM::RefId& id) {
            const auto it = stages.find(id);
            return it == stages.end() ? 0 : it->second;
        };
        progress.mFinished = [finished](const ESM::RefId& id) { return finished.contains(id); };
        return progress;
    }

    TEST(MWGuiQuestCorrectionsTest, targetsShouldBeReadWithTheirDoneConditions)
    {
        std::vector<std::string> problems;
        Corrections corrections = read(vivecEntry, problems);
        check(corrections, vivecContent(), problems);
        EXPECT_TRUE(problems.empty());
        const Entry* entry = corrections.find(vivec, 1);
        ASSERT_NE(entry, nullptr);
        ASSERT_EQ(entry->mTargets.size(), 4);
        EXPECT_EQ(entry->mTargets[0].mObject, ESM::RefId::stringRefId("addhiranirr"));
        EXPECT_EQ(entry->mTargets[0].mDone, (Condition{ addhiranirrQuest, 1, false }));
        EXPECT_EQ(entry->mTargets[1].mDone, (Condition{ huleeyaQuest, 50, false }));
        EXPECT_EQ(entry->mTargets[2].mTask, MWGui::QuestTargets::Task::Visit);
        EXPECT_EQ(entry->mTargets[2].mCell, ESM::RefId::stringRefId("Vivec, Library of Vivec"));
        EXPECT_EQ(entry->mTargets[2].mDone, (Condition{ huleeyaQuest, 0, true }));
        EXPECT_EQ(entry->mTargets[3].mCount, 2);
        EXPECT_FALSE(entry->mTargets[3].mDone);
    }

    TEST(MWGuiQuestCorrectionsTest, aTargetThatCantBeReadShouldLeaveItsEntryOut)
    {
        const std::string targets[] = {
            "{ object: huleeya }",
            "{ task: chat, object: huleeya }",
            "{ task: talk }",
            "{ task: talk, object: huleeya, cell: Vivec }",
            "{ task: talk, object: huleeya, done: { quest: A1_7HuleeyaInformant } }",
            "{ task: talk, object: huleeya, done: { atLeast: 50 } }",
            "{ task: talk, object: huleeya, done: { quest: A1_7HuleeyaInformant, atLeast: 50, finished: true } }",
            "{ task: talk, object: huleeya, colour: green }",
            "{ task: talk, object: huleeya, done: { dead: huleeya, carried: bk_progressoftruth } }",
            "{ task: talk, object: huleeya, done: { quest: A1_7HuleeyaInformant, dead: huleeya } }",
            "{ task: talk, object: huleeya, after: { finished: true } }",
            "{ task: talk, object: huleeya, optional: maybe }",
            "{ task: talk, object: huleeya, after: { global: HortatorVotes } }",
            "{ task: talk, object: huleeya, after: { global: HortatorVotes, finished: true } }",
            "{ task: talk, object: huleeya, after: [{ dead: huleeya }, { colour: green }] }",
        };
        for (const std::string& target : targets)
        {
            std::vector<std::string> problems;
            const Corrections corrections
                = read("  A1_V_VivecInformants:\n    - stages: 1\n      targets: [" + target + "]\n", problems);
            EXPECT_EQ(problems.size(), 1) << target;
            EXPECT_EQ(corrections.find(vivec, 1), nullptr) << target;
        }
    }

    TEST(MWGuiQuestCorrectionsTest, aTargetTheContentDoesntHaveShouldLeaveItsEntryOut)
    {
        const std::string targets[] = {
            "{ task: talk, object: nobody }",
            "{ task: visit, cell: Nowhere }",
            "{ task: talk, object: huleeya, done: { quest: MS_SomeoneElsesQuest, started: true } }",
            "{ task: talk, object: huleeya, after: { dead: nobody } }",
            "{ task: talk, object: huleeya, done: { carried: nobody } }",
            "{ task: talk, object: huleeya, after: { global: NoSuchGlobal, atLeast: 1 } }",
        };
        for (const std::string& target : targets)
        {
            std::vector<std::string> problems;
            Corrections corrections
                = read("  A1_V_VivecInformants:\n    - stages: 1\n      targets: [" + target + "]\n", problems);
            check(corrections, vivecContent(), problems);
            EXPECT_EQ(problems.size(), 1) << target;
            EXPECT_EQ(corrections.find(vivec, 1), nullptr) << target;
        }
    }

    std::vector<ESM::RefId> targetsOf(const std::vector<Way>& ways)
    {
        std::vector<ESM::RefId> result;
        for (const Way& item : ways)
            for (const MWGui::QuestTargets::Objective& objective : item.mObjectives)
                result.push_back(objective.mObject.empty() ? objective.mCell : objective.mObject);
        return result;
    }

    TEST(MWGuiQuestCorrectionsTest, targetsShouldBeMarkedUntilTheyAreDone)
    {
        std::vector<std::string> problems;
        const Corrections corrections = read(vivecEntry, problems);
        const Entry& entry = *corrections.find(vivec, 1);
        const ESM::RefId addhiranirr = ESM::RefId::stringRefId("addhiranirr");
        const ESM::RefId huleeya = ESM::RefId::stringRefId("huleeya");
        const ESM::RefId library = ESM::RefId::stringRefId("Vivec, Library of Vivec");
        const ESM::RefId book = ESM::RefId::stringRefId("bk_progressoftruth");

        EXPECT_EQ(targetsOf(targetWays(entry, ptr(progressOf({})))),
            (std::vector<ESM::RefId>{ addhiranirr, huleeya, library, book }));
        // Addhiranirr's own quest has started, and Huleeya's hasn't reached 50.
        EXPECT_EQ(targetsOf(targetWays(entry, ptr(progressOf({ { addhiranirrQuest, 5 }, { huleeyaQuest, 20 } })))),
            (std::vector<ESM::RefId>{ huleeya, library, book }));
        EXPECT_EQ(targetsOf(targetWays(entry, ptr(progressOf({ { huleeyaQuest, 50 } })))),
            (std::vector<ESM::RefId>{ addhiranirr, library, book }));
        // A finished quest counts as done whatever its stage.
        EXPECT_EQ(targetsOf(targetWays(entry, ptr(progressOf({ { huleeyaQuest, 2 } }, { huleeyaQuest })))),
            (std::vector<ESM::RefId>{ addhiranirr, book }));
    }

    TEST(MWGuiQuestCorrectionsTest, aTargetsWayShouldCarryItsTaskAndCount)
    {
        std::vector<std::string> problems;
        const Corrections corrections = read(vivecEntry, problems);
        const std::vector<Way> ways = targetWays(*corrections.find(vivec, 1), ptr(progressOf({})));
        ASSERT_EQ(ways.size(), 4);
        EXPECT_EQ(ways[3].mObjectives.front().mTask, MWGui::QuestTargets::Task::Fetch);
        EXPECT_EQ(ways[3].mObjectives.front().mCount, 2);
        EXPECT_FALSE(ways[0].mOptional);
    }

    TEST(MWGuiQuestCorrectionsTest, followedQuestsShouldBeRead)
    {
        std::vector<std::string> problems;
        Corrections corrections = read(R"(
  A1_V_VivecInformants:
    - stages: 1
      follow: [A1_6_AddhiranirrInformant, A1_7HuleeyaInformant]
)",
            problems);
        check(corrections, vivecContent(), problems);
        EXPECT_TRUE(problems.empty());
        ASSERT_NE(corrections.find(vivec, 1), nullptr);
        EXPECT_EQ(corrections.find(vivec, 1)->mFollow, (std::vector<ESM::RefId>{ addhiranirrQuest, huleeyaQuest }));
    }

    TEST(MWGuiQuestCorrectionsTest, followingAQuestTheContentDoesntHaveShouldLeaveTheEntryOut)
    {
        std::vector<std::string> problems;
        Corrections corrections
            = read("  A1_V_VivecInformants:\n    - stages: 1\n      follow: MS_SomeoneElsesQuest\n", problems);
        check(corrections, vivecContent(), problems);
        EXPECT_EQ(problems.size(), 1);
        EXPECT_EQ(corrections.find(vivec, 1), nullptr);
    }

    TEST(MWGuiQuestCorrectionsTest, onlyStartedQuestsThatArentFinishedShouldBeFollowed)
    {
        Entry entry;
        entry.mFollow = { addhiranirrQuest, huleeyaQuest };
        EXPECT_TRUE(followedQuests(entry, progressOf({})).empty());
        EXPECT_EQ(
            followedQuests(entry, progressOf({ { huleeyaQuest, 1 } })), (std::vector<ESM::RefId>{ huleeyaQuest }));
        EXPECT_EQ(
            followedQuests(entry, progressOf({ { addhiranirrQuest, 5 }, { huleeyaQuest, 50 } }, { huleeyaQuest })),
            (std::vector<ESM::RefId>{ addhiranirrQuest }));
    }

    TEST(MWGuiQuestCorrectionsTest, aFingerprintShouldNotDependOnTheOrderOfTheFacts)
    {
        EXPECT_EQ(fingerprint({ "a", "b", "c" }), fingerprint({ "c", "a", "b" }));
        EXPECT_EQ(fingerprint({ "a" }).size(), 16);
    }

    TEST(MWGuiQuestCorrectionsTest, aFingerprintShouldChangeWithAnyFact)
    {
        EXPECT_NE(fingerprint({ "a", "b" }), fingerprint({ "a", "c" }));
        EXPECT_NE(fingerprint({ "a", "b" }), fingerprint({ "a", "b", "c" }));
        EXPECT_NE(fingerprint({ "ab", "c" }), fingerprint({ "a", "bc" }));
    }

    ESM::DialInfo questLine(
        std::string_view speaker, std::string_view result, std::vector<ESM::DialogueCondition> conditions = {})
    {
        ESM::DialInfo info;
        info.mActor = ESM::RefId::stringRefId(speaker);
        info.mResultScript = result;
        info.mSelects = std::move(conditions);
        return info;
    }

    ESM::DialogueCondition stageAtLeast(int value)
    {
        ESM::DialogueCondition condition;
        condition.mFunction = ESM::DialogueCondition::Function_Journal;
        condition.mVariable = "A1_1_FindSpymaster";
        condition.mComparison = ESM::DialogueCondition::Comp_Ge;
        condition.mValue = value;
        return condition;
    }

    MWGui::QuestTargets::Index spymasterIndex()
    {
        MWGui::QuestTargets::Index index;
        index.addDialogue(questLine("elone", "Journal A1_1_FindSpymaster 2"), "Balmora");
        index.addDialogue(
            questLine("caius cosades", "Journal A1_1_FindSpymaster 14", { stageAtLeast(1) }), "Report to Caius");
        return index;
    }

    TEST(MWGuiQuestCorrectionsTest, aQuestsFingerprintShouldFollowItsShape)
    {
        const std::set<int> stages = { 1, 2, 14 };
        const std::string original = fingerprintOf(quest, stages, spymasterIndex());
        EXPECT_EQ(fingerprintOf(quest, stages, spymasterIndex()), original);
        // Another journal index.
        EXPECT_NE(fingerprintOf(quest, { 1, 2, 14, 16 }, spymasterIndex()), original);
        // Another speaker.
        MWGui::QuestTargets::Index moved;
        moved.addDialogue(questLine("fargoth", "Journal A1_1_FindSpymaster 2"), "Balmora");
        moved.addDialogue(
            questLine("caius cosades", "Journal A1_1_FindSpymaster 14", { stageAtLeast(1) }), "Report to Caius");
        EXPECT_NE(fingerprintOf(quest, stages, moved), original);
        // Another condition on the quest's stage.
        MWGui::QuestTargets::Index gated;
        gated.addDialogue(questLine("elone", "Journal A1_1_FindSpymaster 2"), "Balmora");
        gated.addDialogue(
            questLine("caius cosades", "Journal A1_1_FindSpymaster 14", { stageAtLeast(2) }), "Report to Caius");
        EXPECT_NE(fingerprintOf(quest, stages, gated), original);
    }

    TEST(MWGuiQuestCorrectionsTest, aQuestThatMovesOnDifferentlyShouldLoseItsEntries)
    {
        const MWGui::QuestTargets::Index index = spymasterIndex();
        Content content = mainQuest();
        content.mFingerprint = [&](const ESM::RefId& id) { return fingerprintOf(id, content.mStages[id], index); };
        const std::string text = "  A1_1_FindSpymaster:\n    - stages: 1\n      next: 14\n";

        std::vector<std::string> problems;
        Corrections corrections = read(text, problems);
        corrections.mFingerprints[quest] = content.mFingerprint(quest);
        check(corrections, content, problems);
        EXPECT_TRUE(problems.empty());
        EXPECT_NE(corrections.find(quest, 1), nullptr);

        corrections = read(text, problems);
        corrections.mFingerprints[quest] = "0123456789abcdef";
        check(corrections, content, problems);
        EXPECT_EQ(problems.size(), 1);
        EXPECT_EQ(corrections.find(quest, 1), nullptr);
    }

    TEST(MWGuiQuestCorrectionsTest, fingerprintsShouldBeRead)
    {
        std::vector<std::string> problems;
        const Corrections corrections = parse(
            "version: 1\nfingerprints:\n  A1_1_FindSpymaster: 0123456789abcdef\nquests:\n  A1_1_FindSpymaster:\n"
            "    - stages: 1\n",
            problems);
        EXPECT_TRUE(problems.empty());
        ASSERT_TRUE(corrections.mFingerprints.contains(quest));
        EXPECT_EQ(corrections.mFingerprints.at(quest), "0123456789abcdef");
    }

    TEST(MWGuiQuestCorrectionsTest, aTargetShouldBeReadWithItsOtherConditionsAndOptional)
    {
        std::vector<std::string> problems;
        Corrections corrections = read(R"(
  A1_V_VivecInformants:
    - stages: 1
      targets:
        - { task: kill, object: huleeya, optional: true, done: { dead: huleeya } }
        - { task: talk, object: addhiranirr, after: { dead: huleeya }, done: { carried: bk_progressoftruth } }
)",
            problems);
        check(corrections, vivecContent(), problems);
        EXPECT_TRUE(problems.empty());
        const Entry& entry = *corrections.find(vivec, 1);
        EXPECT_TRUE(entry.mTargets[0].mOptional);
        EXPECT_EQ(entry.mTargets[0].mDone, (Condition{ {}, 0, false, ESM::RefId::stringRefId("huleeya") }));
        EXPECT_FALSE(entry.mTargets[1].mOptional);
        EXPECT_EQ(entry.mTargets[1].mAfter,
            (std::vector<Condition>{ Condition{ {}, 0, false, ESM::RefId::stringRefId("huleeya") } }));
        EXPECT_EQ(
            entry.mTargets[1].mDone, (Condition{ {}, 0, false, {}, ESM::RefId::stringRefId("bk_progressoftruth") }));
    }

    TEST(MWGuiQuestCorrectionsTest, aTargetShouldBeMarkedFromItsAfterConditionToItsDoneCondition)
    {
        // Kill Huleeya, then report to Addhiranirr until the player carries the book.
        Entry entry;
        Target kill;
        kill.mTask = MWGui::QuestTargets::Task::Kill;
        kill.mObject = ESM::RefId::stringRefId("huleeya");
        kill.mDone = Condition{ {}, 0, false, kill.mObject };
        Target report;
        report.mObject = ESM::RefId::stringRefId("addhiranirr");
        report.mAfter = { Condition{ {}, 0, false, kill.mObject } };
        report.mDone = Condition{ {}, 0, false, {}, ESM::RefId::stringRefId("bk_progressoftruth") };
        entry.mTargets = { kill, report };

        bool dead = false;
        bool carried = false;
        Progress progress = progressOf({});
        progress.mDead = [&](const ESM::RefId&) { return dead; };
        progress.mCarried = [&](const ESM::RefId&) { return carried; };
        EXPECT_EQ(targetsOf(targetWays(entry, &progress)), (std::vector<ESM::RefId>{ kill.mObject }));
        dead = true;
        EXPECT_EQ(targetsOf(targetWays(entry, &progress)), (std::vector<ESM::RefId>{ report.mObject }));
        carried = true;
        EXPECT_TRUE(targetWays(entry, &progress).empty());
        // Without progress, every target counts, as when reviewing.
        EXPECT_EQ(targetWays(entry, nullptr).size(), 2);
    }

    TEST(MWGuiQuestCorrectionsTest, anOptionalTargetShouldGiveAnOptionalWay)
    {
        Entry entry;
        Target target;
        target.mObject = ESM::RefId::stringRefId("sondryn irathi");
        target.mOptional = true;
        entry.mTargets = { target };
        const std::vector<Way> ways = targetWays(entry, nullptr);
        ASSERT_EQ(ways.size(), 1);
        EXPECT_TRUE(ways[0].mOptional);
    }

    TEST(MWGuiQuestCorrectionsTest, aTargetShouldWaitForAllItsAfterConditions)
    {
        std::vector<std::string> problems;
        Corrections corrections = read(R"(
  A1_V_VivecInformants:
    - stages: 1
      targets:
        - task: talk
          object: huleeya
          after: [{ quest: A1_6_AddhiranirrInformant, atLeast: 45 }, { global: HortatorVotes, atLeast: 6 }]
)",
            problems);
        check(corrections, vivecContent(), problems);
        ASSERT_TRUE(problems.empty()) << problems.front();
        const Entry& entry = *corrections.find(vivec, 1);
        ASSERT_EQ(entry.mTargets[0].mAfter.size(), 2);
        EXPECT_EQ(entry.mTargets[0].mAfter[1].mGlobal, "HortatorVotes");
        EXPECT_EQ(entry.mTargets[0].mAfter[1].mAtLeast, 6);

        float votes = 5;
        Progress progress = progressOf({ { addhiranirrQuest, 45 } });
        progress.mGlobal = [&](std::string_view) { return votes; };
        EXPECT_TRUE(targetWays(entry, &progress).empty());
        votes = 6;
        EXPECT_EQ(targetWays(entry, &progress).size(), 1);
        progress.mStage = [](const ESM::RefId&) { return 10; };
        EXPECT_TRUE(targetWays(entry, &progress).empty());
    }

    TEST(MWGuiQuestCorrectionsTest, aCarriedConditionCanNeedSeveralOfAnItem)
    {
        std::vector<std::string> problems;
        Corrections corrections = read(R"(
  A1_V_VivecInformants:
    - stages: 1
      targets:
        - { task: talk, object: huleeya, after: { carried: bk_progressoftruth, atLeast: 4 } }
)",
            problems);
        check(corrections, vivecContent(), problems);
        ASSERT_TRUE(problems.empty()) << problems.front();
        const Entry& entry = *corrections.find(vivec, 1);
        int carried = 3;
        Progress progress = progressOf({});
        progress.mCarried = [&](const ESM::RefId&) { return carried; };
        EXPECT_TRUE(targetWays(entry, &progress).empty());
        carried = 4;
        EXPECT_EQ(targetWays(entry, &progress).size(), 1);
    }
}
