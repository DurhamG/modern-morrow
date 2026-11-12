#ifndef GAME_STATE_CHARACTERMANAGER_H
#define GAME_STATE_CHARACTERMANAGER_H

#include <filesystem>
#include <list>

#include "character.hpp"

namespace MWState
{
    class CharacterManager
    {
        std::filesystem::path mPath;

        // Uses std::list, so that mCurrent stays valid when characters are deleted
        std::list<Character> mCharacters;

        Character* mCurrent;
        std::string mGame;

    private:
        CharacterManager(const CharacterManager&);
        ///< Not implemented

        CharacterManager& operator=(const CharacterManager&);
        ///< Not implemented

        std::list<Character>::iterator findCharacter(const MWState::Character* character);

    public:
        CharacterManager(std::filesystem::path saves, const std::vector<std::string>& contentFiles);

        Character* getCurrentCharacter();
        ///< @note May return null

        void deleteSlot(const MWState::Slot* slot, const Character*& character);

        Character* createCharacter(const std::string& name);
        ///< Create new character within saved game management
        /// \param name Name for the character (does not need to be unique)

        /// Returns the character whose saves are in the folder of this name, creating the character when there is
        /// none. Saving into another OpenMW's saves uses it, so a character keeps the folder name it has here.
        Character* findOrCreateCharacter(const std::filesystem::path& folderName);

        /// Returns the character whose saves are in the folder of this name, or null when there is none.
        Character* findCharacter(const std::filesystem::path& folderName);

        /// The folder that holds a folder for each character.
        const std::filesystem::path& getPath() const { return mPath; }

        void setCurrentCharacter(const Character* character);

        std::list<Character>::const_iterator begin() const;

        std::list<Character>::const_iterator end() const;
    };
}

#endif
