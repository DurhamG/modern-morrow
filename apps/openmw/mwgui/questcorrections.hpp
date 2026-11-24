#ifndef MWGUI_QUESTCORRECTIONS_H
#define MWGUI_QUESTCORRECTIONS_H

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
        };

        /// Every quest's entries.
        struct Corrections
        {
            std::unordered_map<ESM::RefId, std::vector<Entry>> mQuests;

            /// The entry for a stage of a quest, or nullptr.
            const Entry* find(const ESM::RefId& quest, int stage) const;
        };

        /// What the loaded content has, for checking the entries against: each quest's journal indices.
        struct Content
        {
            std::unordered_map<ESM::RefId, std::set<int>> mStages;
        };

        /// Reads the corrections from the text of questcorrections.yaml. An entry that can't be read is left out, and
        /// a line about it is added to the problems. A file that can't be read at all gives no corrections.
        Corrections parse(const std::string& text, std::vector<std::string>& problems);

        /// Leaves out the entries for a quest the content doesn't have, and those that name journal indices it doesn't
        /// have, adding a line about each to the problems. A single index must be one of the quest's, and a range
        /// must have at least one of them. A mod that renumbers a quest then gets the markers worked out from the
        /// content, not wrong ones.
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
    }
}

#endif
