#ifndef OPENMW_ESM_UISTATE_H
#define OPENMW_ESM_UISTATE_H

#include <cstdint>
#include <vector>

#include <components/esm/refid.hpp>

namespace ESM
{
    class ESMReader;
    class ESMWriter;

    // format 0, saved games only

    /// An item that recently entered the player's inventory, and whether the player has seen it in a list.
    struct RecentItem
    {
        ESM::RefId mItem;
        bool mSeen = false;
    };

    /// What the controller menus remember about a game, in a record of its own. OpenMW versions without the
    /// controller menus skip the record, so they can load the save, and the rest of the save keeps their format.
    struct UiState
    {
        /// Who the player was talking to when a quest's journal entry was added, and where the player was. An entry
        /// is found again by its quest, its entry ID and its day, since the entries are saved in records of their own.
        struct JournalSource
        {
            ESM::RefId mQuest;
            ESM::RefId mInfo;
            int32_t mDay = 0;
            ESM::RefId mActor;
            ESM::RefId mCell;
        };

        // Items that recently entered the inventory, newest first
        std::vector<RecentItem> mRecentItems;
        // Spells, and enchanted items or scrolls, that the player recently readied or cast, most recent first
        std::vector<ESM::RefId> mRecentSpells;
        // The quest entries for which the game knows a source
        std::vector<JournalSource> mJournalSources;
        // The quests the player pinned to the top of the journal
        std::vector<ESM::RefId> mPinnedQuests;
        // The quests the player moved to the journal's Archived group, out of the way
        std::vector<ESM::RefId> mArchivedQuests;

        void load(ESMReader& esm);
        void save(ESMWriter& esm) const;
    };
}

#endif
