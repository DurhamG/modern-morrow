#include "uistate.hpp"

#include "esmreader.hpp"
#include "esmwriter.hpp"

namespace ESM
{
    void UiState::load(ESMReader& esm)
    {
        mRecentItems.clear();
        while (esm.isNextSub("RCIT"))
        {
            RecentItem& item = mRecentItems.emplace_back();
            item.mItem = esm.getRefId();
            unsigned char seen = 0;
            esm.getHNT(seen, "RCSN");
            item.mSeen = seen != 0;
        }

        mRecentSpells.clear();
        while (esm.isNextSub("RCSP"))
            mRecentSpells.push_back(esm.getRefId());

        mJournalSources.clear();
        while (esm.isNextSub("JSQU"))
        {
            JournalSource& source = mJournalSources.emplace_back();
            source.mQuest = esm.getRefId();
            source.mInfo = esm.getHNRefId("JSIN");
            esm.getHNT(source.mDay, "JSDA");
            source.mActor = esm.getHNORefId("JSAC");
            source.mCell = esm.getHNORefId("JSCE");
        }

        mPinnedQuests.clear();
        while (esm.isNextSub("QPIN"))
            mPinnedQuests.push_back(esm.getRefId());
    }

    void UiState::save(ESMWriter& esm) const
    {
        for (const RecentItem& item : mRecentItems)
        {
            esm.writeHNRefId("RCIT", item.mItem);
            esm.writeHNT("RCSN", static_cast<unsigned char>(item.mSeen ? 1 : 0));
        }

        for (const ESM::RefId& spell : mRecentSpells)
            esm.writeHNRefId("RCSP", spell);

        for (const JournalSource& source : mJournalSources)
        {
            esm.writeHNRefId("JSQU", source.mQuest);
            esm.writeHNRefId("JSIN", source.mInfo);
            esm.writeHNT("JSDA", source.mDay);
            esm.writeHNORefId("JSAC", source.mActor);
            esm.writeHNORefId("JSCE", source.mCell);
        }

        for (const ESM::RefId& quest : mPinnedQuests)
            esm.writeHNRefId("QPIN", quest);
    }
}
