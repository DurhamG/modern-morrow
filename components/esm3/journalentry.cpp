#include "journalentry.hpp"

#include "esmreader.hpp"
#include "esmwriter.hpp"
#include "formatversion.hpp"

namespace ESM
{

    void JournalEntry::load(ESMReader& esm)
    {
        esm.getHNOT(mType, "JETY");
        mTopic = esm.getHNRefId("YETO");
        mInfo = esm.getHNRefId("YEIN");
        mText = esm.getHNString("TEXT");

        if (mType == Type_Journal)
        {
            esm.getHNT(mDay, "JEDA");
            esm.getHNT(mMonth, "JEMO");
            esm.getHNT(mDayOfMonth, "JEDM");
            if (esm.getFormatVersion() > MaxNoJournalSourceFormatVersion)
            {
                mActor = esm.getHNORefId("JEAC");
                mCell = esm.getHNORefId("JECE");
            }
        }
        else if (mType == Type_Topic)
            mActorName = esm.getHNOString("ACT_");
    }

    void JournalEntry::save(ESMWriter& esm) const
    {
        esm.writeHNT("JETY", mType);
        esm.writeHNRefId("YETO", mTopic);
        esm.writeHNRefId("YEIN", mInfo);
        esm.writeHNString("TEXT", mText);

        if (mType == Type_Journal)
        {
            esm.writeHNT("JEDA", mDay);
            esm.writeHNT("JEMO", mMonth);
            esm.writeHNT("JEDM", mDayOfMonth);
            esm.writeHNORefId("JEAC", mActor);
            esm.writeHNORefId("JECE", mCell);
        }
        else if (mType == Type_Topic)
            esm.writeHNString("ACT_", mActorName);
    }

}
