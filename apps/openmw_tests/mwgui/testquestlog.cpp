#include "../mwdialogue/journalfixture.hpp"

#include "apps/openmw/mwgui/questlog.hpp"

#include <components/esm3/loadfact.hpp>
#include <components/esm3/uistate.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <utility>
#include <vector>

namespace MWGui
{
    namespace
    {
        using namespace MWDialogue::Testing;

        using Commands = std::vector<std::pair<ESM::RefId, int>>;

        ESM::RefId id(std::string_view name)
        {
            return ESM::RefId::stringRefId(name);
        }

        TEST(MWGuiQuestLogTextTest, journalCommandShouldGiveTheQuestAndStage)
        {
            EXPECT_EQ(QuestLog::journalCommands("Journal FG_RatHunt 10"), (Commands{ { id("fg_rathunt"), 10 } }));
        }

        TEST(MWGuiQuestLogTextTest, journalCommandShouldAllowQuotesAndCommas)
        {
            EXPECT_EQ(QuestLog::journalCommands("Journal \"FG_RatHunt\", 10"), (Commands{ { id("fg_rathunt"), 10 } }));
        }

        TEST(MWGuiQuestLogTextTest, journalCommandOnAnotherObjectShouldCount)
        {
            EXPECT_EQ(QuestLog::journalCommands("\"arrille\"->Journal ms_fargothring 10"),
                (Commands{ { id("ms_fargothring"), 10 } }));
        }

        TEST(MWGuiQuestLogTextTest, journalCommandInACommentShouldBeIgnored)
        {
            EXPECT_TRUE(QuestLog::journalCommands("; Journal FG_RatHunt 10").empty());
            EXPECT_EQ(QuestLog::journalCommands("Journal FG_RatHunt 10 ; Journal FG_RatHunt 20"),
                (Commands{ { id("fg_rathunt"), 10 } }));
        }

        TEST(MWGuiQuestLogTextTest, journalCommandWithoutANumberShouldBeIgnored)
        {
            EXPECT_TRUE(QuestLog::journalCommands("Journal FG_RatHunt stage").empty());
            EXPECT_TRUE(QuestLog::journalCommands("Journal FG_RatHunt").empty());
        }

        TEST(MWGuiQuestLogTextTest, scriptWithSeveralCommandsShouldGiveThemAll)
        {
            EXPECT_EQ(QuestLog::journalCommands("Journal FG_RatHunt 10\nAddTopic \"rats\"\nJournal MS_FargothRing 20"),
                (Commands{ { id("fg_rathunt"), 10 }, { id("ms_fargothring"), 20 } }));
        }

        TEST(MWGuiQuestLogTextTest, townShouldBeTheTextBeforeACommaOrColon)
        {
            EXPECT_EQ(QuestLog::townOf("Balmora, Guild of Fighters"), "Balmora");
            EXPECT_EQ(QuestLog::townOf("Mournhold Temple: Reception Area"), "Mournhold Temple");
            EXPECT_EQ(QuestLog::townOf("Seyda Neen"), "Seyda Neen");
        }

        TEST(MWGuiQuestLogTextTest, untitledNameShouldStopAtTheFirstSentence)
        {
            EXPECT_EQ(QuestLog::untitledName("Rules of the Fighters Guild. Do not steal."), "Rules of the Fighters Guild");
            EXPECT_EQ(QuestLog::untitledName("A gift! Enjoy it."), "A gift");
        }

        TEST(MWGuiQuestLogTextTest, untitledNameShouldBreakLongTextAtAWord)
        {
            EXPECT_EQ(QuestLog::untitledName("Caius Cosades gave me a package for the Blades contact in the city"),
                "Caius Cosades gave me a package for the Blades...");
        }

        class MWGuiQuestLogTest : public JournalFixture
        {
        protected:
            void SetUp() override
            {
                JournalFixture::SetUp();
                QuestLog::resetContentCache();
            }

            void TearDown() override { QuestLog::resetContentCache(); }

            static const QuestLog::Quest* find(const std::vector<QuestLog::Quest>& quests, std::string_view name)
            {
                const auto it = std::find_if(
                    quests.begin(), quests.end(), [&](const QuestLog::Quest& quest) { return quest.mName == name; });
                return it != quests.end() ? &*it : nullptr;
            }

            void addFaction(std::string_view factionId, std::string_view name)
            {
                ESM::Faction faction;
                faction.blank();
                faction.mId = id(factionId);
                faction.mName = name;
                mStore.insertStatic(faction);
            }
        };

