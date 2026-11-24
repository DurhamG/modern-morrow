#ifndef MWGUI_QUESTCORRECTIONS_H
#define MWGUI_QUESTCORRECTIONS_H

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <components/esm/refid.hpp>

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

        /// Reads the corrections from the text of questcorrections.yaml. An entry that can't be read is left out, and
        /// a line about it is added to the problems. A file that can't be read at all gives no corrections.
        Corrections parse(const std::string& text, std::vector<std::string>& problems);
    }
}

#endif
