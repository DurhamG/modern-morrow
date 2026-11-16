#include "apps/openmw/mwstate/charactermanager.hpp"

#include <components/esm3/esmwriter.hpp>
#include <components/esm3/formatversion.hpp>
#include <components/esm3/savedgame.hpp>
#include <components/testing/util.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
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

        // Writes a save that the character manager lists for a game of Morrowind.esm.
        void writeSave(const std::filesystem::path& path)
        {
            std::filesystem::create_directories(path.parent_path());
            ESM::SavedGame profile;
            profile.mContentFiles = sContentFiles;
            profile.mPlayerName = "Lendias";
            ESM::ESMWriter writer;
            writer.setFormatVersion(ESM::CurrentSaveGameFormatVersion);
            std::ofstream file(path, std::ios::binary);
            writer.save(file);
            writer.startRecord(ESM::REC_SAVE);
            profile.save(writer);
            writer.endRecord(ESM::REC_SAVE);
            writer.close();
        }

        TEST(MWStateCharacterManagerTest, findCharacterShouldPreferAnExactMatch)
        {
            std::filesystem::create_directories(savesDir() / "Lendias");
            if (std::filesystem::exists(savesDir() / "lendias"))
                GTEST_SKIP() << "This file system ignores case, so it can't hold two such folders";
            writeSave(savesDir() / "Lendias" / "1.omwsave");
            writeSave(savesDir() / "lendias" / "1.omwsave");
            CharacterManager characters(savesDir(), sContentFiles);
            ASSERT_NE(characters.findCharacter("Lendias"), nullptr);
            ASSERT_NE(characters.findCharacter("lendias"), nullptr);
            EXPECT_EQ(characters.findCharacter("Lendias")->getPath(), savesDir() / "Lendias");
            EXPECT_EQ(characters.findCharacter("lendias")->getPath(), savesDir() / "lendias");
            EXPECT_NE(characters.findCharacter("LENDIAS"), nullptr);
        }

        TEST(MWStateCharacterManagerTest, savedCharacterShouldBeFoundAfterARestart)
        {
            writeSave(savesDir() / "Lendias" / "1.omwsave");
            CharacterManager characters(savesDir(), sContentFiles);
            ASSERT_NE(characters.findCharacter("Lendias"), nullptr);
            EXPECT_EQ(characters.findCharacter("Lendias")->getPath(), savesDir() / "Lendias");
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

        TEST(MWStateCharacterManagerTest, createCharacterShouldAvoidNamesUsedInAnotherSavesFolder)
        {
            const std::filesystem::path other = TestingOpenMW::currentTestDirPath() / "other";
            std::filesystem::create_directories(other / "Lendias");
            std::filesystem::create_directories(other / "Lendias - 1");
            CharacterManager characters(savesDir(), sContentFiles);
            EXPECT_EQ(characters.createCharacter("Lendias", other)->getPath(), savesDir() / "Lendias - 2");
            EXPECT_EQ(characters.createCharacter("Vickor", other)->getPath(), savesDir() / "Vickor");
        }

        TEST(MWStateCharacterManagerTest, createCharacterShouldReplaceUnsafeCharacters)
        {
            CharacterManager characters(savesDir(), sContentFiles);
            EXPECT_EQ(characters.createCharacter("J. Doe")->getPath(), savesDir() / "J__Doe");
        }

        TEST(MWStateCharacterFolderNameTest, shouldKeepAsciiLettersAndDigits)
        {
            EXPECT_EQ(characterFolderName("Lendias"), "Lendias");
            EXPECT_EQ(characterFolderName("Vivec2"), "Vivec2");
        }

        TEST(MWStateCharacterFolderNameTest, shouldReplaceOtherCharactersWithUnderscores)
        {
            EXPECT_EQ(characterFolderName("J. Doe"), "J__Doe");
            EXPECT_EQ(characterFolderName("a/b\\c:d"), "a_b_c_d");
        }

        TEST(MWStateCharacterFolderNameTest, shouldReplaceEachLetterOutsideAsciiWithOneUnderscore)
        {
            // "Séverine" in UTF-8, where é is two bytes.
            EXPECT_EQ(characterFolderName("S\xc3\xa9verine"), "S_verine");
        }

        TEST(MWStateCharacterFolderNameTest, shouldBeTheFolderACharacterIsCreatedIn)
        {
            CharacterManager characters(savesDir(), sContentFiles);
            EXPECT_EQ(characters.createCharacter("Fargoth the Elder")->getPath(),
                savesDir() / characterFolderName("Fargoth the Elder"));
        }

        TEST(MWStateCharacterManagerTest, addSaveShouldAddTheFileAsTheCharactersNewestSave)
        {
            writeSave(savesDir() / "Lendias" / "1.omwsave");
            CharacterManager characters(savesDir(), sContentFiles);
            writeSave(savesDir() / "Lendias" / "Cloud.omwsave");
            const Character* character = characters.addSave(savesDir() / "Lendias" / "Cloud.omwsave");
            EXPECT_EQ(character, characters.findCharacter("Lendias"));
            ASSERT_NE(character->begin(), character->end());
            EXPECT_EQ(character->begin()->mPath, savesDir() / "Lendias" / "Cloud.omwsave");
            EXPECT_EQ(std::distance(character->begin(), character->end()), 2);
        }

        TEST(MWStateCharacterManagerTest, addSaveShouldMakeTheCharacterWhenThereIsNone)
        {
            CharacterManager characters(savesDir(), sContentFiles);
            writeSave(savesDir() / "Vivec" / "Cloud.omwsave");
            const Character* character = characters.addSave(savesDir() / "Vivec" / "Cloud.omwsave");
            EXPECT_EQ(character->getPath(), savesDir() / "Vivec");
            EXPECT_EQ(characters.findCharacter("Vivec"), character);
            EXPECT_EQ(std::distance(character->begin(), character->end()), 1);
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
