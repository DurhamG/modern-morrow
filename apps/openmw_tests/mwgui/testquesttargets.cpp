#include "apps/openmw/mwgui/questtargets.hpp"
#include "apps/openmw/mwgui/topiclearning.hpp"

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

    TEST(MWGuiQuestTargetsCallTargetsTest, aTestThatSomethingIsNotSoIsNoTarget)
    {
        // The Bal Isra stronghold's builder waits for the player to be somewhere else, and Dilborn's script for Thrud
        // to be alive.
        const CallTargets targets = targetsOfFirstCall(
            "if ( GetPCCell \"Bal Isra\" == 0 )\n"
            "if ( GetDeadCount \"thrud\" == 0 )\n"
            "if ( Player->GetItemCount \"misc_box\" < 1 )\n"
            "if ( GetDistance \"ex_door\" > 400 )\n"
            "Journal X 90\n");
        EXPECT_TRUE(targets.mCells.empty());
        EXPECT_TRUE(targets.mKills.empty());
        EXPECT_TRUE(targets.mFetches.empty());
        EXPECT_TRUE(targets.mNear.empty());
        EXPECT_EQ(targetsOfFirstCall("if ( OnDeath == 0 )\nJournal X 80\nendif").mEvent, ScriptEvent::Other);
        EXPECT_EQ(targetsOfFirstCall("if ( GetHealth > 50 )\nJournal X 80\nendif").mEvent, ScriptEvent::Other);
        EXPECT_EQ(targetsOfFirstCall("if ( GetPCCell \"Gnaar Mok\" )\nJournal X 90\nendif").mCells,
            std::vector<ESM::RefId>{ id("gnaar mok") });
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

    TEST(MWGuiQuestTargetsItemsTakenTest, findsItemsTakenFromThePlayer)
    {
        const std::vector<std::pair<ESM::RefId, int>> taken = itemsTaken(
            "Player->RemoveItem \"ebony_bow_auriel\" 1\nRemoveItem misc_other 1\nplayer->removeitem, gold_001, 500");
        const std::vector<std::pair<ESM::RefId, int>> expected{ { id("ebony_bow_auriel"), 1 },
            { id("gold_001"), 500 } };
        EXPECT_EQ(taken, expected);
    }

    TEST(MWGuiQuestTargetsTest, choicesOfferedAreTheAnswersNumbers)
    {
        EXPECT_EQ(choicesOffered("Choice \"Yes, here it is.\" 1 \"Not yet.\" 2"), (std::vector<int>{ 1, 2 }));
        EXPECT_EQ(choicesOffered("ModDisposition 5\nchoice, \"Take it\", 3 ; a comment"), (std::vector<int>{ 3 }));
        EXPECT_TRUE(choicesOffered("Journal FG_RatHunt 10").empty());
        EXPECT_EQ(choicesOffered("Choice: \"Help the colony\" 1 \"Refuse\" 2"), (std::vector<int>{ 1, 2 }));
    }

    TEST(MWGuiQuestTargetsTest, anAnswerInheritsTheConditionsOfTheLineThatOffersIt)
    {
        // Shadbak asks for the item only once the player carries it, and the answer moves the quest on.
        const DialogueCondition carries
            = condition(DialogueCondition::Function_Item, "misc_skull_llevule", DialogueCondition::Comp_Ge, 1);
        const DialogueCondition answerOne
            = condition(DialogueCondition::Function_Choice, "", DialogueCondition::Comp_Eq, 1);
        const std::vector<ESM::DialInfo> lines = withOfferingConditions({
            line("shadbak", "Choice \"Give it\" 1 \"Keep it\" 2", { carries }),
            line("", "Journal FG_RatHunt 30", { answerOne }),
        });
        ASSERT_EQ(lines[1].mSelects.size(), 2u);
        EXPECT_EQ(lines[1].mSelects[1].mVariable, "misc_skull_llevule");
        EXPECT_EQ(lines[1].mActor, id("shadbak"));
        // The offering line itself is unchanged.
        EXPECT_EQ(lines[0].mSelects.size(), 1u);
    }

    TEST(MWGuiQuestTargetsTest, anAnswerOfferedByLinesWithOtherConditionsInheritsOnlyWhatTheyShare)
    {
        const DialogueCondition early = stageIs(DialogueCondition::Comp_Ls, 20);
        const DialogueCondition late = stageIs(DialogueCondition::Comp_Ge, 20);
        const DialogueCondition rich
            = condition(DialogueCondition::Function_Item, "Gold_001", DialogueCondition::Comp_Ge, 100);
        const DialogueCondition answerOne
            = condition(DialogueCondition::Function_Choice, "", DialogueCondition::Comp_Eq, 1);
        const std::vector<ESM::DialInfo> lines = withOfferingConditions({
            line("shadbak", "Choice \"Pay\" 1", { early, rich }),
            line("eydis fire-eye", "Choice \"Pay\" 1", { late, rich }),
            line("", "Journal FG_RatHunt 30", { answerOne }),
        });
        ASSERT_EQ(lines[2].mSelects.size(), 2u);
        EXPECT_EQ(lines[2].mSelects[1].mVariable, "Gold_001");
        EXPECT_TRUE(lines[2].mActor.empty());
    }

    TEST(MWGuiQuestTargetsTest, anAnswerNeedsTheStageItsOfferingLineSetsNotTheOneItNeeds)
    {
        // Big Helende starts the quest and asks at once, so by the answer the quest is no longer at 0.
        const std::vector<ESM::DialInfo> lines = withOfferingConditions({
            line("big helende", "Journal FG_RatHunt 10\nChoice \"Yes\" 1", { stageIs(DialogueCondition::Comp_Eq, 0) }),
            line("", "Journal FG_RatHunt 20",
                { condition(DialogueCondition::Function_Choice, "", DialogueCondition::Comp_Eq, 1) }),
        });
        // The answer needs the stage the offering line sets, not the one it needs.
        ASSERT_EQ(lines[1].mSelects.size(), 2u);
        EXPECT_EQ(id(lines[1].mSelects[1].mVariable), id("FG_RatHunt"));
        EXPECT_EQ(lines[1].mSelects[1].mComparison, DialogueCondition::Comp_Eq);
        EXPECT_EQ(std::get<int32_t>(lines[1].mSelects[1].mValue), 10);
        EXPECT_EQ(lines[1].mActor, id("big helende"));
    }

    TEST(MWGuiQuestTargetsIndexTest, anAnswerIsOnlyAWayAtTheStageItsOfferingLineSets)
    {
        Index index;
        for (const ESM::DialInfo& info : withOfferingConditions({
                 line("big helende", "Journal FG_RatHunt 10\nChoice \"Yes\" 1"),
                 line("", "Journal FG_RatHunt 20",
                     { condition(DialogueCondition::Function_Choice, "", DialogueCondition::Comp_Eq, 1) }),
             }))
            index.addDialogue(info);
        EXPECT_EQ(index.waysForward(quest, 10).size(), 1u);
        // Later, the conversation that offered the answer is over.
        EXPECT_TRUE(index.waysForward(quest, 15).empty());
    }

    TEST(MWGuiQuestTargetsIndexTest, goldALineNeedsIsACheckNotAFetch)
    {
        Index index;
        index.addDialogue(line("socucius ergalla", "Journal FG_RatHunt 50",
            { condition(DialogueCondition::Function_Item, "Gold_001", DialogueCondition::Comp_Ge, 200) }));
        const std::vector<Way> ways = index.waysForward(quest, 10);
        ASSERT_EQ(ways.size(), 1u);
        EXPECT_EQ(ways[0].mObjectives, (std::vector<Objective>{ talk("socucius ergalla") }));
        ASSERT_EQ(ways[0].mChecks.size(), 1u);
        EXPECT_EQ(ways[0].mChecks[0].mVariable, "Gold_001");
    }

    TEST(MWGuiQuestTargetsIndexTest, anAnswerThatNeedsAnItemIsAFetchFirst)
    {
        Index index;
        for (const ESM::DialInfo& info : withOfferingConditions({
                 line("shadbak", "Choice \"Give it\" 1",
                     { condition(
                         DialogueCondition::Function_Item, "misc_skull_llevule", DialogueCondition::Comp_Ge, 1) }),
                 line("", "Journal FG_RatHunt 30",
                     { condition(DialogueCondition::Function_Choice, "", DialogueCondition::Comp_Eq, 1) }),
             }))
            index.addDialogue(info);
        const std::vector<Way> ways = index.waysForward(quest, 10, Rule::All);
        ASSERT_EQ(ways.size(), 1u);
        const std::vector<Objective> skull{
            Objective{ .mTask = Task::Fetch, .mObject = id("misc_skull_llevule") },
            talk("shadbak"),
        };
        EXPECT_EQ(ways[0].mObjectives, skull);
        EXPECT_TRUE(ways[0].mChoice);
    }

    TEST(MWGuiQuestTargetsIndexTest, anItemTheLineTakesIsAFetch)
    {
        Index index;
        index.addDialogue(line("therana",
            "Player->RemoveItem \"ebony_bow_auriel\" 1\nPlayer->RemoveItem gold_001 100\nJournal FG_RatHunt 100",
            { condition(DialogueCondition::Function_Item, "ebony_bow_auriel", DialogueCondition::Comp_Ge, 1) }));
        index.addDialogue(line("eydis fire-eye", "Player->RemoveItem misc_box 2\nJournal FG_RatHunt 50"));
        const std::vector<Way> ways = index.waysForward(quest, 10, Rule::All);
        ASSERT_EQ(ways.size(), 2u);
        const std::vector<Objective> box{
            Objective{ .mTask = Task::Fetch, .mObject = id("misc_box"), .mCount = 2 },
            talk("eydis fire-eye"),
        };
        EXPECT_EQ(ways[0].mObjectives, box);
        const std::vector<Objective> bow{
            Objective{ .mTask = Task::Fetch, .mObject = id("ebony_bow_auriel") },
            talk("therana"),
        };
        EXPECT_EQ(ways[1].mObjectives, bow);
    }

    TEST(MWGuiQuestTargetsIndexTest, needingAQuestSpeakerDeadIsAFailure)
    {
        Index index;
        index.addDialogue(line("drarayne thelas", "Journal FG_RatHunt 50"));
        index.addDialogue(line("eydis fire-eye", "Journal FG_RatHunt 40",
            { condition(DialogueCondition::Function_Dead, "drarayne thelas", DialogueCondition::Comp_Eq, 1) }));
        const std::vector<Way> ways = index.waysForward(quest, 10, Rule::Lowest);
        ASSERT_EQ(ways.size(), 1u);
        EXPECT_EQ(ways[0].mToStage, 50);
    }

    TEST(MWGuiQuestTargetsIndexTest, aLineInATopicThePlayerCantKnowYetIsNotAWayForward)
    {
        // Only the lines for stage 20 on name the topic that the line to 55 is in.
        Index index;
        index.addDialogue(line("caius cosades", "Journal FG_RatHunt 55"), "Blades Apprentice");
        index.addDialogue(line("caius cosades", "Journal FG_RatHunt 30"), "Orders");
        MWGui::TopicLearning learning;
        learning.addTopic("Blades Apprentice");
        learning.addLine("You are now a Blades Apprentice.", "", { stageIs(DialogueCondition::Comp_Ge, 20) });
        index.learnTopics(learning);
        EXPECT_EQ(index.waysForward(quest, 10, Rule::All).size(), 1u);
        EXPECT_EQ(index.waysForward(quest, 10, Rule::All)[0].mToStage, 30);
        EXPECT_EQ(index.waysForward(quest, 20, Rule::All).size(), 2u);
    }

    TEST(MWGuiQuestTargetsIndexTest, linesWithOtherChecksAreOtherWays)
    {
        // Velanda Omani answers once either of two other quests has got far enough. If the ways were one, the game
        // would test only the first line's check, and leave her out while the second holds.
        Index index;
        index.addDialogue(line("velanda omani", "Journal FG_RatHunt 30",
            { condition(DialogueCondition::Function_Journal, "HH_WinCamonna", DialogueCondition::Comp_Ge, 50) }));
        index.addDialogue(line("velanda omani", "Journal FG_RatHunt 30",
            { condition(DialogueCondition::Function_Journal, "B6_DrenHortator", DialogueCondition::Comp_Ge, 50) }));
        index.addDialogue(line("velanda omani", "Journal FG_RatHunt 30",
            { condition(DialogueCondition::Function_Journal, "B6_DrenHortator", DialogueCondition::Comp_Ge, 50) }));
        const std::vector<Way> ways = index.waysForward(quest, 20);
        ASSERT_EQ(ways.size(), 2u);
        EXPECT_EQ(ways[0].mChecks[0].mVariable, "HH_WinCamonna");
        EXPECT_EQ(ways[1].mChecks[0].mVariable, "B6_DrenHortator");
    }

    TEST(MWGuiQuestTargetsIndexTest, theStagesSetAreEveryStageALineOrScriptMovesTheQuestTo)
    {
        Index index;
        index.addDialogue(line("bulfim", "Journal FG_RatHunt 110"));
        index.addDialogue(line("eydis fire-eye", "Journal FG_RatHunt 50"));
        index.addScript("begin ratscript\nJournal FG_RatHunt 200\nend", {});
        EXPECT_EQ(index.stagesSet(quest), (std::set<int>{ 50, 110, 200 }));
    }

    TEST(MWGuiQuestTargetsIndexTest, aWayKeepsTheFactionRankItsLineNeedsOfThePlayer)
    {
        ESM::DialInfo master = line("llunela hleran", "Journal FG_RatHunt 210");
        master.mPcFaction = id("telvanni");
        master.mData.mPCrank = 6;
        // A rank without a faction means the speaker's faction.
        ESM::DialInfo member = line("aryon", "Journal FG_RatHunt 220");
        member.mFaction = id("telvanni");
        member.mData.mPCrank = 2;
        Index index;
        index.addDialogue(master);
        index.addDialogue(member);
        index.addDialogue(line("vedam dren", "Journal FG_RatHunt 100"));
        const std::vector<Way> ways = index.waysForward(quest, 10, Rule::All);
        ASSERT_EQ(ways.size(), 3u);
        EXPECT_TRUE(ways[0].mPlayerFaction.empty());
        EXPECT_EQ(ways[1].mPlayerFaction, id("telvanni"));
        EXPECT_EQ(ways[1].mPlayerRank, 6);
        EXPECT_EQ(ways[2].mPlayerFaction, id("telvanni"));
        EXPECT_EQ(ways[2].mPlayerRank, 2);
    }

    TEST(MWGuiQuestTargetsTest, onlyACorrectionsVisitOrFetchMarksABody)
    {
        Way handMade;
        handMade.mHandMade = true;
        EXPECT_TRUE(marksTheDead(handMade, Task::Visit));
        EXPECT_TRUE(marksTheDead(handMade, Task::Fetch));
        EXPECT_FALSE(marksTheDead(handMade, Task::Talk));
        EXPECT_FALSE(marksTheDead(handMade, Task::Kill));
        // The analysis's visits to actors come from scripts that wait for their deaths.
        EXPECT_FALSE(marksTheDead(Way{}, Task::Visit));
    }

    TEST(MWGuiQuestTargetsTest, thePlayersRankMustAllowTheWay)
    {
        Way way;
        EXPECT_TRUE(playerRankAllows(way, {}));
        way.mPlayerFaction = id("telvanni");
        way.mPlayerRank = 6;
        EXPECT_FALSE(playerRankAllows(way, {}));
        EXPECT_FALSE(playerRankAllows(way, { { id("telvanni"), 5 } }));
        EXPECT_TRUE(playerRankAllows(way, { { id("telvanni"), 6 } }));
        way.mPlayerRank = -1;
        EXPECT_TRUE(playerRankAllows(way, { { id("telvanni"), 0 } }));
        EXPECT_FALSE(playerRankAllows(way, { { id("redoran"), 9 } }));
    }

    TEST(MWGuiQuestTargetsIndexTest, aFailureCountsWhenItIsTheOnlyWay)
    {
        // Hanarai Assutlanipal attacks the player, and the quest goes on once she is dead.
        Index index;
        index.addDialogue(line("hanarai assutlanipal", "Journal FG_RatHunt 80"));
        index.addDialogue(line("lloros sarano", "Journal FG_RatHunt 100",
            { condition(DialogueCondition::Function_Dead, "hanarai assutlanipal", DialogueCondition::Comp_Ge, 1) }));
        const std::vector<Way> ways = index.waysForward(quest, 80, Rule::Lowest);
        ASSERT_EQ(ways.size(), 1u);
        EXPECT_TRUE(ways[0].mFailure);
        EXPECT_EQ(ways[0].mToStage, 100);
    }

    TEST(MWGuiQuestTargetsIndexTest, aCompanionDyingIsAFailure)
    {
        // Pemenie follows the player to Gnaar Mok. Her death is one way forward, and arriving is the other.
        Index index;
        index.addDialogue(line("pemenie", "AiFollow Player 0 0 0 0\nJournal Other_Quest 10"));
        index.addScript(
            "if ( OnDeath == 1 )\nJournal FG_RatHunt 110\nendif\n"
            "if ( GetPCCell \"Gnaar Mok\" == 1 )\nJournal FG_RatHunt 90\nendif",
            { Index::Carrier{ id("pemenie"), true } });
        EXPECT_TRUE(index.isCompanion(id("pemenie")));
        const std::vector<Way> ways = index.waysForward(quest, 60, Rule::LowestAndHighest);
        ASSERT_EQ(ways.size(), 1u);
        EXPECT_EQ(ways[0].mToStage, 90);
    }

    TEST(MWGuiQuestTargetsChooseWaysTest, waysOutOfReachOnlyCountWithoutAnother)
    {
        // Almalexia wants the reforged blade, which doesn't exist until Radac has the Pyroil Tar.
        Way tar{ .mToStage = 80, .mObjectives = { talk("dwarven ghost_radac") } };
        Way blade{ .mToStage = 100, .mObjectives = { talk("almalexia") }, .mOutOfReach = true };
        const std::vector<Way> ways = chooseWays({ tar, blade }, Rule::LowestAndHighest);
        ASSERT_EQ(ways.size(), 1u);
        EXPECT_EQ(ways[0].mToStage, 80);
        const std::vector<Way> only = chooseWays({ blade }, Rule::LowestAndHighest);
        ASSERT_EQ(only.size(), 1u);
        EXPECT_EQ(only[0].mToStage, 100);
    }

    TEST(MWGuiQuestTargetsIndexTest, aScriptCanMakeAnotherActorACompanion)
    {
        Index index;
        index.addScript("\"rabinna\"->AiFollow, player, 0, 0, 0, 0", {});
        index.addScript("AiEscort Player 0 100 200 300", { Index::Carrier{ id("fonus rathryon"), true } });
        index.addScript("AiFollow \"guar_white\" 0 0 0 0", { Index::Carrier{ id("scamp"), true } });
        EXPECT_TRUE(index.isCompanion(id("rabinna")));
        EXPECT_TRUE(index.isCompanion(id("fonus rathryon")));
        EXPECT_FALSE(index.isCompanion(id("scamp")));
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

    TEST(MWGuiQuestTargetsIndexTest, answersToChoicesAreWaysForward)
    {
        // Caius Cosades only moves the quest on through answers, and he is where it goes, past Bacola's directions.
        Index index;
        index.addDialogue(line("caius cosades", "Journal FG_RatHunt 14",
            { condition(DialogueCondition::Function_Choice, "", DialogueCondition::Comp_Eq, 3) }));
        index.addDialogue(line("bacola closcius", "Journal FG_RatHunt 10"));
        const std::vector<Way> ways = index.waysForward(quest, 2, Rule::LowestAndHighest);
        ASSERT_EQ(ways.size(), 2u);
        EXPECT_EQ(ways[0].mObjectives, std::vector<Objective>{ talk("bacola closcius") });
        EXPECT_EQ(ways[1].mObjectives, std::vector<Objective>{ talk("caius cosades") });
        EXPECT_TRUE(ways[1].mChoice);
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

    TEST(MWGuiQuestTargetsScriptJournalCallsTest, anEarlyReturnGuardsTheCommandsAfterIt)
    {
        const std::vector<ScriptJournalCall> calls = scriptJournalCalls(
            "if ( GetJournalIndex FG_RatHunt < 50 )\nreturn\nendif\n"
            "if ( GetPCCell \"Pelagiad, Halfway Tavern\" == 1 )\nJournal FG_RatHunt 70\nendif");
        ASSERT_EQ(calls.size(), 1u);
        ASSERT_EQ(calls[0].mGuards.size(), 1u);
        const std::vector<ESM::DialogueCondition> checks = stageChecks(calls[0]);
        ASSERT_EQ(checks.size(), 1u);
        EXPECT_EQ(checks[0].mComparison, DialogueCondition::Comp_Ge);
        EXPECT_FALSE(allowsStage(checks, quest, 10));
        EXPECT_TRUE(allowsStage(checks, quest, 50));
    }

    TEST(MWGuiQuestTargetsScriptJournalCallsTest, aBlockWithAnElseIsNoGuard)
    {
        const std::vector<ScriptJournalCall> calls = scriptJournalCalls(
            "if ( GetJournalIndex FG_RatHunt < 50 )\nreturn\nelse\nset x to 1\nendif\n"
            "Journal FG_RatHunt 70");
        ASSERT_EQ(calls.size(), 1u);
        EXPECT_TRUE(calls[0].mGuards.empty());
    }

    TEST(MWGuiQuestTargetsIndexTest, aScriptsStageChecksRuleOutStages)
    {
        Index index;
        index.addScript(
            "if ( GetJournalIndex FG_RatHunt < 50 )\nreturn\nendif\n"
            "if ( GetPCCell \"Pelagiad, Halfway Tavern\" == 1 )\nJournal FG_RatHunt 70\nendif",
            { Index::Carrier{ id("itermerel"), true } });
        index.addDialogue(line("itermerel", "Journal FG_RatHunt 50"));
        const std::vector<Way> early = index.waysForward(quest, 10, Rule::All);
        ASSERT_EQ(early.size(), 1u);
        EXPECT_EQ(early[0].mObjectives, std::vector<Objective>{ talk("itermerel") });
        const std::vector<Way> later = index.waysForward(quest, 50, Rule::All);
        ASSERT_EQ(later.size(), 1u);
        EXPECT_EQ(later[0].mToStage, 70);
    }

    TEST(MWGuiQuestTargetsIndexTest, aScriptsCheckOnAnotherQuestIsCheckedWhilePlaying)
    {
        Index index;
        index.addScript("if ( GetJournalIndex Other_Quest >= 100 )\nJournal FG_RatHunt 70\nendif",
            { Index::Carrier{ id("itermerel"), true } });
        const std::vector<Way> ways = index.waysForward(quest, 10, Rule::All);
        ASSERT_EQ(ways.size(), 1u);
        ASSERT_EQ(ways[0].mChecks.size(), 1u);
        EXPECT_EQ(ways[0].mChecks[0].mVariable, "other_quest");
    }
}
