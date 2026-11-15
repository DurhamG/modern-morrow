#include "apps/openmw/mwstate/questlists.hpp"

#include <components/testing/util.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace MWState
{
    namespace
    {
        // A character's folder in a saves folder of the test's own, as the game makes them.
        std::filesystem::path characterDir(std::string_view name)
        {
            const std::filesystem::path saves = TestingOpenMW::currentTestDirPath();
            const std::filesystem::path dir = saves / name;
            std::filesystem::create_directories(dir);
            return dir;
        }

        std::vector<ESM::RefId> ids(std::initializer_list<std::string_view> names)
        {
            std::vector<ESM::RefId> result;
            for (std::string_view name : names)
                result.push_back(ESM::RefId::stringRefId(name));
            return result;
        }

        void writeLines(const std::filesystem::path& path, std::initializer_list<std::string_view> lines)
        {
            std::ofstream file(path, std::ios::trunc);
            for (std::string_view line : lines)
                file << line << '\n';
        }

        TEST(MWStateQuestListsTest, pathShouldSitBesideTheCharacterFolder)
        {
            const std::filesystem::path dir = characterDir("Lendias");
            EXPECT_EQ(questListPath(dir, sPinnedQuests), dir.parent_path() / "Lendias.pinned-quests.txt");
            EXPECT_EQ(questListPath(dir, sArchivedQuests), dir.parent_path() / "Lendias.archived-quests.txt");
        }

        TEST(MWStateQuestListsTest, pathShouldKeepDotsInTheFolderName)
        {
            const std::filesystem::path dir = characterDir("J. Doe");
            EXPECT_EQ(questListPath(dir, sPinnedQuests).filename(), "J. Doe.pinned-quests.txt");
        }

        TEST(MWStateQuestListsTest, shouldReadBackWhatWasWritten)
        {
            const std::filesystem::path dir = characterDir("Lendias");
            const std::vector<ESM::RefId> quests = ids({ "a1_1_findspymaster", "fg_rathunt", "ms_fargothring" });
            writeQuestList(dir, sPinnedQuests, quests);
            EXPECT_EQ(readQuestList(dir, sPinnedQuests), quests);
        }

        TEST(MWStateQuestListsTest, shouldKeepIdsWithSpacesAndUpperCase)
        {
            const std::filesystem::path dir = characterDir("Lendias");
            const std::vector<ESM::RefId> quests = ids({ "MS_Fargoth Ring", "Generated" });
            writeQuestList(dir, sPinnedQuests, quests);
            EXPECT_EQ(readQuestList(dir, sPinnedQuests), quests);
        }

        TEST(MWStateQuestListsTest, emptyListShouldRemoveTheFile)
        {
            const std::filesystem::path dir = characterDir("Lendias");
            writeQuestList(dir, sPinnedQuests, ids({ "fg_rathunt" }));
            ASSERT_TRUE(std::filesystem::exists(questListPath(dir, sPinnedQuests)));
            writeQuestList(dir, sPinnedQuests, {});
            EXPECT_FALSE(std::filesystem::exists(questListPath(dir, sPinnedQuests)));
        }

        TEST(MWStateQuestListsTest, emptyListShouldNotFailWhenThereIsNoFile)
        {
            const std::filesystem::path dir = characterDir("Lendias");
            EXPECT_NO_THROW(writeQuestList(dir, sPinnedQuests, {}));
            EXPECT_FALSE(std::filesystem::exists(questListPath(dir, sPinnedQuests)));
        }

        TEST(MWStateQuestListsTest, missingFileShouldReadAsEmpty)
        {
            EXPECT_TRUE(readQuestList(characterDir("Lendias"), sPinnedQuests).empty());
        }

        TEST(MWStateQuestListsTest, blankLinesShouldBeSkipped)
        {
            const std::filesystem::path dir = characterDir("Lendias");
            writeLines(questListPath(dir, sPinnedQuests), { "fg_rathunt", "", "ms_fargothring", "" });
            EXPECT_EQ(readQuestList(dir, sPinnedQuests), ids({ "fg_rathunt", "ms_fargothring" }));
        }

        TEST(MWStateQuestListsTest, damagedLineShouldKeepTheIdsBeforeIt)
        {
            const std::filesystem::path dir = characterDir("Lendias");
            writeLines(questListPath(dir, sPinnedQuests), { "fg_rathunt", "Generated:not a number", "ms_fargothring" });
            EXPECT_EQ(readQuestList(dir, sPinnedQuests), ids({ "fg_rathunt" }));
        }

        TEST(MWStateQuestListsTest, pinnedAndArchivedShouldBeSeparateFiles)
        {
            const std::filesystem::path dir = characterDir("Lendias");
            writeQuestList(dir, sPinnedQuests, ids({ "fg_rathunt" }));
            writeQuestList(dir, sArchivedQuests, ids({ "ms_fargothring" }));
            EXPECT_EQ(readQuestList(dir, sPinnedQuests), ids({ "fg_rathunt" }));
            EXPECT_EQ(readQuestList(dir, sArchivedQuests), ids({ "ms_fargothring" }));
        }

        TEST(MWStateQuestListsTest, filesShouldNotGoInsideTheCharacterFolder)
        {
            const std::filesystem::path dir = characterDir("Lendias");
            writeQuestList(dir, sPinnedQuests, ids({ "fg_rathunt" }));
            EXPECT_TRUE(std::filesystem::is_empty(dir));
        }
    }
}
