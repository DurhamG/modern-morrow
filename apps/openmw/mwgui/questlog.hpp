#ifndef MWGUI_QUESTLOG_H
#define MWGUI_QUESTLOG_H

#include <cstddef>
#include <string>
#include <vector>

#include <components/esm/refid.hpp>

namespace MWDialogue
{
    struct StampedJournalEntry;
}

namespace MWGui
{
    /// The quests in the player's journal, as the controller journal screen lists them. Quests that share a name are
    /// one quest, as in the journal book.
    namespace QuestLog
    {
        struct Quest
        {
            std::string mName;
            bool mFinished = false;
            // Whether the player pinned the quest to the top of the journal
            bool mPinned = false;
            // The IDs of the game's quests that share the name
            std::vector<ESM::RefId> mIds;
            // The quest's journal entries, oldest first
            std::vector<const MWDialogue::StampedJournalEntry*> mEntries;
            // Where the quest's newest entry is in the journal. A higher number is more recent.
            std::size_t mLastActivity = 0;
            // Who gave the quest and which faction it is for. Either can be empty.
            ESM::RefId mGiver;
            ESM::RefId mFaction;
            // Where the quest started, such as "Balmora, Guild of Fighters", and the town or region that place is in,
            // such as "Balmora". Both are empty when the game can't tell.
            std::string mPlace;
            std::string mTown;
        };

        /// The quests the player has started, with the most recent activity first.
        ///
        /// Journal entries added since the game started recording it know who the player was talking to and where
        /// the player was. For older entries, the quest giver is the person whose conversation sets the quest's
        /// earliest stage, and the place is where that person stands.
        std::vector<Quest> collect();

        /// Pins the quest to the top of the journal, or unpins it, with all the IDs that share its name.
        void setPinned(Quest& quest, bool pinned);

        std::string giverName(const Quest& quest);
        std::string factionName(const Quest& quest);

        /// When the entry was added, such as "16 Last Seed (Day 1)".
        std::string timestamp(const MWDialogue::StampedJournalEntry& entry);
    }
}

#endif
