#include "apps/openmw/mwdialogue/keywordsearch.hpp"
#include "apps/openmw/mwdialogue/topic.hpp"
#include "apps/openmw/mwgui/journallinks.hpp"

#include <components/translation/translation.hpp>

#include <gtest/gtest.h>

#include <string>
#include <unordered_map>

namespace MWGui
{
    namespace
    {
        class MWGuiJournalLinksTest : public ::testing::Test
        {
        protected:
            MWDialogue::KeywordSearch mSearch;
            Translation::Storage mTranslations;
            std::unordered_map<std::string, const MWDialogue::Topic*> mTopics;
            MWDialogue::Topic mBalmora;
            MWDialogue::Topic mMagesGuild;
            MWDialogue::Topic mGuild;

            MWGuiJournalLinksTest()
            {
                // The topics the player has heard of, each with a page.
                mSearch.seed("balmora", "balmora");
                mSearch.seed("mages guild", "mages guild");
                mSearch.seed("guild", "guild");
                mTopics["balmora"] = &mBalmora;
                mTopics["mages guild"] = &mMagesGuild;
                mTopics["guild"] = &mGuild;
            }

            LinkedText link(const std::string& text, const MWDialogue::Topic* page = nullptr) const
            {
                return linkTopics(text, mSearch, mTranslations, mTopics, page);
            }

            static std::string spanText(const LinkedText& linked, std::size_t span)
            {
                const auto& [start, end, topic] = linked.mSpans[span];
                return linked.mText.substr(start, end - start);
            }
        };

        TEST_F(MWGuiJournalLinksTest, knownTopicShouldBecomeASpan)
        {
            const LinkedText linked = link("Go to Balmora tomorrow.");
            EXPECT_EQ(linked.mText, "Go to Balmora tomorrow.");
            ASSERT_EQ(linked.mSpans.size(), 1);
            EXPECT_EQ(spanText(linked, 0), "Balmora");
            EXPECT_EQ(std::get<2>(linked.mSpans[0]), &mBalmora);
        }

        TEST_F(MWGuiJournalLinksTest, textWithoutTopicsShouldBeUnchanged)
        {
            const LinkedText linked = link("Nothing to see here.");
            EXPECT_EQ(linked.mText, "Nothing to see here.");
            EXPECT_TRUE(linked.mSpans.empty());
        }

        TEST_F(MWGuiJournalLinksTest, topicWithoutAPageShouldNotLink)
        {
            mSearch.seed("vivec", "vivec");
            const LinkedText linked = link("Go to Vivec.");
            EXPECT_EQ(linked.mText, "Go to Vivec.");
            EXPECT_TRUE(linked.mSpans.empty());
        }

        TEST_F(MWGuiJournalLinksTest, topicShouldNotLinkToItsOwnPage)
        {
            const LinkedText linked = link("Balmora is a town.", &mBalmora);
            EXPECT_TRUE(linked.mSpans.empty());
        }

        TEST_F(MWGuiJournalLinksTest, twoTopicsShouldGiveTwoSpansInOrder)
        {
            const LinkedText linked = link("In Balmora, find the guild.");
            ASSERT_EQ(linked.mSpans.size(), 2);
            EXPECT_EQ(spanText(linked, 0), "Balmora");
            EXPECT_EQ(spanText(linked, 1), "guild");
        }

        TEST_F(MWGuiJournalLinksTest, longerTopicShouldWinOverAShorterOneInsideIt)
        {
            const LinkedText linked = link("Join the Mages Guild.");
            ASSERT_EQ(linked.mSpans.size(), 1);
            EXPECT_EQ(spanText(linked, 0), "Mages Guild");
            EXPECT_EQ(std::get<2>(linked.mSpans[0]), &mMagesGuild);
        }

        TEST_F(MWGuiJournalLinksTest, explicitTopicShouldShowItsDisplayName)
        {
            const LinkedText linked = link("Ask about @Balmora# soon.");
            EXPECT_EQ(linked.mText, "Ask about Balmora soon.");
            ASSERT_EQ(linked.mSpans.size(), 1);
            EXPECT_EQ(spanText(linked, 0), "Balmora");
        }
    }
}
