#ifndef MWGUI_QUESTREVIEW_H
#define MWGUI_QUESTREVIEW_H

#include <set>
#include <string>
#include <vector>

#include <components/esm/refid.hpp>

#include "questcorrections.hpp"
#include "questtargets.hpp"

namespace MWGui
{
    /// The record of a review of the quest markers, which says what each reviewed stage of a quest should mark, and a
    /// check of the markers against it. A record lives in ui-rework/quests/reviewed/<quest>.yaml. See
    /// ui-rework/design/quest-marker-corrections.md.
    namespace QuestReview
    {
        /// What a stage marks, worked out from the content files alone, without the checks made while playing. A
        /// target is an object ID, "cell:" and a cell's name, or "faction:" and a faction's ID, all in lowercase.
        struct Shown
        {
            std::set<std::string> mMarked;
            // The targets that only optional ways give.
            std::set<std::string> mOptional;

            bool operator==(const Shown& other) const = default;
        };

        /// One reviewed stage.
        struct Stage
        {
            int mStage = 0;
            // What the reviewer found: "right" when the markers were right without a correction, "corrected" when a
            // correction makes them right, or "unfixable" when no marker can be right, such as for a stage that
            // depends on a random roll.
            std::string mVerdict;
            Shown mShown;
        };

        /// The review of one quest.
        struct Record
        {
            ESM::RefId mQuest;
            std::vector<Stage> mStages;
        };

        /// The target an objective names, as Shown has it.
        std::string targetOf(const QuestTargets::Objective& objective);

        /// What a stage of a quest marks, by the rule and the stage's correction, from the content files alone. The
        /// checks made while playing aren't made, so every way the rule chooses counts, and so does every one of a
        /// correction's targets, whatever its conditions.
        Shown shownAt(const QuestTargets::Index& index, const QuestCorrections::Corrections& corrections,
            const ESM::RefId& quest, int stage, QuestTargets::Rule rule);

        /// Reads a review record. A file that can't be read gives a record without stages, and a line about it is
        /// added to the problems.
        Record parseRecord(const std::string& text, std::vector<std::string>& problems);

        /// Writes a review record, as parseRecord reads it.
        std::string writeRecord(const Record& record);

        /// A difference between a review record and what the markers show now.
        struct Change
        {
            int mStage = 0;
            Shown mExpected;
            Shown mActual;
        };

        /// The reviewed stages whose markers have changed since the review.
        std::vector<Change> compare(const Record& record, const QuestTargets::Index& index,
            const QuestCorrections::Corrections& corrections, QuestTargets::Rule rule);
    }
}

#endif
