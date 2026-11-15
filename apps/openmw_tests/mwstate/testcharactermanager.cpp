#include "apps/openmw/mwstate/charactermanager.hpp"

#include <components/testing/util.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

namespace MWState
{
    namespace
    {
        const std::vector<std::string> sContentFiles{ "Morrowind.esm" };

        std::filesystem::path savesDir()
        {
            return TestingOpenMW::currentTestDirPath() / "saves";
        }

        TEST(MWStateCharacterManagerTest, getPathShouldBeTheSavesFolder)
        {
            CharacterManager characters(savesDir(), sContentFiles);
            EXPECT_EQ(characters.getPath(), savesDir());
            EXPECT_TRUE(std::filesystem::is_directory(savesDir()));
        }

        TEST(MWStateCharacterManagerTest, findCharacterShouldMatchTheFolderName)
        {
            CharacterManager characters(savesDir(), sContentFiles);
            const Character* lendias = characters.findOrCreateCharacter("Lendias");
            characters.findOrCreateCharacter("Vickor");
            EXPECT_EQ(characters.findCharacter("Lendias"), lendias);
        }

        TEST(MWStateCharacterManagerTest, findCharacterShouldReturnNullForAnUnknownName)
        {
            CharacterManager characters(savesDir(), sContentFiles);
            characters.findOrCreateCharacter("Lendias");
            EXPECT_EQ(characters.findCharacter("Vickor"), nullptr);
            EXPECT_EQ(characters.findCharacter("Lendia"), nullptr);
        }

        TEST(MWStateCharacterManagerTest, findCharacterShouldIgnoreCase)
        {
            CharacterManager characters(savesDir(), sContentFiles);
            const Character* lendias = characters.findOrCreateCharacter("Lendias");
            EXPECT_EQ(characters.findCharacter("lendias"), lendias);
        }

        TEST(MWStateCharacterManagerTest, findOrCreateShouldReturnTheExistingCharacter)
        {
            CharacterManager characters(savesDir(), sContentFiles);
            const Character* first = characters.findOrCreateCharacter("Lendias");
            EXPECT_EQ(characters.findOrCreateCharacter("Lendias"), first);
        }

        TEST(MWStateCharacterManagerTest, findOrCreateShouldCreateAFolderWithTheGivenName)
        {
            CharacterManager characters(savesDir(), sContentFiles);
            const Character* character = characters.findOrCreateCharacter("J. Doe");
            EXPECT_EQ(character->getPath(), savesDir() / "J. Doe");
            EXPECT_TRUE(std::filesystem::is_directory(savesDir() / "J. Doe"));
        }

        TEST(MWStateCharacterManagerTest, findOrCreateShouldUseAFolderThatAlreadyExists)
        {
            std::filesystem::create_directories(savesDir() / "Lendias");
            CharacterManager characters(savesDir(), sContentFiles);
            const Character* character = characters.findOrCreateCharacter("Lendias");
            EXPECT_EQ(character->getPath(), savesDir() / "Lendias");
            EXPECT_FALSE(std::filesystem::exists(savesDir() / "Lendias - 1"));
        }

        TEST(MWStateCharacterManagerTest, createCharacterShouldNotReuseAnExistingFolder)
        {
            CharacterManager characters(savesDir(), sContentFiles);
            const Character* first = characters.createCharacter("Lendias");
            const Character* second = characters.createCharacter("Lendias");
            EXPECT_NE(first, second);
            EXPECT_EQ(first->getPath(), savesDir() / "Lendias");
            EXPECT_EQ(second->getPath(), savesDir() / "Lendias - 1");
        }

        TEST(MWStateCharacterManagerTest, createCharacterShouldReplaceUnsafeCharacters)
        {
            CharacterManager characters(savesDir(), sContentFiles);
            EXPECT_EQ(characters.createCharacter("J. Doe")->getPath(), savesDir() / "J__Doe");
        }

        TEST(MWStateCharacterManagerTest, currentCharacterShouldBeTheOneSet)
        {
            CharacterManager characters(savesDir(), sContentFiles);
            EXPECT_EQ(characters.getCurrentCharacter(), nullptr);
            const Character* lendias = characters.findOrCreateCharacter("Lendias");
            characters.setCurrentCharacter(lendias);
            EXPECT_EQ(characters.getCurrentCharacter(), lendias);
        }
    }
}
