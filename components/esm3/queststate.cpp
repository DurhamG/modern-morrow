#include "queststate.hpp"

#include "esmreader.hpp"
#include "esmwriter.hpp"
#include "formatversion.hpp"

namespace ESM
{

    void QuestState::load(ESMReader& esm)
    {
        mTopic = esm.getHNRefId("YETO");
        esm.getHNOT(mState, "QSTA");
        esm.getHNOT(mFinished, "QFIN");
        mPinned = false;
        if (esm.getFormatVersion() > MaxNoPinnedQuestsFormatVersion)
        {
            unsigned char pinned = 0;
            esm.getHNOT(pinned, "QPIN");
            mPinned = pinned != 0;
        }
    }

    void QuestState::save(ESMWriter& esm) const
    {
        esm.writeHNRefId("YETO", mTopic);
        esm.writeHNT("QSTA", mState);
        esm.writeHNT("QFIN", mFinished);
        if (mPinned && esm.getFormatVersion() > MaxNoPinnedQuestsFormatVersion)
            esm.writeHNT("QPIN", static_cast<unsigned char>(1));
    }

}