        TEST_F(MWGuiQuestLogTest, questsWithTheSameNameShouldBeOneQuest)
        {
            addQuest("ms_fargothring", "Fargoth's Ring", { { 10, "Find the ring." } });
            addQuest("ms_fargothring2", "fargoth's ring", { { 10, "Give back the ring." } });
            loadContent();
            addEntries({ { "ms_fargothring", 10, 1 }, { "ms_fargothring2", 10, 2 } });
            const std::vector<QuestLog::Quest> quests = QuestLog::collect();
            ASSERT_EQ(quests.size(), 1);
            EXPECT_EQ(quests[0].mIds.size(), 2);
            ASSERT_EQ(quests[0].mEntries.size(), 2);
            EXPECT_EQ(quests[0].mEntries[0]->getText(), "Find the ring.");
            EXPECT_EQ(quests[0].mEntries[1]->getText(), "Give back the ring.");
        }

        TEST_F(MWGuiQuestLogTest, questsWithoutANameShouldStayApart)
        {
            addQuest("a1_note", "", { { 10, "A note." } });
            addQuest("a1_note2", "", { { 10, "Another note." } });
            loadContent();
            addEntries({ { "a1_note", 10, 1 }, { "a1_note2", 10, 2 } });
            const std::vector<QuestLog::Quest> quests = QuestLog::collect();
            ASSERT_EQ(quests.size(), 2);
            EXPECT_NE(find(quests, "A note"), nullptr);
            EXPECT_NE(find(quests, "Another note"), nullptr);
        }

        TEST_F(MWGuiQuestLogTest, questShouldBeFinishedPinnedOrArchivedWhenAnyIdIs)
        {
            addQuest("ms_fargothring", "Fargoth's Ring", { { 10, "Find the ring." } });
            addQuest("ms_fargothring2", "Fargoth's Ring", { { 10, "Give back the ring.", true } });
            loadContent();
            addEntries({ { "ms_fargothring", 10, 1 }, { "ms_fargothring2", 10, 2 } });
            quest("ms_fargothring").setPinned(true);
            quest("ms_fargothring2").setArchived(true);
            const std::vector<QuestLog::Quest> quests = QuestLog::collect();
            ASSERT_EQ(quests.size(), 1);
            EXPECT_TRUE(quests[0].mFinished);
            EXPECT_TRUE(quests[0].mPinned);
            EXPECT_TRUE(quests[0].mArchived);
        }

        TEST_F(MWGuiQuestLogTest, unnamedIdWithNoFinishingEntryShouldBeANote)
        {
            addQuest("a1_note", "", { { 10, "Rules of the guild." } });
            loadContent();
            addEntries({ { "a1_note", 10, 1 } });
            const std::vector<QuestLog::Quest> quests = QuestLog::collect();
            ASSERT_EQ(quests.size(), 1);
            EXPECT_TRUE(quests[0].mNote);
            EXPECT_EQ(quests[0].mName, "Rules of the guild");
        }

        TEST_F(MWGuiQuestLogTest, unnamedIdWithAFinishingEntryShouldBeAQuest)
        {
            addQuest("a1_errand", "", { { 10, "Fetch the gift." }, { 100, "The gift is delivered.", true } });
            loadContent();
            addEntries({ { "a1_errand", 10, 1 } });
            const std::vector<QuestLog::Quest> quests = QuestLog::collect();
            ASSERT_EQ(quests.size(), 1);
            EXPECT_FALSE(quests[0].mNote);
        }

        TEST_F(MWGuiQuestLogTest, namedIdShouldNeverBeANote)
        {
            addQuest("fg_rathunt", "Rat Hunt", { { 10, "Kill the rats." } });
            loadContent();
            addEntries({ { "fg_rathunt", 10, 1 } });
            const std::vector<QuestLog::Quest> quests = QuestLog::collect();
            ASSERT_EQ(quests.size(), 1);
            EXPECT_FALSE(quests[0].mNote);
        }

        TEST_F(MWGuiQuestLogTest, questsShouldComeNewestActivityFirst)
        {
            addQuest("fg_rathunt", "Rat Hunt", { { 10, "Kill the rats." }, { 20, "Report back." } });
            addQuest("ms_fargothring", "Fargoth's Ring", { { 10, "Find the ring." } });
            loadContent();
            addEntries({ { "fg_rathunt", 10, 1 }, { "ms_fargothring", 10, 2 }, { "fg_rathunt", 20, 3 } });
            const std::vector<QuestLog::Quest> quests = QuestLog::collect();
            ASSERT_EQ(quests.size(), 2);
            EXPECT_EQ(quests[0].mName, "Rat Hunt");
            EXPECT_EQ(quests[1].mName, "Fargoth's Ring");
        }

