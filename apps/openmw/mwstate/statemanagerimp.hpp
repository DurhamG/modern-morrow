#ifndef GAME_STATE_STATEMANAGER_H
#define GAME_STATE_STATEMANAGER_H

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <utility>

#include "../mwbase/statemanager.hpp"

#include "charactermanager.hpp"

namespace MWState
{
    class StateManager : public MWBase::StateManager
    {
    private:
        struct NewGameRequest
        {
            bool mBypass;
        };

        bool mQuitRequest;
        bool mAskLoadRecent;
        std::optional<NewGameRequest> mNewGameRequest;
        std::optional<std::pair<const Character*, std::filesystem::path>> mLoadRequest;
        State mState;
        CharacterManager mCharacterManager;
        std::vector<std::string> mContentFiles;
        // The saves folder of an OpenMW install on this computer, if one was found, and its characters, once the
        // folder exists.
        std::filesystem::path mOpenMWSaves;
        std::optional<CharacterManager> mOpenMWCharacters;
        // When the game was loaded from OpenMW's saves and this game has no character folder of that name yet, the
        // folder's name, so the first save here creates a character folder of the same name.
        std::filesystem::path mOpenMWFolderName;
        double mTimePlayed;
        // Seconds of play, outside menus, since the game was last saved or loaded.
        double mPlayedSinceSave = 0;
        std::filesystem::path mLastSavegame;

        void cleanup(bool force = false);

        void printSavegameFormatError(const std::string& exceptionText, const std::string& messageBoxText);

        bool confirmLoading(const std::vector<std::string_view>& missingFiles) const;

        void writeScreenshot(std::vector<char>& imageData) const;

        /// Autosaves once the player has played for the autosave interval without saving.
        void updateTimedAutosave(float duration);

        /// The current character's saves with the given description, such as "Quicksave".
        std::vector<const Slot*> findSaves(std::string_view description);

        std::map<int, int> buildContentFileIndexMap(const ESM::ESMReader& reader) const;

        /// Whether the save file is in OpenMW's saves folder.
        bool isOpenMWSave(const std::filesystem::path& filepath) const;

        /// The name of the current character's folder, or of the OpenMW character folder the game was loaded from.
        /// Empty for a new game that was never saved.
        std::filesystem::path currentFolderName();

        Character* findSaveCharacter(SaveFolder folder);

        /// OpenMW's characters, creating its saves folder if it doesn't exist yet.
        CharacterManager& openMWCharacters();

        /// Copies an OpenMW save that is about to be overwritten into this game's backups folder.
        void backUpOpenMWSave(const std::filesystem::path& filepath) const;

    public:
        StateManager(const std::filesystem::path& saves, const std::vector<std::string>& contentFiles);

        void requestQuit() override;

        bool hasQuitRequest() const override;

        void askLoadRecent() override;

        void requestNewGame(bool bypass) override { mNewGameRequest = NewGameRequest{ .mBypass = bypass }; }

        void requestLoad(const Character* character, const std::filesystem::path& filepath) override
        {
            mLoadRequest.emplace(character, filepath);
        }

        State getState() const override;

        void newGame(bool bypass = false) override;
        ///< Start a new game.
        ///
        /// \param bypass Skip new game mechanics.

        void endGame();

        void resumeGame() override;

        void deleteGame(const MWState::Character* character, const MWState::Slot* slot) override;
        ///< Delete a saved game slot from this character. If all save slots are deleted, the character will be deleted
        ///< too.

        void saveGame(std::string_view description, const Slot* slot = nullptr) override;
        ///< Write a saved game to \a slot or create a new slot if \a slot == 0.
        ///
        /// \note Slot must belong to the current character.

        void saveGame(std::string_view description, const Slot* slot, SaveFolder folder) override;

        bool hasSaveFolder(SaveFolder folder) const override;

        void rescanSaveFolder(SaveFolder folder) override;

        const Character* getSaveCharacter(SaveFolder folder) override;

        /// Saves a file, using supplied filename, overwritting if needed
        /** This is mostly used for quicksaving and autosaving, for they use the same name over and over again
            \param name Name of save, defaults to "Quicksave"**/
        void quickSave(std::string name = "Quicksave") override;

        /// Loads the last saved file
        /** Used for quickload **/
        void quickLoad() override;

        void loadGame(const std::filesystem::path& filepath) override;
        ///< Load a saved game directly from the given file path. This will search the CharacterManager
        /// for a Character containing this save file, and set this Character current if one was found.
        /// Otherwise, a new Character will be created.

        void loadGame(const Character* character, const std::filesystem::path& filepath) override;
        ///< Load a saved game file belonging to the given character.

        Character* getCurrentCharacter() override;
        ///< @note May return null.

        CharacterIterator characterBegin() override;
        ///< Any call to SaveGame and getCurrentCharacter can invalidate the returned
        /// iterator.

        CharacterIterator characterEnd() override;

        CharacterIterator characterBegin(SaveFolder folder) override;
        CharacterIterator characterEnd(SaveFolder folder) override;

        void update(float duration);
    };
}

#endif
