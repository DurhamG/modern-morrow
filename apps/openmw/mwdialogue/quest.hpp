#ifndef GAME_MWDIALOG_QUEST_H
#define GAME_MWDIALOG_QUEST_H

#include "topic.hpp"

namespace ESM
{
    struct QuestState;
}

namespace MWDialogue
{
    /// \brief A quest in progress or a completed quest
    class Quest : public Topic
    {
        int mIndex;
        bool mFinished;
        bool mPinned = false;
        bool mArchived = false;

    public:
        Quest();

        Quest(const ESM::RefId& topic);

        Quest(const ESM::QuestState& state);

        std::string_view getName() const override;
        ///< May be an empty string

        int getIndex() const;

        void setIndex(int index);
        ///< Calling this function with a non-existent index will throw an exception.

        bool isFinished() const;
        void setFinished(bool finished);

        /// Whether the player pinned the quest to the top of the journal.
        bool isPinned() const { return mPinned; }
        void setPinned(bool pinned) { mPinned = pinned; }

        /// Whether the player moved the quest to the journal's Archived group, out of the way.
        bool isArchived() const { return mArchived; }
        void setArchived(bool archived) { mArchived = archived; }

        bool addEntry(const JournalEntry& entry) override;
        ///< Add entry and adjust index accordingly. Returns true if the quest should be restarted.
        ///
        /// \note Redundant entries are ignored, but the index is still adjusted.

        void write(ESM::QuestState& state) const;
    };
}

#endif
