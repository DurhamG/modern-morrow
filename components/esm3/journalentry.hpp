#ifndef OPENMW_ESM_JOURNALENTRY_H
#define OPENMW_ESM_JOURNALENTRY_H

#include <components/esm/refid.hpp>

#include <cstdint>
#include <string>

namespace ESM
{
    class ESMReader;
    class ESMWriter;

    // format 0, saved games only

    struct JournalEntry
    {
        enum Type
        {
            Type_Journal = 0,
            Type_Topic = 1,
            Type_Quest = 2
        };

        int32_t mType;
        ESM::RefId mTopic;
        ESM::RefId mInfo;
        std::string mText;
        std::string mActorName; // Could also be Actor ID to allow switching of localisation, but since mText is
                                // plaintext anyway...
        int32_t mDay; // time stamp
        int32_t mMonth;
        int32_t mDayOfMonth;
        // For quest entries: the actor the player was talking to when the entry was added, if any, and the cell the
        // player was in. Only saves at formats 42 and 43 have them here. Newer saves keep them in ESM::UiState.
        ESM::RefId mActor;
        ESM::RefId mCell;

        void load(ESMReader& esm);
        void save(ESMWriter& esm) const;
    };
}

#endif
