#ifndef MWGUI_QUESTCORRECTIONS_H
#define MWGUI_QUESTCORRECTIONS_H

#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <components/esm/refid.hpp>

#include "questtargets.hpp"

namespace MWWorld
{
    class ESMStore;
}

namespace VFS
{
    class Manager;
}

namespace MWGui
{
    /// Hand-made corrections to the quest markers, read from questcorrections.yaml. QuestTargets works out each
    /// quest's ways forward from the content, but the content can't tell a side step, such as someone who only gives
    /// directions, from a step the quest needs. An entry says, at some of a quest's stages, which ways forward to mark
    /// and which to leave out. See ui-rework/design/quest-marker-corrections.md.
    namespace QuestCorrections
    {
        /// Journal indices from mFirst to mLast. A single index has both the same.
        struct Range
        {
            int mFirst = 0;
            int mLast = 0;

            bool contains(int index) const { return index >= mFirst && index <= mLast; }
            bool operator==(const Range& other) const = default;
        };

        bool contains(const std::vector<Range>& ranges, int index);

        /// When a target no longer needs marking: when a quest has reached a stage, or has finished.
        struct Done
        {
            ESM::RefId mQuest;
            // The stage the quest must reach, or 0 when only finishing counts.
            int mAtLeast = 0;
            bool mFinished = false;

            bool operator==(const Done& other) const = default;
        };

        /// Something to do that no line or script in the content names, such as talking to the people a journal entry
        /// lists. The target is an object, or else a cell.
        struct Target
        {
            QuestTargets::Task mTask = QuestTargets::Task::Talk;
            ESM::RefId mObject;
            ESM::RefId mCell;
            // How many of the object must be dead or carried, for Kill and Fetch.
            int mCount = 1;
            std::optional<Done> mDone;

            bool operator==(const Target& other) const = default;
        };

        /// What to mark at some of a quest's stages.
        struct Entry
        {
            // The stages the entry is for.
            std::vector<Range> mStages;
            // Mark only the ways that lead to these stages. Empty leaves the choice to the default rule.
            std::vector<Range> mNext;
            // Mark the ways that lead to these stages as optional, as side steps the player can take or leave.
            std::vector<Range> mOptional;
            // Never mark the ways that lead to these stages.
            std::vector<Range> mHide;
            // Mark these too, until each is done.
            std::vector<Target> mTargets;
            // Quests that help with this one, such as one for each person the journal sends the player to. While one
            // has started and not finished, what it marks is marked too, as if it were tracked.
            std::vector<ESM::RefId> mFollow;
        };

        /// Every quest's entries.
        struct Corrections
        {
            std::unordered_map<ESM::RefId, std::vector<Entry>> mQuests;

            /// The entry for a stage of a quest, or nullptr.
            const Entry* find(const ESM::RefId& quest, int stage) const;
        };

        /// What the loaded content has, for checking the entries against: each quest's journal indices, and whether an
        /// object or a cell exists.
        struct Content
        {
            std::unordered_map<ESM::RefId, std::set<int>> mStages;
            std::function<bool(const ESM::RefId&)> mHasObject;
            std::function<bool(const ESM::RefId&)> mHasCell;
        };

        /// How far the player's quests have got, for working out whether a target is done.
        struct Progress
        {
            // A quest's stage, 0 if it hasn't started.
            std::function<int(const ESM::RefId&)> mStage;
            std::function<bool(const ESM::RefId&)> mFinished;
        };

        /// Reads the corrections from the text of questcorrections.yaml. An entry that can't be read is left out, and
        /// a line about it is added to the problems. A file that can't be read at all gives no corrections.
        Corrections parse(const std::string& text, std::vector<std::string>& problems);

        /// Leaves out the entries for a quest the content doesn't have, and those that name journal indices, quests,
        /// objects or cells it doesn't have, adding a line about each to the problems. A single index must be one of
        /// the quest's, and a range must have at least one of them. A mod that renumbers a quest or removes a person
        /// then gets the markers worked out from the content, not wrong ones.
        void check(Corrections& corrections, const Content& content, std::vector<std::string>& problems);

        /// Reads questcorrections.yaml from the data files, and checks it against the loaded content. Each problem
        /// goes in the log as a warning. A game without the file has no corrections.
        Corrections load(const VFS::Manager& vfs, const MWWorld::ESMStore& store);

        /// Chooses the ways forward to mark, as QuestTargets::chooseWays does, with a stage's entry if it has one. The
        /// ways to the entry's hidden stages are left out. The ways to its optional stages are marked as optional,
        /// unless they are failures or out of reach. Of the other ways, if the entry has next stages, only the ways to
        /// them are marked, leaving out failures and ways out of reach. When none of those is left, such as when the
        /// person to talk to is dead, the rule chooses among the other ways.
        std::vector<QuestTargets::Way> chooseWays(
            std::vector<QuestTargets::Way> ways, const Entry* entry, QuestTargets::Rule rule);

        /// Whether a target's done condition holds. A target without one is never done.
        bool isDone(const Target& target, const Progress& progress);

        /// A way to mark for each of an entry's targets that isn't done. Each way has the target as its only
        /// objective, and leads to no stage, since the content doesn't say what it leads to.
        std::vector<QuestTargets::Way> targetWays(const Entry& entry, const Progress& progress);

        /// The quests an entry follows that have started and not finished.
        std::vector<ESM::RefId> followedQuests(const Entry& entry, const Progress& progress);
    }
}

#endif
