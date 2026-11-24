#include "apps/openmw/mwgui/topiclearning.hpp"

#include <gtest/gtest.h>

namespace
{
    using ESM::DialogueCondition;
    using MWGui::TopicLearning;

    const ESM::RefId muzgob = ESM::RefId::stringRefId("A1_4_MuzgobInformant");

    DialogueCondition stageIs(std::string_view quest, DialogueCondition::Comparison comparison, int value)
    {
        DialogueCondition result;
        result.mFunction = DialogueCondition::Function_Journal;
        result.mVariable = quest;
        result.mComparison = comparison;
        result.mValue = value;
        return result;
    }

    TopicLearning blades()
    {
        TopicLearning learning;
        learning.addTopic("Blades Apprentice");
        return learning;
    }

    TEST(MWGuiTopicLearningTest, aTopicNamedOnlyAtALaterStageShouldBeLearnedThen)
    {
        TopicLearning learning = blades();
        learning.addLine(
            "You're now a Blades Apprentice.", "", { stageIs("A1_4_MuzgobInformant", DialogueCondition::Comp_Ge, 20) });
        learning.addLine("Speak to me again as a blades apprentice.", "",
            { stageIs("A1_4_MuzgobInformant", DialogueCondition::Comp_Gt, 24) });
        EXPECT_EQ(learning.learnedAt("Blades Apprentice", muzgob), 20);
        EXPECT_EQ(learning.learnedAt("blades apprentice", muzgob), 20);
    }

    TEST(MWGuiTopicLearningTest, aLineWithoutConditionsShouldTeachTheTopicAtAnyStage)
    {
        TopicLearning learning = blades();
        learning.addLine(
            "You're now a Blades Apprentice.", "", { stageIs("A1_4_MuzgobInformant", DialogueCondition::Comp_Ge, 20) });
        learning.addLine("Every Blades Apprentice starts somewhere.", "", {});
        EXPECT_EQ(learning.learnedAt("Blades Apprentice", muzgob), 0);
    }

    TEST(MWGuiTopicLearningTest, aLineThatNeedsAnotherQuestShouldTeachTheTopicAtAnyStageOfThisOne)
    {
        TopicLearning learning = blades();
        learning.addLine("You're now a Blades Apprentice.", "",
            { stageIs("A1_2_AntabolisInformant", DialogueCondition::Comp_Ge, 20) });
        EXPECT_EQ(learning.learnedAt("Blades Apprentice", muzgob), 0);
    }

    TEST(MWGuiTopicLearningTest, aNameInsideAWordShouldNotTeachTheTopic)
    {
        TopicLearning learning;
        learning.addTopic("Ald");
        learning.addLine(
            "Balmora is not Ald'ruhn.", "", { stageIs("A1_4_MuzgobInformant", DialogueCondition::Comp_Ge, 20) });
        learning.addLine("The road to Bald Hill.", "", {});
        EXPECT_EQ(learning.learnedAt("Ald", muzgob), 20);
        // A name after a quote or a bracket starts a word.
        learning.addLine("Go to (Ald'ruhn).", "", {});
        EXPECT_EQ(learning.learnedAt("Ald", muzgob), 0);
    }

    TEST(MWGuiTopicLearningTest, addTopicInALineShouldTeachTheTopicAtTheLinesStages)
    {
        TopicLearning learning;
        learning.addTopic("necromancers");
        learning.addTopic("Blades Apprentice");
        learning.addLine("Bring me the skull.", "journal, \"a1_4_muzgobinformant\", 10\naddtopic necromancers",
            { stageIs("A1_4_MuzgobInformant", DialogueCondition::Comp_Ge, 1) });
        learning.addLine("Hm.", "AddTopic, \"Blades Apprentice\" ; a comment",
            { stageIs("A1_4_MuzgobInformant", DialogueCondition::Comp_Eq, 25) });
        EXPECT_EQ(learning.learnedAt("necromancers", muzgob), 1);
        EXPECT_EQ(learning.learnedAt("Blades Apprentice", muzgob), 25);
    }

    TEST(MWGuiTopicLearningTest, addTopicInAScriptShouldTeachTheTopicAtAnyStage)
    {
        TopicLearning learning = blades();
        learning.addLine(
            "You're now a Blades Apprentice.", "", { stageIs("A1_4_MuzgobInformant", DialogueCondition::Comp_Ge, 20) });
        learning.addScript("begin bladesscript\nif ( x == 1 )\n  AddTopic \"Blades Apprentice\"\nendif\nend");
        EXPECT_EQ(learning.learnedAt("Blades Apprentice", muzgob), 0);
    }

    TEST(MWGuiTopicLearningTest, aTopicNothingTeachesShouldCountAsKnownAtAnyStage)
    {
        TopicLearning learning = blades();
        EXPECT_EQ(learning.learnedAt("Blades Apprentice", muzgob), 0);
        EXPECT_EQ(learning.learnedAt("a topic nobody noted", muzgob), 0);
    }

    TEST(MWGuiTopicLearningTest, onlyNotedTopicsShouldBeLookedFor)
    {
        TopicLearning learning;
        learning.addLine(
            "You're now a Blades Apprentice.", "", { stageIs("A1_4_MuzgobInformant", DialogueCondition::Comp_Ge, 20) });
        EXPECT_EQ(learning.learnedAt("Blades Apprentice", muzgob), 0);
    }
}
