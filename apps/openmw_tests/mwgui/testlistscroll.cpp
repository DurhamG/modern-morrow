#include "apps/openmw/mwgui/listscroll.hpp"

#include <gtest/gtest.h>

namespace MWGui::ListScroll
{
    namespace
    {
        Row item(int height = 10)
        {
            return Row{ height, true };
        }

        Row header(int height = 10)
        {
            return Row{ height, false };
        }

        // A header, three items, a header and three more items, each 10 high.
        const std::vector<Row> sGroups{ header(), item(), item(), item(), header(), item(), item(), item() };

        TEST(MWGuiListScrollTest, headersShouldNotCountAsItems)
        {
            EXPECT_EQ(itemCount(sGroups), 6);
            EXPECT_EQ(itemCount({ header() }), 0);
            EXPECT_EQ(itemCount({}), 0);
        }

        TEST(MWGuiListScrollTest, rowOfItemShouldSkipHeaders)
        {
            EXPECT_EQ(rowOfItem(sGroups, 0), 1);
            EXPECT_EQ(rowOfItem(sGroups, 3), 5);
            EXPECT_EQ(rowOfItem(sGroups, 5), 7);
        }

        TEST(MWGuiListScrollTest, itemsBeforeShouldCountOnlyItems)
        {
            EXPECT_EQ(itemsBefore(sGroups, 0), 0);
            EXPECT_EQ(itemsBefore(sGroups, 5), 3);
            EXPECT_EQ(itemsBefore(sGroups, 8), 6);
        }

        TEST(MWGuiListScrollTest, focusInViewShouldNotScroll)
        {
            EXPECT_EQ(scrollToShow(sGroups, 0, 3, 40), 0);
            EXPECT_EQ(scrollToShow(sGroups, 2, 4, 40), 2);
        }

        TEST(MWGuiListScrollTest, focusBelowTheViewShouldScrollJustFarEnough)
        {
            // Rows 0 to 3 fit in 40. Showing row 6 needs rows 3 to 6.
            EXPECT_EQ(scrollToShow(sGroups, 0, 6, 40), 3);
        }

        TEST(MWGuiListScrollTest, focusAboveTheViewShouldScrollToIt)
        {
            EXPECT_EQ(scrollToShow(sGroups, 5, 3, 40), 3);
        }

        TEST(MWGuiListScrollTest, scrollingUpToAGroupsFirstItemShouldShowItsHeader)
        {
            EXPECT_EQ(scrollToShow(sGroups, 6, 5, 40), 4);
            EXPECT_EQ(scrollToShow(sGroups, 3, 1, 40), 0);
        }

        TEST(MWGuiListScrollTest, rowsOfDifferentHeightsShouldBeMeasured)
        {
            const std::vector<Row> rows{ item(30), item(30), item(10), item(10) };
            // Rows 2 and 3 take 20, and row 1 doesn't fit above them in 45.
            EXPECT_EQ(scrollToShow(rows, 0, 3, 45), 2);
            EXPECT_EQ(scrollToShow(rows, 0, 3, 50), 1);
        }

        TEST(MWGuiListScrollTest, scrollOutsideTheListShouldBeClamped)
        {
            EXPECT_EQ(scrollToShow(sGroups, -3, 1, 40), 0);
            EXPECT_EQ(scrollToShow(sGroups, 20, 7, 40), 7);
        }

        TEST(MWGuiListScrollTest, listShouldNotScrollPastItsEnd)
        {
            // The last four rows fill 40, so scrolling further would leave space below the last row.
            EXPECT_EQ(maxScroll(sGroups, 40), 4);
            EXPECT_EQ(maxScroll(sGroups, 45), 4);
            EXPECT_EQ(maxScroll(sGroups, 50), 3);
        }

        TEST(MWGuiListScrollTest, listThatFitsShouldNotScroll)
        {
            EXPECT_EQ(maxScroll(sGroups, 80), 0);
            EXPECT_EQ(maxScroll(sGroups, 500), 0);
        }

        TEST(MWGuiListScrollTest, lastRowTallerThanTheViewShouldStillBeReachable)
        {
            EXPECT_EQ(maxScroll({ item(), item(), item(100) }, 40), 2);
        }

        TEST(MWGuiListScrollTest, wheelShouldFocusTheItemUnderThePointer)
        {
            // Scrolled to row 2: rows 2, 3, 4 and 5 fit, from 0 to 40.
            EXPECT_EQ(itemRowAt(sGroups, 2, 40, 5), 2);
            EXPECT_EQ(itemRowAt(sGroups, 2, 40, 15), 3);
        }

        TEST(MWGuiListScrollTest, pointerOnAHeaderShouldFocusTheItemBelowIt)
        {
            EXPECT_EQ(itemRowAt(sGroups, 2, 40, 25), 5);
        }

        TEST(MWGuiListScrollTest, pointerBelowTheLastRowShouldFocusTheLastItem)
        {
            EXPECT_EQ(itemRowAt(sGroups, 2, 40, 39), 5);
            EXPECT_EQ(itemRowAt(sGroups, 2, 40, 100), 5);
        }

        TEST(MWGuiListScrollTest, rowThatDoesNotFitShouldNotTakeTheFocus)
        {
            // Row 6 starts at 40, so it doesn't fit in 45.
            EXPECT_EQ(itemRowAt(sGroups, 2, 45, 44), 5);
        }

        TEST(MWGuiListScrollTest, viewWithNoItemsShouldFocusNothing)
        {
            EXPECT_EQ(itemRowAt({ header(), item(50) }, 0, 40, 5), -1);
        }
    }
}
