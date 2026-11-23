#include "apps/openmw/mwgui/questtargets.hpp"

#include <components/esm3/loadinfo.hpp>

#include <gtest/gtest.h>

namespace
{
    using namespace MWGui::QuestTargets;
    using ESM::DialogueCondition;

    const ESM::RefId quest = ESM::RefId::stringRefId("fg_rathunt");

    ESM::RefId id(std::string_view name)
    {
        return ESM::RefId::stringRefId(name);
    }

    DialogueCondition condition(DialogueCondition::Function function, std::string_view variable,
        DialogueCondition::Comparison comparison, int value)
    {
        DialogueCondition result;
        result.mFunction = function;
        result.mVariable = variable;
        result.mComparison = comparison;
        result.mValue = value;
        return result;
    }

    DialogueCondition stageIs(DialogueCondition::Comparison comparison, int value)
    {
        return condition(DialogueCondition::Function_Journal, "FG_RatHunt", comparison, value);
    }

    ESM::DialInfo line(
        std::string_view speaker, std::string_view result, std::vector<DialogueCondition> conditions = {})
    {
        ESM::DialInfo info;
        info.mActor = speaker.empty() ? ESM::RefId() : id(speaker);
        info.mResultScript = result;
        info.mSelects = std::move(conditions);
        return info;
    }

    Objective talk(std::string_view speaker)
    {
        return Objective{ .mTask = Task::Talk, .mObject = id(speaker) };
    }

    TEST(MWGuiQuestTargetsJournalChangesTest, findsJournalAndSetJournalIndex)
    {
        const auto changes = journalChanges("Journal FG_RatHunt 50\nSetJournalIndex, \"MS Lookout\", 20\n");
        ASSERT_EQ(changes.size(), 2u);
        EXPECT_EQ(changes[0], std::make_pair(id("fg_rathunt"), 50));
        EXPECT_EQ(changes[1], std::make_pair(id("ms lookout"), 20));
    }

    TEST(MWGuiQuestTargetsJournalChangesTest, findsCommandsRunOnAnotherObject)
    {
        const auto changes = journalChanges("\"fargoth\"->Journal MS_Fargoth 10\nplayer->journal A1_1 5");
        ASSERT_EQ(changes.size(), 2u);
        EXPECT_EQ(changes[0], std::make_pair(id("ms_fargoth"), 10));
        EXPECT_EQ(changes[1], std::make_pair(id("a1_1"), 5));
    }

    TEST(MWGuiQuestTargetsJournalChangesTest, ignoresComments)
    {
        EXPECT_TRUE(journalChanges("; Journal FG_RatHunt 50\nset x to 1 ; journal FG_RatHunt 10").empty());
    }

    TEST(MWGuiQuestTargetsScriptEventTest, recognisesDeathAndPickUp)
    {
        EXPECT_EQ(scriptEvent("if ( OnDeath == 1 )\nJournal MT_Writ 80\nendif"), ScriptEvent::Death);
        EXPECT_EQ(scriptEvent("if ( GetHealth <= 0 )\nendif"), ScriptEvent::Death);
        EXPECT_EQ(scriptEvent("if ( OnPCAdd == 1 )\nendif"), ScriptEvent::PickUp);
        EXPECT_EQ(scriptEvent("if ( OnActivate == 1 )\nendif"), ScriptEvent::Other);
    }

    TEST(MWGuiQuestTargetsAllowsStageTest, checksConditionsOnTheQuestOnly)
    {
        const std::vector<DialogueCondition> conditions{ stageIs(DialogueCondition::Comp_Ge, 10),
            stageIs(DialogueCondition::Comp_Ls, 50),
            condition(DialogueCondition::Function_Journal, "Other", DialogueCondition::Comp_Eq, 99) };
        EXPECT_FALSE(allowsStage(conditions, quest, 5));
        EXPECT_TRUE(allowsStage(conditions, quest, 10));
        EXPECT_TRUE(allowsStage(conditions, quest, 49));
        EXPECT_FALSE(allowsStage(conditions, quest, 50));
        EXPECT_TRUE(allowsStage({}, quest, 70));
    }