        TEST_F(MWGuiQuestLogTest, giverShouldComeFromTheEntrysActor)
        {
            addQuest("fg_rathunt", "Rat Hunt", { { 10, "Kill the rats." } });
            loadContent();
            addEntries({ { "fg_rathunt", 10, 1 } });
            ESM::UiState state;
            state.mJournalSources.push_back({ id("fg_rathunt"), stageInfo("fg_rathunt", 10), 1, id("eydis"), {} });
            mJournal.readUiState(state);
            const std::vector<QuestLog::Quest> quests = QuestLog::collect();
            ASSERT_EQ(quests.size(), 1);
            EXPECT_EQ(quests[0].mGiver, id("eydis"));
        }

        TEST_F(MWGuiQuestLogTest, giverShouldFallBackToTheConversationWithTheEarliestStage)
        {
            addQuest("fg_rathunt", "Rat Hunt", { { 10, "Kill the rats." }, { 20, "Report back." } });
            ESM::Dialogue& topic = mContent.emplace_back();
            topic.blank();
            topic.mId = id("rats");
            topic.mStringId = "rats";
            topic.mType = ESM::Dialogue::Topic;
            for (const auto& [infoId, actor, stage] : { std::tuple{ "late", "ajira", 20 }, { "early", "eydis", 10 } })
            {
                ESM::DialInfo& info = topic.mInfo.emplace_back();
                info.blank();
                info.mId = id(infoId);
                info.mActor = id(actor);
                info.mResultScript = "Journal FG_RatHunt " + std::to_string(stage);
            }
            loadContent();
            addEntries({ { "fg_rathunt", 10, 1 } });
            const std::vector<QuestLog::Quest> quests = QuestLog::collect();
            ASSERT_EQ(quests.size(), 1);
            EXPECT_EQ(quests[0].mGiver, id("eydis"));
        }

        TEST_F(MWGuiQuestLogTest, factionShouldComeFromTheQuestsName)
        {
            addFaction("fighters guild", "Fighters Guild");
            addFaction("hlaalu", "Great House Hlaalu");
            addQuest("fg_rathunt", "Fighters Guild: Rat Hunt", { { 10, "Kill the rats." } });
            addQuest("hh_twinlamps", "House Hlaalu: The Twin Lamps", { { 10, "Find the Twin Lamps." } });
            addQuest("ms_fargothring", "Fargoth's Ring", { { 10, "Find the ring." } });
            loadContent();
            addEntries({ { "fg_rathunt", 10, 1 }, { "hh_twinlamps", 10, 2 }, { "ms_fargothring", 10, 3 } });
            const std::vector<QuestLog::Quest> quests = QuestLog::collect();
            ASSERT_NE(find(quests, "Fighters Guild: Rat Hunt"), nullptr);
            EXPECT_EQ(find(quests, "Fighters Guild: Rat Hunt")->mFaction, id("fighters guild"));
            EXPECT_EQ(find(quests, "House Hlaalu: The Twin Lamps")->mFaction, id("hlaalu"));
            EXPECT_TRUE(find(quests, "Fargoth's Ring")->mFaction.empty());
        }

        TEST_F(MWGuiQuestLogTest, pinningShouldUnarchiveEveryId)
        {
            addQuest("ms_fargothring", "Fargoth's Ring", { { 10, "Find the ring." } });
            addQuest("ms_fargothring2", "Fargoth's Ring", { { 10, "Give back the ring." } });
            loadContent();
            addEntries({ { "ms_fargothring", 10, 1 }, { "ms_fargothring2", 10, 2 } });
            quest("ms_fargothring").setArchived(true);
            quest("ms_fargothring2").setArchived(true);
            std::vector<QuestLog::Quest> quests = QuestLog::collect();
            QuestLog::setPinned(quests[0], true);
            EXPECT_TRUE(quests[0].mPinned);
            EXPECT_FALSE(quests[0].mArchived);
            for (std::string_view name : { "ms_fargothring", "ms_fargothring2" })
            {
                EXPECT_TRUE(quest(name).isPinned());
                EXPECT_FALSE(quest(name).isArchived());
            }
        }

