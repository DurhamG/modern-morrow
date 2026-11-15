#include "apps/mwiniimporter/importer.hpp"

#include <components/testing/util.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{
    using Names = std::vector<std::string>;

    // A Morrowind folder of the test's own, with Morrowind.ini and a Data Files folder.
    struct MwIniImporterTest : ::testing::Test
    {
        std::filesystem::path mDir = TestingOpenMW::currentTestDirPath();
        std::filesystem::path mIni = mDir / "Morrowind" / "Morrowind.ini";
        std::filesystem::path mDataFiles = mDir / "Morrowind" / "Data Files";
        MwIniImporter mImporter;
        MwIniImporter::multistrmap mCfg;

        MwIniImporterTest() { std::filesystem::create_directories(mDataFiles); }

        void writeIni(const std::string& text)
        {
            std::ofstream file(mIni, std::ios::binary);
            file << text;
        }

        static void addFile(const std::filesystem::path& path)
        {
            std::filesystem::create_directories(path.parent_path());
            std::ofstream file(path, std::ios::binary);
        }

        Names importArchives()
        {
            const MwIniImporter::multistrmap ini = mImporter.loadIniFile(mIni);
            mImporter.importArchives(mCfg, ini, mIni);
            return mCfg["fallback-archive"];
        }
    };

    TEST_F(MwIniImporterTest, archiveInTheInisDataFilesFolderShouldBeKept)
    {
        addFile(mDataFiles / "Tribunal.bsa");
        writeIni("[Archives]\r\nArchive 0=Tribunal.bsa\r\n");
        EXPECT_EQ(importArchives(), (Names{ "Morrowind.bsa", "Tribunal.bsa" }));
    }

    TEST_F(MwIniImporterTest, archiveInADataFolderFromTheCfgShouldBeKept)
    {
        addFile(mDir / "mods" / "TR_Data.bsa");
        addFile(mDir / "local" / "PC_Data.bsa");
        mCfg["data"] = { "\"" + (mDir / "mods").string() + "\"" };
        mCfg["data-local"] = { (mDir / "local").string() };
        writeIni("[Archives]\r\nArchive 0=TR_Data.bsa\r\nArchive 1=PC_Data.bsa\r\n");
        EXPECT_EQ(importArchives(), (Names{ "Morrowind.bsa", "TR_Data.bsa", "PC_Data.bsa" }));
    }

    TEST_F(MwIniImporterTest, missingArchiveShouldBeLeftOut)
    {
        addFile(mDataFiles / "Tribunal.bsa");
        writeIni("[Archives]\r\nArchive 0=Tribunal.bsa\r\nArchive 1=PC_Data.bsa\r\n");
        EXPECT_EQ(importArchives(), (Names{ "Morrowind.bsa", "Tribunal.bsa" }));
    }

    TEST_F(MwIniImporterTest, archiveWithDifferentCapitalisationShouldBeKept)
    {
        addFile(mDataFiles / "Tribunal.BSA");
        writeIni("[Archives]\r\nArchive 0=tribunal.bsa\r\n");
        EXPECT_EQ(importArchives(), (Names{ "Morrowind.bsa", "tribunal.bsa" }));
    }

    TEST_F(MwIniImporterTest, dataFolderThatDoesNotExistShouldBeSkipped)
    {
        addFile(mDataFiles / "Tribunal.bsa");
        mCfg["data"] = { (mDir / "missing").string() };
        writeIni("[Archives]\r\nArchive 0=Tribunal.bsa\r\n");
        EXPECT_EQ(importArchives(), (Names{ "Morrowind.bsa", "Tribunal.bsa" }));
    }

    TEST_F(MwIniImporterTest, orderShouldFollowTheIni)
    {
        addFile(mDataFiles / "Tribunal.bsa");
        addFile(mDataFiles / "Bloodmoon.bsa");
        writeIni("[Archives]\r\nArchive 0=Bloodmoon.bsa\r\nArchive 1=Tribunal.bsa\r\n");
        EXPECT_EQ(importArchives(), (Names{ "Morrowind.bsa", "Bloodmoon.bsa", "Tribunal.bsa" }));
    }

    TEST_F(MwIniImporterTest, iniWithoutAnArchivesSectionShouldGiveOnlyMorrowindsArchive)
    {
        writeIni("[General]\r\nSomething=1\r\n");
        EXPECT_EQ(importArchives(), (Names{ "Morrowind.bsa" }));
    }

    TEST_F(MwIniImporterTest, earlierArchivesShouldBeReplaced)
    {
        addFile(mDataFiles / "Tribunal.bsa");
        mCfg["fallback-archive"] = { "Morrowind.bsa", "Old.bsa" };
        writeIni("[Archives]\r\nArchive 0=Tribunal.bsa\r\n");
        EXPECT_EQ(importArchives(), (Names{ "Morrowind.bsa", "Tribunal.bsa" }));
    }

    TEST_F(MwIniImporterTest, missingPluginShouldBeLeftOut)
    {
        writeIni("[Game Files]\r\nGameFile0=Missing.esm\r\n");
        const MwIniImporter::multistrmap ini = mImporter.loadIniFile(mIni);
        mImporter.importGameFiles(mCfg, ini, mIni);
        EXPECT_TRUE(mCfg["content"].empty());
    }
}
