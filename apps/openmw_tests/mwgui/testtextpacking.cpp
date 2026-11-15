#include "apps/openmw/mwgui/textpacking.hpp"

#include <gtest/gtest.h>

namespace MWGui
{
    namespace
    {
        using Lines = std::vector<std::string>;
        using Rows = std::vector<std::vector<std::size_t>>;

        // Text measures 10 units a character.
        int measure(const std::string& text)
        {
            return static_cast<int>(text.size()) * 10;
        }

        const std::string sGap = "      ";

        TEST(MWGuiTextPackingTest, statusThatFitsShouldStayOnOneLine)
        {
            const std::string status = "6 done" + sGap + "2 left";
            EXPECT_EQ(packStatus(status, 1000, measure), (Lines{ status }));
        }

        TEST(MWGuiTextPackingTest, partsShouldMoveToANewLineWhenTheyRunOut)
        {
            const std::string status = "6 in progress" + sGap + "2 finished";
            EXPECT_EQ(packStatus(status, 150, measure), (Lines{ "6 in progress", "2 finished" }));
        }

        TEST(MWGuiTextPackingTest, threePartsShouldFillLinesGreedily)
        {
            const std::string status = "aaa" + sGap + "bbb" + sGap + "ccc";
            // "aaa      bbb" is 12 characters, and the three parts together are 21.
            EXPECT_EQ(packStatus(status, 120, measure), (Lines{ "aaa" + sGap + "bbb", "ccc" }));
            EXPECT_EQ(packStatus(status, 50, measure), (Lines{ "aaa", "bbb", "ccc" }));
        }

        TEST(MWGuiTextPackingTest, partWiderThanTheRoomShouldGetItsOwnLine)
        {
            const std::string status = "a very long part" + sGap + "b";
            EXPECT_EQ(packStatus(status, 50, measure), (Lines{ "a very long part", "b" }));
        }

        TEST(MWGuiTextPackingTest, statusWithoutGapsShouldStayOneLine)
        {
            EXPECT_EQ(packStatus("Magicka 50 / 50", 50, measure), (Lines{ "Magicka 50 / 50" }));
        }

        TEST(MWGuiTextPackingTest, singleSpacesShouldNotBreakALine)
        {
            EXPECT_EQ(packStatus("Armor 0 Gold 10", 50, measure), (Lines{ "Armor 0 Gold 10" }));
        }

        TEST(MWGuiTextPackingTest, emptyStatusShouldGiveOneEmptyLine)
        {
            EXPECT_EQ(packStatus("", 50, measure), (Lines{ "" }));
        }

        TEST(MWGuiTextPackingTest, thingsThatFitShouldShareARow)
        {
            EXPECT_EQ(packRows({ 30, 30, 30 }, 10, 110), (Rows{ { 0, 1, 2 } }));
        }

        TEST(MWGuiTextPackingTest, rowShouldBreakWhenTheNextThingDoesNotFit)
        {
            EXPECT_EQ(packRows({ 30, 30, 30 }, 10, 109), (Rows{ { 0, 1 }, { 2 } }));
        }

        TEST(MWGuiTextPackingTest, thingWiderThanTheWidthShouldGetARowOfItsOwn)
        {
            EXPECT_EQ(packRows({ 20, 200, 20 }, 10, 100), (Rows{ { 0 }, { 1 }, { 2 } }));
        }

        TEST(MWGuiTextPackingTest, nothingToPackShouldGiveOneEmptyRow)
        {
            EXPECT_EQ(packRows({}, 10, 100), (Rows{ {} }));
        }
    }
}
