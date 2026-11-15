#include "apps/openmw/mwstate/savefolders.hpp"

#include <components/esm3/formatversion.hpp>
#include <components/testing/util.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>

namespace MWState
{
    namespace
    {
        using namespace std::chrono_literals;

        std::filesystem::path testDir()
        {
            return TestingOpenMW::currentTestDirPath();
        }

        void writeFile(const std::filesystem::path& path, std::string_view text)
        {
            std::filesystem::create_directories(path.parent_path());
            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            file << text;
        }

        std::string readFile(const std::filesystem::path& path)
        {
            std::ifstream file(path, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        }

        // A save in a character's folder, last written at the given time.
        void writeSave(const std::filesystem::path& saves, std::string_view character, std::string_view name,
            std::filesystem::file_time_type time)
        {
            const std::filesystem::path path = saves / character / name;
            writeFile(path, "save");
            std::filesystem::last_write_time(path, time);
        }

        const std::filesystem::file_time_type sDay = std::filesystem::file_time_type::clock::now() - 24h;

        TEST(MWStateSaveFoldersTest, newestSaveShouldBeTheLatestFileInAnyCharacterFolder)
        {
            const std::filesystem::path saves = testDir() / "saves";
            writeSave(saves, "Lendias", "1.omwsave", sDay);
            writeSave(saves, "Lendias", "2.omwsave", sDay + 2h);
            writeSave(saves, "Vickor", "1.omwsave", sDay + 1h);
            EXPECT_EQ(newestSave(saves), sDay + 2h);
        }

        TEST(MWStateSaveFoldersTest, newestSaveShouldIgnoreFilesBesideTheCharacterFolders)
        {
            const std::filesystem::path saves = testDir() / "saves";
            writeSave(saves, "Lendias", "1.omwsave", sDay);
            writeFile(saves / "Lendias.pinned-quests.txt", "fg_rathunt\n");
            std::filesystem::last_write_time(saves / "Lendias.pinned-quests.txt", sDay + 5h);
            EXPECT_EQ(newestSave(saves), sDay);
        }

        TEST(MWStateSaveFoldersTest, newestSaveOfAMissingFolderShouldBeTheOldestTime)
        {
            EXPECT_EQ(newestSave(testDir() / "missing"), std::filesystem::file_time_type::min());
        }

        TEST(MWStateSaveFoldersTest, settingShouldWinOverEveryInstall)
        {
            const std::filesystem::path dir = testDir();
            writeSave(dir / "install", "Lendias", "1.omwsave", sDay);
            const std::vector<OpenMWInstall> installs{ { dir / "config", dir / "install" } };
            EXPECT_EQ(chooseOpenMWSaves("C:/Elsewhere/saves", installs, dir / "own"),
                std::filesystem::path("C:/Elsewhere/saves"));
        }

        TEST(MWStateSaveFoldersTest, settingShouldBeUsedEvenWhenTheFolderDoesNotExist)
        {
            const std::filesystem::path missing = testDir() / "missing";
            EXPECT_EQ(chooseOpenMWSaves(missing.string(), {}, testDir() / "own"), missing);
        }

        TEST(MWStateSaveFoldersTest, installWithoutConfigOrSavesFolderShouldBeSkipped)
        {
            const std::filesystem::path dir = testDir();
            const std::vector<OpenMWInstall> installs{ { dir / "no-config", dir / "no-saves" } };
            EXPECT_EQ(chooseOpenMWSaves("", installs, dir / "own"), std::filesystem::path());
        }

        TEST(MWStateSaveFoldersTest, installWithOnlyAConfigFolderShouldBeFound)
        {
            const std::filesystem::path dir = testDir();
            std::filesystem::create_directories(dir / "config");
            const std::vector<OpenMWInstall> installs{ { dir / "config", dir / "saves" } };
            EXPECT_EQ(chooseOpenMWSaves("", installs, dir / "own"), dir / "saves");
        }

        TEST(MWStateSaveFoldersTest, ownSavesFolderShouldNeverBeChosen)
        {
            const std::filesystem::path dir = testDir();
            writeSave(dir / "own", "Lendias", "1.omwsave", sDay);
            const std::vector<OpenMWInstall> installs{ { dir / "config", dir / "own" } };
            EXPECT_EQ(chooseOpenMWSaves("", installs, dir / "own"), std::filesystem::path());
        }

        TEST(MWStateSaveFoldersTest, installSavedToMostRecentlyShouldWin)
        {
            const std::filesystem::path dir = testDir();
            writeSave(dir / "flatpak", "Lendias", "1.omwsave", sDay);
            writeSave(dir / "usual", "Lendias", "1.omwsave", sDay + 1h);
            const std::vector<OpenMWInstall> installs{
                { dir / "flatpak-config", dir / "flatpak" },
                { dir / "usual-config", dir / "usual" },
            };
            EXPECT_EQ(chooseOpenMWSaves("", installs, dir / "own"), dir / "usual");
            writeSave(dir / "flatpak", "Lendias", "2.omwsave", sDay + 2h);
            EXPECT_EQ(chooseOpenMWSaves("", installs, dir / "own"), dir / "flatpak");
        }

        TEST(MWStateSaveFoldersTest, firstInstallShouldWinWhenNeitherHasSaves)
        {
            const std::filesystem::path dir = testDir();
            std::filesystem::create_directories(dir / "first-config");
            std::filesystem::create_directories(dir / "second-config");
            const std::vector<OpenMWInstall> installs{
                { dir / "first-config", dir / "first" },
                { dir / "second-config", dir / "second" },
            };
            EXPECT_EQ(chooseOpenMWSaves("", installs, dir / "own"), dir / "first");
        }

        TEST(MWStateSaveFoldersTest, noInstallShouldGiveAnEmptyPath)
        {
            EXPECT_EQ(chooseOpenMWSaves("", {}, testDir() / "own"), std::filesystem::path());
        }

        TEST(MWStateSaveFoldersTest, saveInFolderShouldMatchTwoLevelsUp)
        {
            const std::filesystem::path saves = testDir() / "saves";
            writeSave(saves, "Lendias", "1.omwsave", sDay);
            writeFile(saves / "1.omwsave", "save");
            EXPECT_TRUE(isSaveInFolder(saves / "Lendias" / "1.omwsave", saves));
            EXPECT_FALSE(isSaveInFolder(saves / "1.omwsave", saves));
            EXPECT_FALSE(isSaveInFolder(saves / "Lendias" / "1.omwsave", testDir() / "other"));
        }

        TEST(MWStateSaveFoldersTest, noFolderShouldHoldNoSaves)
        {
            const std::filesystem::path saves = testDir() / "saves";
            writeSave(saves, "Lendias", "1.omwsave", sDay);
            EXPECT_FALSE(isSaveInFolder(saves / "Lendias" / "1.omwsave", std::filesystem::path()));
        }

        TEST(MWStateSaveFoldersTest, backupShouldGoUnderTheCharactersName)
        {
            const std::filesystem::path dir = testDir();
            const std::filesystem::path save = dir / "openmw" / "Lendias" / "Quicksave.omwsave";
            writeFile(save, "an OpenMW save");
            const std::filesystem::path copy = backUpSave(save, dir / "backups", sDay);
            EXPECT_EQ(copy.parent_path(), dir / "backups" / "Lendias");
            EXPECT_TRUE(copy.filename().string().starts_with("Quicksave 2"));
            EXPECT_EQ(copy.extension(), ".omwsave");
            EXPECT_EQ(readFile(copy), "an OpenMW save");
            EXPECT_EQ(readFile(save), "an OpenMW save");
        }

        TEST(MWStateSaveFoldersTest, backupsAtDifferentTimesShouldBothBeKept)
        {
            const std::filesystem::path dir = testDir();
            const std::filesystem::path save = dir / "openmw" / "Lendias" / "1.omwsave";
            writeFile(save, "first");
            const std::filesystem::path first = backUpSave(save, dir / "backups", sDay);
            writeFile(save, "second");
            const std::filesystem::path second = backUpSave(save, dir / "backups", sDay + 1h);
            EXPECT_NE(first, second);
            EXPECT_EQ(readFile(first), "first");
            EXPECT_EQ(readFile(second), "second");
        }

        TEST(MWStateSaveFoldersTest, backupOfAMissingSaveShouldThrow)
        {
            const std::filesystem::path dir = testDir();
            EXPECT_ANY_THROW(backUpSave(dir / "openmw" / "Lendias" / "missing.omwsave", dir / "backups", sDay));
        }

        TEST(MWStateSaveFoldersTest, ownFolderShouldLoadFormats42And43)
        {
            EXPECT_FALSE(isSaveFormatTooNew(ESM::CurrentSaveGameFormatVersion, false));
            EXPECT_FALSE(isSaveFormatTooNew(42, false));
            EXPECT_FALSE(isSaveFormatTooNew(ESM::MaxModernMorrowSaveGameFormatVersion, false));
            EXPECT_TRUE(isSaveFormatTooNew(ESM::MaxModernMorrowSaveGameFormatVersion + 1, false));
        }

        TEST(MWStateSaveFoldersTest, openMWFolderShouldRefuseFormatsAbove41)
        {
            EXPECT_FALSE(isSaveFormatTooNew(ESM::CurrentSaveGameFormatVersion, true));
            EXPECT_TRUE(isSaveFormatTooNew(ESM::CurrentSaveGameFormatVersion + 1, true));
            EXPECT_TRUE(isSaveFormatTooNew(ESM::MaxModernMorrowSaveGameFormatVersion, true));
        }

        TEST(MWStateSaveFoldersTest, saveWithoutUiStateShouldUseTheQuestFiles)
        {
            EXPECT_TRUE(usesQuestListFiles(false, ESM::CurrentSaveGameFormatVersion));
            EXPECT_TRUE(usesQuestListFiles(false, ESM::MinSupportedSaveGameFormatVersion));
            EXPECT_FALSE(usesQuestListFiles(false, ESM::MaxModernMorrowSaveGameFormatVersion));
            EXPECT_FALSE(usesQuestListFiles(true, ESM::CurrentSaveGameFormatVersion));
        }

        TEST(MWStateSaveFoldersTest, writeShouldReplaceTheFile)
        {
            const std::filesystem::path path = testDir() / "own" / "Lendias" / "1.omwsave";
            writeFile(path, "old save, longer than the new one");
            std::istringstream save("new save");
            writeSaveFile(save, path, std::nullopt);
            EXPECT_EQ(readFile(path), "new save");
            EXPECT_FALSE(std::filesystem::exists(testDir() / "backups"));
        }

        TEST(MWStateSaveFoldersTest, writeIntoOpenMWShouldBackUpTheOldFileFirst)
        {
            const std::filesystem::path dir = testDir();
            const std::filesystem::path path = dir / "openmw" / "Lendias" / "1.omwsave";
            writeFile(path, "old save");
            std::istringstream save("new save");
            writeSaveFile(save, path, dir / "backups");
            EXPECT_EQ(readFile(path), "new save");
            const std::filesystem::directory_iterator backups(dir / "backups" / "Lendias");
            ASSERT_NE(backups, std::filesystem::directory_iterator());
            EXPECT_EQ(readFile(backups->path()), "old save");
        }

        TEST(MWStateSaveFoldersTest, failedBackupShouldLeaveTheOldFileAsItWas)
        {
            const std::filesystem::path dir = testDir();
            const std::filesystem::path path = dir / "openmw" / "Lendias" / "1.omwsave";
            writeFile(path, "old save");
            // A file where the backups folder should be, so the backup can't make its folder.
            writeFile(dir / "backups", "not a folder");
            std::istringstream save("new save");
            EXPECT_ANY_THROW(writeSaveFile(save, path, dir / "backups"));
            EXPECT_EQ(readFile(path), "old save");
        }

        TEST(MWStateSaveFoldersTest, writeOfANewFileShouldNeedNoBackup)
        {
            const std::filesystem::path dir = testDir();
            const std::filesystem::path path = dir / "openmw" / "Lendias" / "1.omwsave";
            std::filesystem::create_directories(path.parent_path());
            std::istringstream save("new save");
            writeSaveFile(save, path, dir / "backups");
            EXPECT_EQ(readFile(path), "new save");
            EXPECT_FALSE(std::filesystem::exists(dir / "backups"));
        }
    }
}