    TEST(MWGuiQuestTargetsIndexTest, lowestRuleTakesTheNextStageOnly)
    {
        Index index;
        index.addDialogue(
            line("drarayne thelas", "Journal FG_RatHunt 50", { stageIs(DialogueCondition::Comp_Eq, 10) }));
        index.addDialogue(
            line("eydis fire-eye", "Journal FG_RatHunt 105", { stageIs(DialogueCondition::Comp_Eq, 100) }));
        index.addDialogue(line("drarayne thelas", "Journal FG_RatHunt 100"));
        const std::vector<Way> ways = index.waysForward(quest, 10, Rule::Lowest);
        ASSERT_EQ(ways.size(), 1u);
        EXPECT_EQ(ways[0].mToStage, 50);
        EXPECT_EQ(ways[0].mObjectives, std::vector<Objective>{ talk("drarayne thelas") });
    }

    TEST(MWGuiQuestTargetsIndexTest, allRuleTakesEveryWayForwardInStageOrder)
    {
        Index index;
        index.addDialogue(line("caius cosades", "Journal FG_RatHunt 14"));
        index.addDialogue(line("elone", "Journal FG_RatHunt 2"));
        index.addDialogue(line("bacola closcius", "Journal FG_RatHunt 10"));
        const std::vector<Way> ways = index.waysForward(quest, 1, Rule::All);
        ASSERT_EQ(ways.size(), 3u);
        EXPECT_EQ(ways[0].mToStage, 2);
        EXPECT_EQ(ways[1].mToStage, 10);
        EXPECT_EQ(ways[2].mToStage, 14);
    }

    TEST(MWGuiQuestTargetsIndexTest, killsAndFetchesComeBeforeTheTalk)
    {
        Index index;
        index.addDialogue(line("drarayne thelas", "Journal FG_RatHunt 100",
            { condition(DialogueCondition::Function_Dead, "rat_cave_fgrh", DialogueCondition::Comp_Ge, 3),
                condition(DialogueCondition::Function_Item, "misc_puzzle_box", DialogueCondition::Comp_Ge, 1),
                condition(DialogueCondition::Function_Item, "Gold_001", DialogueCondition::Comp_Ge, 100) }));
        const std::vector<Way> ways = index.waysForward(quest, 50, Rule::Lowest);
        ASSERT_EQ(ways.size(), 1u);
        const std::vector<Objective> expected{
            Objective{ .mTask = Task::Kill, .mObject = id("rat_cave_fgrh"), .mCount = 3 },
            Objective{ .mTask = Task::Fetch, .mObject = id("misc_puzzle_box") },
            talk("drarayne thelas"),
        };
        EXPECT_EQ(ways[0].mObjectives, expected);
    }

    TEST(MWGuiQuestTargetsIndexTest, needingAQuestSpeakerDeadIsAFailureBranch)
    {
        Index index;
        index.addDialogue(line("drarayne thelas", "Journal FG_RatHunt 50"));
        index.addDialogue(line("eydis fire-eye", "Journal FG_RatHunt 40",
            { condition(DialogueCondition::Function_Dead, "drarayne thelas", DialogueCondition::Comp_Eq, 1) }));
        const std::vector<Way> ways = index.waysForward(quest, 10, Rule::Lowest);
        ASSERT_EQ(ways.size(), 1u);
        EXPECT_EQ(ways[0].mToStage, 50);
    }

    TEST(MWGuiQuestTargetsIndexTest, aDeadConditionThatNeedsSomeoneAliveIsNoKill)
    {
        Index index;
        index.addDialogue(line("drarayne thelas", "Journal FG_RatHunt 50",
            { condition(DialogueCondition::Function_Dead, "rat_cave_fgrh", DialogueCondition::Comp_Eq, 0) }));
        const std::vector<Way> ways = index.waysForward(quest, 10, Rule::Lowest);
        ASSERT_EQ(ways.size(), 1u);
        EXPECT_EQ(ways[0].mObjectives, std::vector<Objective>{ talk("drarayne thelas") });
    }

