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

    CallTargets targetsOfFirstCall(std::string_view script)
    {
        const std::vector<ScriptJournalCall> calls = scriptJournalCalls(script);
        return calls.empty() ? CallTargets() : callTargets(calls.front().mConditions);
    }

    TEST(MWGuiQuestTargetsScriptJournalCallsTest, keepsTheConditionsAroundEachCall)
    {
        const std::vector<ScriptJournalCall> calls = scriptJournalCalls(
            "if ( OnDeath == 1 )\n"
            "  if ( GetJournalIndex FG_RatHunt < 80 )\n"
            "    Journal FG_RatHunt 80\n"
            "  endif\n"
            "endif\n"
            "if ( OnActivate == 1 )\n"
            "  Journal FG_RatHunt 90\n"
            "endif\n");
        ASSERT_EQ(calls.size(), 2u);
        EXPECT_EQ(calls[0].mStage, 80);
        ASSERT_EQ(calls[0].mConditions.size(), 2u);
        EXPECT_EQ(calls[0].mConditions[0][1], "getjournalindex");
        EXPECT_EQ(calls[0].mConditions[1][1], "ondeath");
        EXPECT_EQ(calls[1].mStage, 90);
        ASSERT_EQ(calls[1].mConditions.size(), 1u);
        EXPECT_EQ(calls[1].mConditions[0][1], "onactivate");
    }

    TEST(MWGuiQuestTargetsScriptJournalCallsTest, anElseifBranchHasItsOwnCondition)
    {
        const std::vector<ScriptJournalCall> calls = scriptJournalCalls(
            "if ( OnDeath == 1 )\n"
            "  Journal FG_RatHunt 80\n"
            "elseif ( GetPCCell \"Gnaar Mok\" == 1 )\n"
            "  Journal FG_RatHunt 90\n"
            "endif\n");
        ASSERT_EQ(calls.size(), 2u);
        ASSERT_EQ(calls[1].mConditions.size(), 1u);
        EXPECT_EQ(calls[1].mConditions[0][0], "elseif");
    }

    TEST(MWGuiQuestTargetsCallTargetsTest, recognisesDeathAndPickUp)
    {
        EXPECT_EQ(targetsOfFirstCall("if ( OnDeath == 1 )\nJournal MT_Writ 80\nendif").mEvent, ScriptEvent::Death);
        EXPECT_EQ(targetsOfFirstCall("if ( GetHealth <= 0 )\nJournal MT_Writ 80\nendif").mEvent, ScriptEvent::Death);
        EXPECT_EQ(targetsOfFirstCall("if ( OnPCAdd == 1 )\nJournal MT_Writ 80\nendif").mEvent, ScriptEvent::PickUp);
        EXPECT_EQ(targetsOfFirstCall("if ( OnActivate == 1 )\nJournal MT_Writ 80\nendif").mEvent, ScriptEvent::Other);
    }

    TEST(MWGuiQuestTargetsCallTargetsTest, findsWhatThePlayerHasToBeNear)
    {
        const CallTargets near
            = targetsOfFirstCall("if ( Player->GetDistance \"sattir the bold\" <= 200 )\nJournal X 40\nendif");
        EXPECT_EQ(near.mNear, std::vector<ESM::RefId>{ id("sattir the bold") });
        const CallTargets self = targetsOfFirstCall("if ( GetDistance BM_KA_door <= 2100 )\nJournal X 60\nendif");
        EXPECT_EQ(self.mNear, std::vector<ESM::RefId>{ id("bm_ka_door") });
        const CallTargets toPlayer = targetsOfFirstCall("if ( GetDistance Player < 512 )\nJournal X 90\nendif");
        EXPECT_TRUE(toPlayer.mNear.empty());
    }

    TEST(MWGuiQuestTargetsCallTargetsTest, findsCellsDeadCountsAndItems)
    {
        const CallTargets targets = targetsOfFirstCall(
            "if ( GetPCCell, \"Gnaar Mok\" == 1 )\n"
            "if ( GetDeadCount \"rat_cave\" >= 3 )\n"
            "if ( Player->GetItemCount \"misc_box\" > 0 )\n"
            "if ( GetItemCount \"misc_other\" > 0 )\n"
            "Journal X 90\n");
        EXPECT_EQ(targets.mCells, std::vector<ESM::RefId>{ id("gnaar mok") });
        EXPECT_EQ(targets.mKills, std::vector<ESM::RefId>{ id("rat_cave") });
        EXPECT_EQ(targets.mFetches, std::vector<ESM::RefId>{ id("misc_box") });
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

    TEST(MWGuiQuestTargetsIndexTest, aScriptGoesWhereItsConditionsPoint)
    {
        Index index;
        index.addScript(
            "if ( GetPCCell \"Gnaar Mok\" == 1 )\nif ( GetDistance Player < 512 )\nJournal FG_RatHunt 90\n"
            "endif\nendif",
            { Index::Carrier{ id("pemenie"), true } });
        const std::vector<Way> ways = index.waysForward(quest, 60, Rule::Lowest);
        ASSERT_EQ(ways.size(), 1u);
        const std::vector<Objective> expected{ Objective{ .mTask = Task::Visit, .mCell = id("gnaar mok") } };
        EXPECT_EQ(ways[0].mObjectives, expected);
    }

    TEST(MWGuiQuestTargetsIndexTest, answersToChoicesOnlyCountWithoutAnotherWay)
    {
        Index index;
        index.addDialogue(line("gadayn andarys", "Journal FG_RatHunt 30",
            { condition(DialogueCondition::Function_Choice, "", DialogueCondition::Comp_Eq, 2) }));
        index.addDialogue(line("eraldil", "Journal FG_RatHunt 40"));
        const std::vector<Way> ways = index.waysForward(quest, 20, Rule::Lowest);
        ASSERT_EQ(ways.size(), 1u);
        EXPECT_EQ(ways[0].mObjectives, std::vector<Objective>{ talk("eraldil") });

        Index choicesOnly;
        choicesOnly.addDialogue(line("ashibaal", "Journal FG_RatHunt 50",
            { condition(DialogueCondition::Function_Choice, "", DialogueCondition::Comp_Eq, 1) }));
        const std::vector<Way> answers = choicesOnly.waysForward(quest, 20, Rule::Lowest);
        ASSERT_EQ(answers.size(), 1u);
        EXPECT_TRUE(answers[0].mChoice);
    }

    TEST(MWGuiQuestTargetsIndexTest, keepsConditionsOnGlobalsAndOtherQuestsToCheckWhilePlaying)
    {
        Index index;
        index.addDialogue(line("edryno arethi", "Journal FG_RatHunt 200",
            { stageIs(DialogueCondition::Comp_Ge, 10),
                condition(DialogueCondition::Function_Journal, "B3_ZainabKill", DialogueCondition::Comp_Ge, 1),
                condition(DialogueCondition::Function_Global, "MonopolyVotes", DialogueCondition::Comp_Gt, 2) }));
        const std::vector<Way> ways = index.waysForward(quest, 10, Rule::Lowest);
        ASSERT_EQ(ways.size(), 1u);
        ASSERT_EQ(ways[0].mChecks.size(), 2u);
        EXPECT_EQ(ways[0].mChecks[0].mVariable, "B3_ZainabKill");
        EXPECT_EQ(ways[0].mChecks[1].mVariable, "MonopolyVotes");
        EXPECT_TRUE(compare(ways[0].mChecks[0], 1.f));
        EXPECT_FALSE(compare(ways[0].mChecks[0], 0.f));
    }

    TEST(MWGuiQuestTargetsIndexTest, lowestAndHighestRuleKeepsBothEnds)
    {
        Index index;
        index.addDialogue(line("elone", "Journal FG_RatHunt 2"));
        index.addDialogue(line("bacola closcius", "Journal FG_RatHunt 10"));
        index.addDialogue(line("caius cosades", "Journal FG_RatHunt 14"));
        const std::vector<Way> ways = index.waysForward(quest, 1, Rule::LowestAndHighest);
        ASSERT_EQ(ways.size(), 2u);
        EXPECT_EQ(ways[0].mToStage, 2);
        EXPECT_EQ(ways[1].mToStage, 14);
    }

    TEST(MWGuiQuestTargetsIndexTest, aFactionLineKeepsItsRankAndCell)
    {
        Index index;
        ESM::DialInfo info = line("", "Journal FG_RatHunt 100");
        info.mFaction = id("Morag Tong");
        info.mCell = id("Balmora");
        info.mData.mRank = 5;
        index.addDialogue(info);
        const std::vector<Way> ways = index.waysForward(quest, 90, Rule::Lowest);
        ASSERT_EQ(ways.size(), 1u);
        const std::vector<Objective> expected{ Objective{
            .mTask = Task::Talk, .mFaction = id("Morag Tong"), .mCell = id("Balmora"), .mRank = 5 } };
        EXPECT_EQ(ways[0].mObjectives, expected);
    }

    TEST(MWGuiQuestTargetsIndexTest, findsHoldersAndMembers)
    {
        Index index;
        index.addHolder(id("apparatus_a_spipe_tsiya"), id("ernil omoran"));
        index.addHolder(id("apparatus_a_spipe_tsiya"), id("ernil omoran"));
        index.addMember(id("Morag Tong"), id("ethasi rilvayn"), 7);
        index.addMember(id("Morag Tong"), id("novice"), 1);
        EXPECT_EQ(index.holdersOf(id("apparatus_a_spipe_tsiya")), std::vector<ESM::RefId>{ id("ernil omoran") });
        EXPECT_TRUE(index.holdersOf(id("other")).empty());
        EXPECT_EQ(index.membersOf(id("Morag Tong"), 5), std::vector<ESM::RefId>{ id("ethasi rilvayn") });
        EXPECT_EQ(index.membersOf(id("Morag Tong"), -1).size(), 2u);
    }

    TEST(MWGuiQuestTargetsIndexTest, aCounterInsteadOfOnDeathIsNoKillOfTheScriptsActor)
    {
        Index index;
        index.addScript(
            "if ( OnDeath == 1 )\nJournal FG_RatHunt 105\nendif\n"
            "if ( riekkilled >= 5 )\nJournal FG_RatHunt 30\nendif",
            { Index::Carrier{ id("bm_bear_be_unique"), true } });
        const std::vector<Way> ways = index.waysForward(quest, 20, Rule::Lowest);
        ASSERT_EQ(ways.size(), 1u);
        const std::vector<Objective> expected{ Objective{ .mTask = Task::Visit, .mObject = id("bm_bear_be_unique") } };
        EXPECT_EQ(ways[0].mObjectives, expected);
    }
}
