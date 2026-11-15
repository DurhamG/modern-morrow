#include "apps/openmw/mwclass/npc.hpp"
#include "apps/openmw/mwworld/player.hpp"

#include <components/esm3/loadnpc.hpp>
#include <components/esm3/uistate.hpp>

#include <gtest/gtest.h>

#include <string>

namespace MWWorld
{
    namespace
    {
        ESM::RefId item(int number)
        {
            return ESM::RefId::stringRefId("item" + std::to_string(number));
        }

        // The player is an NPC, so the NPC class has to be known before one can be made.
        const ESM::NPC* blankNpc()
        {
            MWClass::Npc::registerSelf();
            static const ESM::NPC npc = [] {
                ESM::NPC result;
                result.blank();
                return result;
            }();
            return &npc;
        }

        class MWWorldPlayerRecentTest : public ::testing::Test
        {
        protected:
            MWWorldPlayerRecentTest()
                : mPlayer(blankNpc())
            {
            }

            Player mPlayer;
        };

        TEST_F(MWWorldPlayerRecentTest, newItemShouldGoFirstAndBeNew)
        {
            mPlayer.addRecentItem(item(1));
            mPlayer.addRecentItem(item(2));
            ASSERT_EQ(mPlayer.getRecentItems().size(), 2);
            EXPECT_EQ(mPlayer.getRecentItems()[0].mItem, item(2));
            EXPECT_EQ(mPlayer.getRecentItems()[1].mItem, item(1));
            EXPECT_TRUE(mPlayer.isRecentItemNew(item(2)));
        }

        TEST_F(MWWorldPlayerRecentTest, addingAnItemAgainShouldMoveItFirstAndMakeItNew)
        {
            mPlayer.addRecentItem(item(1));
            mPlayer.addRecentItem(item(2));
            mPlayer.markRecentItemSeen(item(1));
            mPlayer.addRecentItem(item(1));
            ASSERT_EQ(mPlayer.getRecentItems().size(), 2);
            EXPECT_EQ(mPlayer.getRecentItems()[0].mItem, item(1));
            EXPECT_TRUE(mPlayer.isRecentItemNew(item(1)));
        }

        TEST_F(MWWorldPlayerRecentTest, markingAnItemSeenShouldClearNew)
        {
            mPlayer.addRecentItem(item(1));
            mPlayer.markRecentItemSeen(item(1));
            EXPECT_FALSE(mPlayer.isRecentItemNew(item(1)));
            ASSERT_EQ(mPlayer.getRecentItems().size(), 1);
        }

        TEST_F(MWWorldPlayerRecentTest, unknownItemShouldNotBeNew)
        {
            mPlayer.addRecentItem(item(1));
            EXPECT_FALSE(mPlayer.isRecentItemNew(item(2)));
        }

        TEST_F(MWWorldPlayerRecentTest, seenItemsPastTheTabShouldBeDropped)
        {
            for (int i = 0; i <= static_cast<int>(Player::sRecentItemsShown); ++i)
            {
                mPlayer.addRecentItem(item(i));
                mPlayer.markRecentItemSeen(item(i));
            }
            mPlayer.addRecentItem(item(1000));
            EXPECT_EQ(mPlayer.getRecentItems().size(), Player::sRecentItemsShown);
            EXPECT_EQ(mPlayer.getRecentItems().back().mItem, item(2));
        }

        TEST_F(MWWorldPlayerRecentTest, unseenItemsPastTheTabShouldBeKept)
        {
            for (int i = 0; i < static_cast<int>(Player::sRecentItemsShown) + 10; ++i)
                mPlayer.addRecentItem(item(i));
            EXPECT_EQ(mPlayer.getRecentItems().size(), Player::sRecentItemsShown + 10);
            EXPECT_TRUE(mPlayer.isRecentItemNew(item(0)));
        }

        TEST_F(MWWorldPlayerRecentTest, atMost200ItemsShouldBeKept)
        {
            for (int i = 0; i < 250; ++i)
                mPlayer.addRecentItem(item(i));
            EXPECT_EQ(mPlayer.getRecentItems().size(), 200);
            EXPECT_EQ(mPlayer.getRecentItems().front().mItem, item(249));
            EXPECT_EQ(mPlayer.getRecentItems().back().mItem, item(50));
        }

        TEST_F(MWWorldPlayerRecentTest, recentSpellShouldGoFirstWithoutDuplicates)
        {
            mPlayer.addRecentSpell(item(1));
            mPlayer.addRecentSpell(item(2));
            mPlayer.addRecentSpell(item(1));
            ASSERT_EQ(mPlayer.getRecentSpells().size(), 2);
            EXPECT_EQ(mPlayer.getRecentSpells()[0], item(1));
            EXPECT_EQ(mPlayer.getRecentSpells()[1], item(2));
        }

        TEST_F(MWWorldPlayerRecentTest, atMost40SpellsShouldBeKept)
        {
            for (int i = 0; i < 50; ++i)
                mPlayer.addRecentSpell(item(i));
            ASSERT_EQ(mPlayer.getRecentSpells().size(), 40);
            EXPECT_EQ(mPlayer.getRecentSpells().front(), item(49));
            EXPECT_EQ(mPlayer.getRecentSpells().back(), item(10));
        }

        TEST_F(MWWorldPlayerRecentTest, uiStateShouldCarryBothLists)
        {
            mPlayer.addRecentItem(item(1));
            mPlayer.addRecentItem(item(2));
            mPlayer.markRecentItemSeen(item(1));
            mPlayer.addRecentSpell(item(3));
            ESM::UiState state;
            mPlayer.writeUiState(state);

            Player other(blankNpc());
            other.readUiState(state);
            ASSERT_EQ(other.getRecentItems().size(), 2);
            EXPECT_EQ(other.getRecentItems()[0].mItem, item(2));
            EXPECT_TRUE(other.isRecentItemNew(item(2)));
            EXPECT_FALSE(other.isRecentItemNew(item(1)));
            EXPECT_EQ(other.getRecentSpells(), std::vector<ESM::RefId>{ item(3) });
        }
    }
}
