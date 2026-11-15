#include "journalfixture.hpp"

#include <components/esm3/uistate.hpp>

#include <gtest/gtest.h>

#include <algorithm>

namespace MWDialogue
{
    namespace
    {
        using namespace Testing;

        class MWDialogueJournalTest : public JournalFixture
        {
        protected:
            void SetUp() override
            {
                JournalFixture::SetUp();
                addQuest("fg_rathunt", "Fighters Guild: Rat Hunt",
                    { { 10, "Kill the rats." }, { 100, "The rats are dead.", true } });
                addQuest("ms_fargothring", "Fargoth's Ring", { { 10, "Find the ring." } });
                addQuest("ms_fargothring2", "fargoth's ring", { { 10, "Give back the ring." } });
                addQuest("a1_note", "", { { 10, "A note." } });
                addQuest("a1_note2", "", { { 10, "Another note." } });
                loadContent();
                addEntries({ { "fg_rathunt", 10, 1 }, { "ms_fargothring", 10, 2 }, { "ms_fargothring2", 10, 3 },
                    { "a1_note", 10, 4 }, { "a1_note2", 10, 5 } });
            }

            const StampedJournalEntry& entry(std::string_view quest, int stage) const
            {
                const auto& entries = mJournal.getEntries();
                const auto found = std::find_if(entries.begin(), entries.end(), [&](const StampedJournalEntry& e) {
                    return e.mTopic == id(quest) && e.mInfoId == stageInfo(quest, stage);
                });
                EXPECT_NE(found, entries.end());
                return *found;
            }
        };

        TEST_F(MWDialogueJournalTest, savedEntriesShouldLoadIntoTheJournal)
        {
            EXPECT_EQ(mJournal.getEntries().size(), 5);
            EXPECT_EQ(mJournal.getQuests().size(), 5);
            EXPECT_EQ(quest("fg_rathunt").getName(), "Fighters Guild: Rat Hunt");
            EXPECT_FALSE(quest("fg_rathunt").isFinished());
        }

        TEST_F(MWDialogueJournalTest, writeUiStateShouldListPinnedAndArchivedQuests)
        {
            quest("fg_rathunt").setPinned(true);
            quest("a1_note").setArchived(true);
            ESM::UiState state;
            mJournal.writeUiState(state);
            EXPECT_EQ(state.mPinnedQuests, std::vector<ESM::RefId>{ id("fg_rathunt") });
            EXPECT_EQ(state.mArchivedQuests, std::vector<ESM::RefId>{ id("a1_note") });
        }

        TEST_F(MWDialogueJournalTest, writeUiStateShouldLeaveOutEntriesWithNoActorOrCell)
        {
            ESM::UiState state;
            mJournal.writeUiState(state);
            EXPECT_TRUE(state.mJournalSources.empty());
        }

        TEST_F(MWDialogueJournalTest, readUiStateShouldGiveEntriesTheirActorAndCell)
        {
            ESM::UiState state;
            state.mJournalSources.push_back(
                { id("fg_rathunt"), stageInfo("fg_rathunt", 10), 1, id("eydis fire-eye"), id("balmora") });
            mJournal.readUiState(state);
            EXPECT_EQ(entry("fg_rathunt", 10).mActor, id("eydis fire-eye"));
            EXPECT_EQ(entry("fg_rathunt", 10).mCell, id("balmora"));
            EXPECT_TRUE(entry("ms_fargothring", 10).mActor.empty());
        }

        TEST_F(MWDialogueJournalTest, readUiStateShouldNotMatchAnEntryFromAnotherDay)
        {
            ESM::UiState state;
            state.mJournalSources.push_back(
                { id("fg_rathunt"), stageInfo("fg_rathunt", 10), 7, id("eydis fire-eye"), id("balmora") });
            mJournal.readUiState(state);
            EXPECT_TRUE(entry("fg_rathunt", 10).mActor.empty());
        }

        TEST_F(MWDialogueJournalTest, readUiStateShouldSkipQuestsTheJournalDoesNotHave)
        {
            ESM::UiState state;
            state.mPinnedQuests = { id("never_started"), id("fg_rathunt") };
            state.mArchivedQuests = { id("never_started") };
            mJournal.readUiState(state);
            EXPECT_EQ(mJournal.getQuestOrNull(id("never_started")), nullptr);
            EXPECT_TRUE(quest("fg_rathunt").isPinned());
        }

        TEST_F(MWDialogueJournalTest, uiStateShouldRoundTrip)
        {
            quest("fg_rathunt").setPinned(true);
            quest("ms_fargothring").setArchived(true);
            ESM::UiState state;
            state.mJournalSources.push_back(
                { id("fg_rathunt"), stageInfo("fg_rathunt", 10), 1, id("eydis fire-eye"), id("balmora") });
            mJournal.readUiState(state);
            ESM::UiState written;
            mJournal.writeUiState(written);

            quest("fg_rathunt").setPinned(false);
            quest("ms_fargothring").setArchived(false);
            mJournal.readUiState(written);
            EXPECT_TRUE(quest("fg_rathunt").isPinned());
            EXPECT_TRUE(quest("ms_fargothring").isArchived());
            ASSERT_EQ(written.mJournalSources.size(), 1);
            EXPECT_EQ(written.mJournalSources[0].mActor, id("eydis fire-eye"));
        }

        TEST_F(MWDialogueJournalTest, unarchiveShouldClearTheQuest)
        {
            quest("fg_rathunt").setArchived(true);
            mJournal.unarchiveQuest(id("fg_rathunt"));
            EXPECT_FALSE(quest("fg_rathunt").isArchived());
        }

        TEST_F(MWDialogueJournalTest, unarchiveShouldClearOtherQuestsWithTheSameName)
        {
            quest("ms_fargothring").setArchived(true);
            quest("ms_fargothring2").setArchived(true);
            mJournal.unarchiveQuest(id("ms_fargothring2"));
            EXPECT_FALSE(quest("ms_fargothring").isArchived());
            EXPECT_FALSE(quest("ms_fargothring2").isArchived());
        }

        TEST_F(MWDialogueJournalTest, unarchiveShouldLeaveOtherQuestsAlone)
        {
            quest("fg_rathunt").setArchived(true);
            quest("ms_fargothring").setArchived(true);
            mJournal.unarchiveQuest(id("ms_fargothring"));
            EXPECT_TRUE(quest("fg_rathunt").isArchived());
        }

        TEST_F(MWDialogueJournalTest, unarchiveShouldNotJoinQuestsWithoutNames)
        {
            quest("a1_note").setArchived(true);
            quest("a1_note2").setArchived(true);
            mJournal.unarchiveQuest(id("a1_note2"));
            EXPECT_TRUE(quest("a1_note").isArchived());
            EXPECT_FALSE(quest("a1_note2").isArchived());
        }
    }
}
