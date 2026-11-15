#ifndef OPENMW_TEST_SUITE_MWDIALOGUE_JOURNALFIXTURE_H
#define OPENMW_TEST_SUITE_MWDIALOGUE_JOURNALFIXTURE_H

#include "apps/openmw/mwbase/environment.hpp"
#include "apps/openmw/mwdialogue/journalimp.hpp"
#include "apps/openmw/mwlua/luamanagerimp.hpp"
#include "apps/openmw/mwworld/esmstore.hpp"
#include "apps/openmw/mwworld/worldimp.hpp"

#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>
#include <components/esm3/formatversion.hpp>
#include <components/esm3/journalentry.hpp>
#include <components/esm3/loaddial.hpp>
#include <components/esm3/loadinfo.hpp>
#include <components/esm3/queststate.hpp>

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace MWDialogue::Testing
{
    /// One stage of a quest in the game's content: its journal index, its text, and whether reaching it finishes
    /// the quest.
    struct QuestStage
    {
        int mIndex;
        std::string mText;
        bool mFinishes = false;
    };

    /// An entry the player has in their journal, on a day of the game.
    struct HeardEntry
    {
        std::string mQuest;
        int mStage;
        int mDay = 1;
    };

    /// A journal, and the game content it needs. Tests add quests with addQuest, load them with loadContent, and fill
    /// the journal with addEntries, which goes through Journal::readRecord as loading a save does. No window manager
    /// is needed.
    class JournalFixture : public ::testing::Test
    {
    protected:
        MWBase::Environment mEnvironment;
        MWWorld::World mWorld{ nullptr, -1, {}, {} };
        MWWorld::ESMStore& mStore = mWorld.getStore();
        // Quests tell Lua scripts when their stage changes.
        MWLua::LuaManager mLuaManager{ nullptr, {} };
        Journal mJournal;
        std::vector<ESM::Dialogue> mContent;

        void SetUp() override
        {
            mEnvironment.setWorld(mWorld);
            mEnvironment.setESMStore(mStore);
            mEnvironment.setJournal(mJournal);
            mEnvironment.setLuaManager(mLuaManager);
        }

        static ESM::RefId id(std::string_view name) { return ESM::RefId::stringRefId(name); }

        static ESM::RefId stageInfo(std::string_view quest, int stage)
        {
            return id(std::string(quest) + "_" + std::to_string(stage));
        }

        /// Adds a journal topic to the content. An empty name gives a topic with no name, which the journal shows as
        /// a note when no stage finishes it.
        ESM::Dialogue& addQuest(std::string_view quest, std::string_view name, const std::vector<QuestStage>& stages)
        {
            ESM::Dialogue& dialogue = mContent.emplace_back();
            dialogue.blank();
            dialogue.mId = id(quest);
            dialogue.mStringId = quest;
            dialogue.mType = ESM::Dialogue::Journal;
            if (!name.empty())
            {
                ESM::DialInfo& info = dialogue.mInfo.emplace_back();
                info.blank();
                info.mId = id(std::string(quest) + "_name");
                info.mResponse = name;
                info.mQuestStatus = ESM::DialInfo::QS_Name;
            }
            for (const QuestStage& stage : stages)
            {
                ESM::DialInfo& info = dialogue.mInfo.emplace_back();
                info.blank();
                info.mId = stageInfo(quest, stage.mIndex);
                info.mData.mJournalIndex = stage.mIndex;
                info.mResponse = stage.mText;
                info.mQuestStatus = stage.mFinishes ? ESM::DialInfo::QS_Finished : ESM::DialInfo::QS_None;
            }
            return dialogue;
        }

        /// Loads the content added so far into the store.
        void loadContent()
        {
            auto stream = std::make_unique<std::stringstream>();
            ESM::ESMWriter writer;
            writer.setFormatVersion(ESM::CurrentSaveGameFormatVersion);
            writer.save(*stream);
            for (const ESM::Dialogue& dialogue : mContent)
            {
                writer.startRecord(ESM::REC_DIAL);
                dialogue.save(writer);
                writer.endRecord(ESM::REC_DIAL);
                for (const ESM::DialInfo& info : dialogue.mInfo)
                {
                    writer.startRecord(ESM::REC_INFO);
                    info.save(writer);
                    writer.endRecord(ESM::REC_INFO);
                }
            }
            ESM::ESMReader reader;
            reader.open(std::move(stream), "journal-test-content");
            ESM::Dialogue* dialogue = nullptr;
            mStore.load(reader, nullptr, dialogue);
            mStore.getWritable<ESM::Dialogue>().setUp();
        }

        /// Fills the journal as loading a save with these entries does. As Journal::write does, the save has each
        /// quest's state, finished when one of its entries finishes it, followed by the quest's entries, and then the
        /// journal's dated list of entries.
        void addEntries(const std::vector<HeardEntry>& entries)
        {
            auto stream = std::make_unique<std::stringstream>();
            ESM::ESMWriter writer;
            writer.setFormatVersion(ESM::CurrentSaveGameFormatVersion);
            writer.save(*stream);

            const auto writeEntry = [&](const HeardEntry& heard, ESM::JournalEntry::Type type) {
                ESM::JournalEntry record;
                record.mType = type;
                record.mTopic = id(heard.mQuest);
                record.mInfo = stageInfo(heard.mQuest, heard.mStage);
                record.mText = stageText(heard.mQuest, heard.mStage);
                record.mDay = heard.mDay;
                record.mMonth = 0;
                record.mDayOfMonth = heard.mDay;
                writer.startRecord(ESM::REC_JOUR);
                record.save(writer);
                writer.endRecord(ESM::REC_JOUR);
            };

            std::map<std::string, ESM::QuestState> states;
            for (const HeardEntry& heard : entries)
            {
                ESM::QuestState& state = states[heard.mQuest];
                state.mTopic = id(heard.mQuest);
                state.mState = std::max(state.mState, heard.mStage);
                if (stageFinishes(heard.mQuest, heard.mStage))
                    state.mFinished = 1;
            }
            for (const auto& [quest, state] : states)
            {
                writer.startRecord(ESM::REC_QUES);
                state.save(writer);
                writer.endRecord(ESM::REC_QUES);
                for (const HeardEntry& heard : entries)
                    if (heard.mQuest == quest)
                        writeEntry(heard, ESM::JournalEntry::Type_Quest);
            }
            for (const HeardEntry& heard : entries)
                writeEntry(heard, ESM::JournalEntry::Type_Journal);

            ESM::ESMReader reader;
            reader.open(std::move(stream), "journal-test-save");
            while (reader.hasMoreRecs())
            {
                const ESM::NAME name = reader.getRecName();
                reader.getRecHeader();
                mJournal.readRecord(reader, name.toInt());
            }
        }

        Quest& quest(std::string_view name) { return *mJournal.getQuestOrNull(id(name)); }

    private:
        const ESM::DialInfo* findStage(std::string_view quest, int stage) const
        {
            for (const ESM::Dialogue& dialogue : mContent)
                if (dialogue.mId == id(quest))
                    for (const ESM::DialInfo& info : dialogue.mInfo)
                        if (info.mId == stageInfo(quest, stage))
                            return &info;
            return nullptr;
        }

        std::string stageText(std::string_view quest, int stage) const
        {
            const ESM::DialInfo* info = findStage(quest, stage);
            return info != nullptr ? info->mResponse : std::string();
        }

        bool stageFinishes(std::string_view quest, int stage) const
        {
            const ESM::DialInfo* info = findStage(quest, stage);
            return info != nullptr && info->mQuestStatus == ESM::DialInfo::QS_Finished;
        }
    };
}

#endif
