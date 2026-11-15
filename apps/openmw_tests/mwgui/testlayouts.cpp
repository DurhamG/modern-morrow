#include <gtest/gtest.h>

#include <MyGUI_XmlDocument.h>

#include <filesystem>
#include <fstream>
#include <vector>

namespace MWGui
{
    namespace
    {
        // The game's MyGUI layouts, skins and other MyGUI files. They are read with MyGUI's own parser, as the game
        // reads them. It accepts a few things a strict XML parser refuses, which two of upstream's layouts rely on.
        std::vector<std::filesystem::path> myGuiFiles()
        {
            std::vector<std::filesystem::path> files;
            const std::filesystem::path folder
                = std::filesystem::path{ OPENMW_PROJECT_SOURCE_DIR } / "files" / "data" / "mygui";
            for (const auto& entry : std::filesystem::directory_iterator(folder))
            {
                const std::filesystem::path extension = entry.path().extension();
                if (entry.is_regular_file() && (extension == ".layout" || extension == ".xml"))
                    files.push_back(entry.path());
            }
            return files;
        }

        TEST(MWGuiLayoutsTest, thereShouldBeLayoutsToCheck)
        {
            EXPECT_GT(myGuiFiles().size(), 50);
        }

        TEST(MWGuiLayoutsTest, everyLayoutShouldParse)
        {
            for (const std::filesystem::path& path : myGuiFiles())
            {
                std::ifstream stream(path, std::ios::binary);
                MyGUI::xml::Document document;
                EXPECT_TRUE(document.open(stream)) << path.filename() << ": " << document.getLastError();
            }
        }

        TEST(MWGuiLayoutsTest, everyLayoutShouldHaveAMyGuiRoot)
        {
            for (const std::filesystem::path& path : myGuiFiles())
            {
                std::ifstream stream(path, std::ios::binary);
                MyGUI::xml::Document document;
                if (!document.open(stream))
                    continue;
                ASSERT_NE(document.getRoot(), nullptr) << path.filename();
                EXPECT_EQ(document.getRoot()->getName(), "MyGUI") << path.filename();
            }
        }
    }
}
