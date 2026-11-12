#ifndef GAME_MWSTATE_STATEMANAGER_H
#define GAME_MWSTATE_STATEMANAGER_H

#include <filesystem>
#include <list>
#include <string>

namespace MWState
{
    struct Slot;
    class Character;
}

namespace MWBase
{
    /// \brief Interface for game state manager (implemented in MWState)
    class StateManager
    {
    public:
        enum State
        {
            State_NoGame,
            State_Ended,
            State_Running
        };

        typedef std::list<MWState::Character>::const_iterator CharacterIterator;

        /// The saves folders the save and load screens show: this game's own, or that of an OpenMW install on the
        /// same computer, so a game moves between the two without copying files by hand.
        enum class SaveFolder
        {
            Own,
            OpenMW
        };

    private:
        StateManager(const StateManager&);
        ///< not implemented

        StateManager& operator=(const StateManager&);
        ///< not implemented

    public:
        StateManager() {}

        virtual ~StateManager() {}

        virtual void requestQuit() = 0;

        virtual bool hasQuitRequest() const = 0;

        virtual void askLoadRecent() = 0;

        virtual void requestNewGame(bool bypass) = 0;
        virtual void requestLoad(const MWState::Character* character, const std::filesystem::path& filepath) = 0;

        virtual State getState() const = 0;

        virtual void newGame(bool bypass = false) = 0;
        ///< Start a new game.
        ///
        /// \param bypass Skip new game mechanics.

        virtual void resumeGame() = 0;

        virtual void deleteGame(const MWState::Character* character, const MWState::Slot* slot) = 0;

        virtual void saveGame(std::string_view description, const MWState::Slot* slot = nullptr) = 0;
        ///< Write a saved game to \a slot or create a new slot if \a slot == 0.
        ///
        /// \note Slot must belong to the current character.

        /// Writes a saved game into a saves folder. In OpenMW's folder, the save goes to the character with the same
        /// folder name as the current character, and a save it overwrites is first copied to this game's backups.
        ///
        /// \note Slot must belong to getSaveCharacter(folder).
        virtual void saveGame(std::string_view description, const MWState::Slot* slot, SaveFolder folder) = 0;

        /// Whether the folder can be shown. This game's own folder always can. OpenMW's can when an OpenMW install
        /// was found.
        virtual bool hasSaveFolder(SaveFolder folder) const = 0;

        /// Reads OpenMW's saves again, since OpenMW may have changed them since. Any character or slot from that
        /// folder is invalid afterwards.
        virtual void rescanSaveFolder(SaveFolder folder) = 0;

        /// The character that saving into the folder writes to. In this game's own folder, that is the current
        /// character. In OpenMW's folder, it is the character with the same folder name, if it has one.
        /// @note May return null.
        virtual const MWState::Character* getSaveCharacter(SaveFolder folder) = 0;

        virtual void loadGame(const std::filesystem::path& filepath) = 0;
        ///< Load a saved game directly from the given file path. This will search the CharacterManager
        /// for a Character containing this save file, and set this Character current if one was found.
        /// Otherwise, a new Character will be created.

        virtual void loadGame(const MWState::Character* character, const std::filesystem::path& filepath) = 0;
        ///< Load a saved game file belonging to the given character.

        /// Simple saver, writes over the file if already existing
        /** Used for quick save and autosave **/
        virtual void quickSave(std::string = "Quicksave") = 0;

        /// Simple loader, loads the last saved file
        /** Used for quickload **/
        virtual void quickLoad() = 0;

        virtual MWState::Character* getCurrentCharacter() = 0;
        ///< @note May return null.

        virtual CharacterIterator characterBegin() = 0;
        ///< Any call to SaveGame and getCurrentCharacter can invalidate the returned
        /// iterator.

        virtual CharacterIterator characterEnd() = 0;

        /// The characters in a saves folder. Any call to SaveGame or rescanSaveFolder can invalidate the iterator.
        virtual CharacterIterator characterBegin(SaveFolder folder) = 0;
        virtual CharacterIterator characterEnd(SaveFolder folder) = 0;
    };
}

#endif