    TEST(MWGuiQuestTargetsIndexTest, aScriptOnAnActorThatDiesIsAKill)
    {
        Index index;
        index.addScript("begin writ\nif ( OnDeath == 1 )\nJournal FG_RatHunt 80\nendif\nend",
            { Index::Carrier{ id("tirer belvayn"), true } });
        const std::vector<Way> ways = index.waysForward(quest, 10, Rule::Lowest);
        ASSERT_EQ(ways.size(), 1u);
        const std::vector<Objective> expected{ Objective{ .mTask = Task::Kill, .mObject = id("tirer belvayn") } };
        EXPECT_EQ(ways[0].mObjectives, expected);
    }

    TEST(MWGuiQuestTargetsIndexTest, aScriptOnAnItemPickedUpIsAFetch)
    {
        Index index;
        index.addScript(
            "if ( OnPCAdd == 1 )\nJournal FG_RatHunt 7\nendif", { Index::Carrier{ id("misc_box"), false } });
        const std::vector<Way> ways = index.waysForward(quest, 5, Rule::Lowest);
        ASSERT_EQ(ways.size(), 1u);
        const std::vector<Objective> expected{ Objective{ .mTask = Task::Fetch, .mObject = id("misc_box") } };
        EXPECT_EQ(ways[0].mObjectives, expected);
    }

    TEST(MWGuiQuestTargetsIndexTest, aGlobalScriptKillsWhatItCountsTheDeadOf)
    {
        Index index;
        index.addScript("if ( GetDeadCount \"dagoth_x\" > 0 )\nJournal FG_RatHunt 30\nendif", {});
        const std::vector<Way> ways = index.waysForward(quest, 10, Rule::Lowest);
        ASSERT_EQ(ways.size(), 1u);
        const std::vector<Objective> expected{ Objective{ .mTask = Task::Kill, .mObject = id("dagoth_x") } };
        EXPECT_EQ(ways[0].mObjectives, expected);
    }

    TEST(MWGuiQuestTargetsIndexTest, aLineWithoutASpeakerTalksToItsFactionOrCell)
    {
        Index index;
        ESM::DialInfo byFaction = line("", "Journal FG_RatHunt 100");
        byFaction.mFaction = id("Morag Tong");
        index.addDialogue(byFaction);
        ESM::DialInfo byCell = line("", "Journal FG_RatHunt 90");
        byCell.mCell = id("Maar Gan");
        index.addDialogue(byCell);
        const std::vector<Way> ways = index.waysForward(quest, 80, Rule::All);
        ASSERT_EQ(ways.size(), 2u);
        EXPECT_EQ(ways[0].mObjectives, std::vector<Objective>{ Objective{ .mCell = id("Maar Gan") } });
        EXPECT_EQ(ways[1].mObjectives, std::vector<Objective>{ Objective{ .mFaction = id("Morag Tong") } });
    }

    TEST(MWGuiQuestTargetsIndexTest, repeatedLinesGiveOneWay)
    {
        Index index;
        index.addDialogue(line("drarayne thelas", "Journal FG_RatHunt 50"));
        index.addDialogue(line("drarayne thelas", "Journal FG_RatHunt 50"));
        EXPECT_EQ(index.waysForward(quest, 10, Rule::All).size(), 1u);
    }

    TEST(MWGuiQuestTargetsIndexTest, finishedQuestsAndOtherQuestsGiveNothing)
    {
        Index index;
        index.addDialogue(line("drarayne thelas", "Journal FG_RatHunt 50"));
        EXPECT_TRUE(index.waysForward(quest, 50, Rule::All).empty());
        EXPECT_TRUE(index.waysForward(id("other"), 0, Rule::All).empty());
    }
}
