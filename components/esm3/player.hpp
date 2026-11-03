#ifndef OPENMW_ESM_PLAYER_H
#define OPENMW_ESM_PLAYER_H

#include <string>
#include <vector>

#include <components/esm/attr.hpp>
#include <components/esm/position.hpp>

#include "loadskil.hpp"
#include "npcstate.hpp"

namespace ESM
{
    class ESMReader;
    class ESMWriter;

    // format 0, saved games only

    struct Player
    {
        NpcState mObject;
        RefId mCellId;
        float mLastKnownExteriorPosition[3];
        unsigned char mHasMark;
        bool mSetWerewolfAcrobatics;
        Position mMarkedPosition;
        RefId mMarkedCell;
        ESM::RefId mBirthsign;

        int32_t mCurrentCrimeId;
        int32_t mPaidCrimeId;

        std::map<ESM::RefId, float> mSaveAttributes;
        std::map<ESM::RefId, float> mSaveSkills;

        std::map<ESM::RefId, ESM::RefId> mPreviousItems; // previous equipped items, needed for bound spells

        struct RecentItem
        {
            ESM::RefId mItem;
            bool mSeen = false;
        };
        // Items that recently entered the inventory, newest first
        std::vector<RecentItem> mRecentItems;
        // Spells, and enchanted items or scrolls, that the player recently readied or cast, most recent first
        std::vector<ESM::RefId> mRecentSpells;

        void load(ESMReader& esm);
        void save(ESMWriter& esm) const;
    };
}

#endif