        TEST_F(MWGuiQuestLogTest, archivingShouldUnpinEveryId)
        {
            addQuest("ms_fargothring", "Fargoth's Ring", { { 10, "Find the ring." } });
            addQuest("ms_fargothring2", "Fargoth's Ring", { { 10, "Give back the ring." } });
            loadContent();
            addEntries({ { "ms_fargothring", 10, 1 }, { "ms_fargothring2", 10, 2 } });
            quest("ms_fargothring").setPinned(true);
            std::vector<QuestLog::Quest> quests = QuestLog::collect();
            QuestLog::setArchived(quests[0], true);
            EXPECT_FALSE(quests[0].mPinned);
            EXPECT_TRUE(quests[0].mArchived);
            for (std::string_view name : { "ms_fargothring", "ms_fargothring2" })
            {
                EXPECT_FALSE(quest(name).isPinned());
                EXPECT_TRUE(quest(name).isArchived());
            }
        }

        TEST_F(MWGuiQuestLogTest, unpinningShouldNotArchive)
        {
            addQuest("fg_rathunt", "Rat Hunt", { { 10, "Kill the rats." } });
            loadContent();
            addEntries({ { "fg_rathunt", 10, 1 } });
            quest("fg_rathunt").setPinned(true);
            std::vector<QuestLog::Quest> quests = QuestLog::collect();
            QuestLog::setPinned(quests[0], false);
            EXPECT_FALSE(quest("fg_rathunt").isPinned());
            EXPECT_FALSE(quest("fg_rathunt").isArchived());
        }

        class MWGuiQuestLogOrderTest : public MWGuiQuestLogTest
        {
        protected:
            using Names = std::vector<std::string>;

            static QuestLog::Quest quest(std::string_view name)
            {
                QuestLog::Quest result;
                result.mName = name;
                return result;
            }

            static Names names(const std::vector<QuestLog::Quest>& quests, const std::vector<int>& order)
            {
                Names result;
                for (int index : order)
                    result.push_back(quests[index].mName);
                return result;
            }

            static Names headers(const std::vector<QuestLog::Row>& rows)
            {
                Names result;
                for (const QuestLog::Row& row : rows)
                    if (row.mEntry == -1)
                        result.push_back(row.mHeader);
                return result;
            }
        };

        TEST_F(MWGuiQuestLogOrderTest, pinnedShouldComeFirstThenInProgressThenFinishedThenArchived)
        {
            std::vector<QuestLog::Quest> quests{ quest("archived"), quest("finished"), quest("open"), quest("pinned") };
            quests[0].mArchived = true;
            quests[1].mFinished = true;
            quests[3].mPinned = true;
            EXPECT_EQ(names(quests, QuestLog::order(quests, QuestLog::Tab::Recent)),
                (Names{ "pinned", "open", "finished", "archived" }));
        }

        TEST_F(MWGuiQuestLogOrderTest, archivedAndFinishedShouldCountAsArchived)
        {
            std::vector<QuestLog::Quest> quests{ quest("both"), quest("finished") };
            quests[0].mArchived = true;
            quests[0].mFinished = true;
            quests[1].mFinished = true;
            EXPECT_EQ(names(quests, QuestLog::order(quests, QuestLog::Tab::Recent)), (Names{ "finished", "both" }));
            EXPECT_EQ(QuestLog::groupOf(quests[0], QuestLog::Tab::Recent), "#{Interface:JournalArchived}");
        }

        TEST_F(MWGuiQuestLogOrderTest, orderWithinAGroupShouldStayNewestFirst)
        {
            const std::vector<QuestLog::Quest> quests{ quest("newest"), quest("middle"), quest("oldest") };
            EXPECT_EQ(names(quests, QuestLog::order(quests, QuestLog::Tab::Recent)),
                (Names{ "newest", "middle", "oldest" }));
        }

        TEST_F(MWGuiQuestLogOrderTest, placesTabShouldGroupQuestsInProgressByTown)
        {
            std::vector<QuestLog::Quest> quests{ quest("in vivec"), quest("in balmora"), quest("in caldera") };
            quests[0].mTown = "Vivec";
            quests[1].mTown = "balmora";
            quests[2].mTown = "Caldera";
            const std::vector<int> order = QuestLog::order(quests, QuestLog::Tab::Places);
            EXPECT_EQ(names(quests, order), (Names{ "in balmora", "in caldera", "in vivec" }));
            EXPECT_EQ(headers(QuestLog::buildRows(quests, order, QuestLog::Tab::Places)),
                (Names{ "balmora", "Caldera", "Vivec" }));
        }

