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

        /// Something that holds or not while playing, for when a target starts or stops needing a marker: a quest has
        /// reached a stage or has finished, someone is dead, the player carries an item, or a global variable has
        /// reached a value. Exactly one is named.
        struct Condition
        {
            ESM::RefId mQuest;
            // The stage the quest must reach, or 0 when only finishing counts. For a global variable, the value it
            // must reach, and for a carried item, how many the player must carry, where 0 counts as 1.
            int mAtLeast = 0;
            bool mFinished = false;
            // Someone who must have died.
            ESM::RefId mDead;
            // An item the player must carry.
            ESM::RefId mCarried;
            // A global variable, such as one that counts votes.
            std::string mGlobal;

            bool operator==(const Condition& other) const = default;
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
            // Whether the target is a side step, marked as optional.
            bool mOptional = false;
            // The target is marked only once all of these hold.
            std::vector<Condition> mAfter;
            // The target isn't marked once this holds.
            std::optional<Condition> mDone;

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
            // The fingerprint of each corrected quest's shape in the content the entries were written for.
            std::unordered_map<ESM::RefId, std::string> mFingerprints;

            /// The entry for a stage of a quest, or nullptr.
            const Entry* find(const ESM::RefId& quest, int stage) const;
        };

        /// What the loaded content has, for checking the entries against: each quest's journal indices, and whether an
        /// object or a cell exists.
        struct Content
        {
            std::unordered_map<ESM::RefId, std::set<int>> mStages;
            // The stages lines and scripts move each quest to, which can have no journal entry.
            std::unordered_map<ESM::RefId, std::set<int>> mSetStages;
            std::function<bool(const ESM::RefId&)> mHasObject;
            std::function<bool(const ESM::RefId&)> mHasCell;
            std::function<bool(std::string_view)> mHasGlobal;
            // A quest's fingerprint in the loaded content.
            std::function<std::string(const ESM::RefId&)> mFingerprint;
        };

        /// How far the player has got, for working out whether a target's conditions hold. A function left empty
        /// counts as false.
        struct Progress
        {
            // A quest's stage, 0 if it hasn't started.
            std::function<int(const ESM::RefId&)> mStage;
            std::function<bool(const ESM::RefId&)> mFinished;
            // Whether anyone of the ID has died.
            std::function<bool(const ESM::RefId&)> mDead;
            // How many of the item the player carries.
            std::function<int(const ESM::RefId&)> mCarried;
            // A global variable's value.
            std::function<float(std::string_view)> mGlobal;
        };

        /// Reads the corrections from the text of questcorrections.yaml. An entry that can't be read is left out, and
        /// a line about it is added to the problems. A file that can't be read at all gives no corrections.
        Corrections parse(const std::string& text, std::vector<std::string>& problems);

        /// A short text that changes when any of the facts change, whatever their order: a 64-bit FNV-1a hash of
        /// the sorted facts, in hexadecimal.
        std::string fingerprint(std::vector<std::string> facts);

        /// A quest's fingerprint in the content: its journal indices and the facts of how it moves on.
        std::string fingerprintOf(
            const ESM::RefId& quest, const std::set<int>& stages, const QuestTargets::Index& index);

        /// Leaves out the entries for a quest whose fingerprint differs from the loaded content's, such as one a mod
        /// rewrote. Leaves out the entries for a quest the content doesn't have, and those that name journal indices,
        /// quests, objects or cells it doesn't have, adding a line about each to the problems. A single index must be
        /// one of the quest's, and a range must have at least one of them. A stage that a line or script sets counts
        /// as one of the quest's, even without a journal entry. A mod that renumbers a quest or removes a
        /// person then gets the markers worked out from the content, not wrong ones.
        void check(Corrections& corrections, const Content& content, std::vector<std::string>& problems);

        /// What the loaded content has, for checking entries against it.
        Content contentOf(const MWWorld::ESMStore& store, const QuestTargets::Index& index);

        /// Reads questcorrections.yaml from the data files, and checks it against the loaded content. Each problem
        /// goes in the log as a warning, and in the problems when they are given. A game without the file has no
        /// corrections.
        Corrections load(const VFS::Manager& vfs, const MWWorld::ESMStore& store, const QuestTargets::Index& index,
            std::vector<std::string>* problems = nullptr);

        /// Chooses the ways forward to mark, as QuestTargets::chooseWays does, with a stage's entry if it has one. The
        /// ways to the entry's hidden stages are left out. The ways to its optional stages are marked as optional,
        /// unless they are failures or out of reach. Of the other ways, if the entry has next stages, only the ways to
        /// them are marked, leaving out ways out of reach, and failures unless every one of them is a failure. So a
        /// kill the journal asks for, of someone who talks in the quest, is marked when the entry names its stage. When
        /// none of those is left, such as when the person to talk to is dead, the rule chooses among the other ways.
        std::vector<QuestTargets::Way> chooseWays(
            std::vector<QuestTargets::Way> ways, const Entry* entry, QuestTargets::Rule rule);

        /// Whether a condition holds.
        bool holds(const Condition& condition, const Progress& progress);

        /// A way to mark for each of an entry's targets whose after conditions all hold, and whose done condition
        /// doesn't. Without progress, every target counts, as when reviewing what a stage can mark. Each
        /// way has the target as its only objective, and leads to no stage, since the content doesn't say what it
        /// leads to.
        std::vector<QuestTargets::Way> targetWays(const Entry& entry, const Progress* progress);

        /// The entries that change nothing the rule would mark at any of their stages that the content has, and
        /// have no targets and follow no quests. Each is named by its quest and the first of its stages.
        std::vector<std::string> entriesWithoutEffect(const Corrections& corrections, const QuestTargets::Index& index,
            const Content& content, QuestTargets::Rule rule);

        /// The quests an entry follows that have started and not finished.
        std::vector<ESM::RefId> followedQuests(const Entry& entry, const Progress& progress);
    }
}

#endif