        TEST_F(MWGuiQuestLogOrderTest, placesTabShouldPutQuestsWithoutATownLastBeforeFinished)
        {
            std::vector<QuestLog::Quest> quests{ quest("finished"), quest("nowhere"), quest("in balmora") };
            quests[0].mFinished = true;
            quests[2].mTown = "Balmora";
            const std::vector<int> order = QuestLog::order(quests, QuestLog::Tab::Places);
            EXPECT_EQ(names(quests, order), (Names{ "in balmora", "nowhere", "finished" }));
            EXPECT_EQ(headers(QuestLog::buildRows(quests, order, QuestLog::Tab::Places)),
                (Names{ "Balmora", "#{Interface:JournalUnknownPlace}", "#{Interface:JournalFinished}" }));
        }

        TEST_F(MWGuiQuestLogOrderTest, factionsTabShouldGroupByFactionNameWithoutAFactionLast)
        {
            addFaction("mages", "Mages Guild");
            addFaction("fighters", "Fighters Guild");
            std::vector<QuestLog::Quest> quests{ quest("none"), quest("mages"), quest("fighters") };
            quests[1].mFaction = id("mages");
            quests[2].mFaction = id("fighters");
            const std::vector<int> order = QuestLog::order(quests, QuestLog::Tab::Factions);
            EXPECT_EQ(names(quests, order), (Names{ "fighters", "mages", "none" }));
            EXPECT_EQ(headers(QuestLog::buildRows(quests, order, QuestLog::Tab::Factions)),
                (Names{ "Fighters Guild", "Mages Guild", "#{Interface:JournalNoFaction}" }));
        }

        TEST_F(MWGuiQuestLogOrderTest, questTabsShouldLeaveOutNotesButKeepPinnedOnes)
        {
            std::vector<QuestLog::Quest> quests{ quest("note"), quest("quest"), quest("pinned note") };
            quests[0].mNote = true;
            quests[2].mNote = true;
            quests[2].mPinned = true;
            for (QuestLog::Tab tab : { QuestLog::Tab::Recent, QuestLog::Tab::Places, QuestLog::Tab::Factions })
                EXPECT_EQ(names(quests, QuestLog::order(quests, tab)), (Names{ "pinned note", "quest" }));
        }

        TEST_F(MWGuiQuestLogOrderTest, notesTabShouldListOnlyNotes)
        {
            std::vector<QuestLog::Quest> quests{ quest("note"), quest("quest"), quest("archived note") };
            quests[0].mNote = true;
            quests[2].mNote = true;
            quests[2].mArchived = true;
            const std::vector<int> order = QuestLog::order(quests, QuestLog::Tab::Notes);
            EXPECT_EQ(names(quests, order), (Names{ "note", "archived note" }));
            EXPECT_EQ(headers(QuestLog::buildRows(quests, order, QuestLog::Tab::Notes)),
                (Names{ "#{Interface:JournalTabNotes}", "#{Interface:JournalArchived}" }));
        }

        TEST_F(MWGuiQuestLogOrderTest, rowsShouldStartEachGroupWithAHeader)
        {
            std::vector<QuestLog::Quest> quests{ quest("a"), quest("b"), quest("c") };
            quests[2].mFinished = true;
            const std::vector<int> order = QuestLog::order(quests, QuestLog::Tab::Recent);
            const std::vector<QuestLog::Row> rows = QuestLog::buildRows(quests, order, QuestLog::Tab::Recent);
            ASSERT_EQ(rows.size(), 5);
            EXPECT_EQ(rows[0].mHeader, "#{Interface:JournalInProgress}");
            EXPECT_EQ(rows[1].mEntry, 0);
            EXPECT_EQ(rows[2].mEntry, 1);
            EXPECT_EQ(rows[3].mHeader, "#{Interface:JournalFinished}");
            EXPECT_EQ(rows[4].mEntry, 2);
            EXPECT_EQ(QuestLog::groupStarts(rows), (std::vector<int>{ 0, 2 }));
        }

        TEST_F(MWGuiQuestLogOrderTest, townNameShouldBeEscapedInItsHeader)
        {
            std::vector<QuestLog::Quest> quests{ quest("odd") };
            quests[0].mTown = "#{Interface:OK}";
            EXPECT_NE(QuestLog::groupOf(quests[0], QuestLog::Tab::Places), "#{Interface:OK}");
        }

        TEST_F(MWGuiQuestLogOrderTest, emptyJournalShouldGiveNoRows)
        {
            const std::vector<QuestLog::Quest> quests;
            const std::vector<int> order = QuestLog::order(quests, QuestLog::Tab::Recent);
            EXPECT_TRUE(order.empty());
            EXPECT_TRUE(QuestLog::buildRows(quests, order, QuestLog::Tab::Recent).empty());
            EXPECT_TRUE(QuestLog::groupStarts({}).empty());
        }
    }
}
