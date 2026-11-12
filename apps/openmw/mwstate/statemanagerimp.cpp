#include "statemanagerimp.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <system_error>

#include <SDL_clipboard.h>

#include <components/debug/debuglog.hpp>

#include <components/esm3/actoridconverter.hpp>
#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>
#include <components/esm3/loadcell.hpp>
#include <components/esm3/loadclas.hpp>
#include <components/esm3/uistate.hpp>

#include <components/l10n/manager.hpp>

#include <components/loadinglistener/loadinglistener.hpp>

#include <components/files/conversion.hpp>
#include <components/files/fixedpath.hpp>
#include <components/misc/algorithm.hpp>
#include <components/misc/timeconvert.hpp>
#include <components/settings/values.hpp>

#include "../mwbase/dialoguemanager.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/journal.hpp"
#include "../mwbase/luamanager.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/scriptmanager.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/cellstore.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/datetimemanager.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/globals.hpp"
#include "../mwworld/player.hpp"
#include "../mwworld/scene.hpp"
#include "../mwworld/worldmodel.hpp"

#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/npcstats.hpp"

#include "../mwscript/globalscripts.hpp"

#include "quicksavemanager.hpp"

namespace
{
    // The quests pinned in the character's latest save from this game, one quest ID to a line. OpenMW versions without
    // the controller menus drop the save's record of the pins, so a save they wrote takes its pins from here. The file
    // sits in the folder of all the characters, named after the character's folder, such as
    // "Lendias.pinned-quests.txt". A character's folder holds only saves, and the save list reads every file in it.
    std::filesystem::path pinnedQuestsPath(const std::filesystem::path& characterDir)
    {
        std::filesystem::path name = characterDir.filename();
        name += ".pinned-quests.txt";
        return characterDir.parent_path() / name;
    }

    void writePinnedQuests(const std::filesystem::path& characterDir, const std::vector<ESM::RefId>& quests)
    {
        std::ofstream file(pinnedQuestsPath(characterDir), std::ios::trunc);
        for (const ESM::RefId& quest : quests)
            file << quest.serializeText() << '\n';
        if (file.fail())
            Log(Debug::Warning) << "Failed to write the pinned quests to " << pinnedQuestsPath(characterDir);
    }

    std::vector<ESM::RefId> readPinnedQuests(const std::filesystem::path& characterDir)
    {
        std::vector<ESM::RefId> quests;
        std::ifstream file(pinnedQuestsPath(characterDir));
        std::string line;
        try
        {
            while (std::getline(file, line))
                if (!line.empty())
                    quests.push_back(ESM::RefId::deserializeText(line));
        }
        catch (const std::exception& e)
        {
            Log(Debug::Warning) << "Failed to read the pinned quests from " << pinnedQuestsPath(characterDir) << ": "
                                << e.what();
        }
        return quests;
    }

    // When a file in the saves folder was last written, or the oldest possible time when it has no saves.
    std::filesystem::file_time_type newestSave(const std::filesystem::path& saves)
    {
        std::filesystem::file_time_type newest = std::filesystem::file_time_type::min();
        std::error_code ec;
        for (const auto& characterDir : std::filesystem::directory_iterator(saves, ec))
        {
            if (!characterDir.is_directory(ec))
                continue;
            for (const auto& file : std::filesystem::directory_iterator(characterDir.path(), ec))
                if (file.is_regular_file(ec))
                    newest = std::max(newest, file.last_write_time(ec));
        }
        return newest;
    }

    // Finds the saves folder of an OpenMW install on this computer. The "openmw saves folder" setting can name it.
    // Otherwise, on Linux, it is either the Flatpak, which app stores such as the Steam Deck's Discover install, or
    // OpenMW's usual folder. Elsewhere, it is OpenMW's usual folder. When more than one is installed, it is the one
    // saved to most recently. Returns an empty path when there is no install.
    std::filesystem::path findOpenMWSaves(const std::filesystem::path& ownSaves)
    {
        const std::string& setting = Settings::saves().mOpenMWSavesFolder;
        if (!setting.empty())
            return Files::pathFromUnicodeString(setting);

        // An install has a config folder, which its launcher and setup wizard make, and a saves folder, which its
        // first save makes.
        std::vector<std::pair<std::filesystem::path, std::filesystem::path>> installs;
#if defined(__linux__)
        if (const char* home = std::getenv("HOME"))
        {
            const std::filesystem::path flatpak = std::filesystem::path(home) / ".var/app/org.openmw.OpenMW";
            installs.emplace_back(flatpak / "config/openmw", flatpak / "data/openmw/saves");
        }
#endif
        const Files::FixedPath<> openmw("openmw");
        installs.emplace_back(openmw.getUserConfigPath(), openmw.getUserDataPath() / "saves");

        std::filesystem::path found;
        std::filesystem::file_time_type foundNewest = std::filesystem::file_time_type::min();
        std::error_code ec;
        for (const auto& [config, saves] : installs)
        {
            if (!std::filesystem::is_directory(config, ec) && !std::filesystem::is_directory(saves, ec))
                continue;
            if (std::filesystem::equivalent(saves, ownSaves, ec))
                continue;
            const std::filesystem::file_time_type newest = newestSave(saves);
            if (found.empty() || newest > foundNewest)
            {
                found = saves;
                foundNewest = newest;
            }
        }
        return found;
    }
}

void MWState::StateManager::cleanup(bool force)
{
    if (mState != State_NoGame || force)
    {
        MWBase::Environment::get().getSoundManager()->clear();
        MWBase::Environment::get().getDialogueManager()->clear();
        MWBase::Environment::get().getJournal()->clear();
        MWBase::Environment::get().getScriptManager()->clear();
        MWBase::Environment::get().getWindowManager()->clear();
        MWBase::Environment::get().getWorld()->clear();
        MWBase::Environment::get().getInputManager()->clear();
        MWBase::Environment::get().getMechanicsManager()->clear();

        mCharacterManager.setCurrentCharacter(nullptr);
        mOpenMWFolderName.clear();
        mTimePlayed = 0;
        mPlayedSinceSave = 0;
        mLastSavegame.clear();

        mState = State_NoGame;
        MWBase::Environment::get().getLuaManager()->noGame();
    }
    else
    {
        // TODO: do we need this cleanup?
        MWBase::Environment::get().getLuaManager()->clear();
    }
}

std::map<int, int> MWState::StateManager::buildContentFileIndexMap(const ESM::ESMReader& reader) const
{
    const std::vector<std::string>& current = MWBase::Environment::get().getWorld()->getContentFiles();

    const std::vector<ESM::Header::MasterData>& prev = reader.getGameFiles();

    std::map<int, int> map;

    for (int iPrev = 0; iPrev < static_cast<int>(prev.size()); ++iPrev)
    {
        for (int iCurrent = 0; iCurrent < static_cast<int>(current.size()); ++iCurrent)
            if (Misc::StringUtils::ciEqual(prev[iPrev].name, current[iCurrent]))
            {
                map.insert(std::make_pair(iPrev, iCurrent));
                break;
            }
    }

    return map;
}

MWState::StateManager::StateManager(const std::filesystem::path& saves, const std::vector<std::string>& contentFiles)
    : mQuitRequest(false)
    , mAskLoadRecent(false)
    , mState(State_NoGame)
    , mCharacterManager(saves, contentFiles)
    , mContentFiles(contentFiles)
    , mOpenMWSaves(findOpenMWSaves(saves))
    , mTimePlayed(0)
{
    if (!mOpenMWSaves.empty())
        Log(Debug::Info) << "Found OpenMW's saves folder " << mOpenMWSaves;
    rescanSaveFolder(SaveFolder::OpenMW);
}

bool MWState::StateManager::hasSaveFolder(SaveFolder folder) const
{
    return folder == SaveFolder::Own || !mOpenMWSaves.empty();
}

void MWState::StateManager::rescanSaveFolder(SaveFolder folder)
{
    // This game keeps its own list up to date as it saves and deletes.
    if (folder != SaveFolder::OpenMW)
        return;
    mOpenMWCharacters.reset();
    std::error_code ec;
    if (mOpenMWSaves.empty() || !std::filesystem::is_directory(mOpenMWSaves, ec))
        return;
    try
    {
        mOpenMWCharacters.emplace(mOpenMWSaves, mContentFiles);
    }
    catch (const std::exception& e)
    {
        Log(Debug::Warning) << "Failed to read OpenMW's saves in " << mOpenMWSaves << ": " << e.what();
        mOpenMWCharacters.reset();
    }
}

MWState::CharacterManager& MWState::StateManager::openMWCharacters()
{
    if (mOpenMWSaves.empty())
        throw std::runtime_error("No OpenMW install was found");
    if (!mOpenMWCharacters)
        mOpenMWCharacters.emplace(mOpenMWSaves, mContentFiles);
    return *mOpenMWCharacters;
}

bool MWState::StateManager::isOpenMWSave(const std::filesystem::path& filepath) const
{
    std::error_code ec;
    return !mOpenMWSaves.empty() && std::filesystem::equivalent(filepath.parent_path().parent_path(), mOpenMWSaves, ec);
}

std::filesystem::path MWState::StateManager::currentFolderName()
{
    if (const Character* character = mCharacterManager.getCurrentCharacter())
        return character->getPath().filename();
    return mOpenMWFolderName;
}

MWState::Character* MWState::StateManager::findSaveCharacter(SaveFolder folder)
{
    if (folder == SaveFolder::Own)
        return mCharacterManager.getCurrentCharacter();
    const std::filesystem::path folderName = currentFolderName();
    if (!mOpenMWCharacters || folderName.empty())
        return nullptr;
    return mOpenMWCharacters->findCharacter(folderName);
}

const MWState::Character* MWState::StateManager::getSaveCharacter(SaveFolder folder)
{
    return findSaveCharacter(folder);
}

void MWState::StateManager::backUpOpenMWSave(const std::filesystem::path& filepath) const
{
    // The backups go in this game's own folder, since OpenMW would list a file in its character folders as a save.
    const std::filesystem::path folder
        = mCharacterManager.getPath().parent_path() / "backups" / "OpenMW" / filepath.parent_path().filename();
    std::filesystem::create_directories(folder);
    std::filesystem::path name = filepath.stem();
    name += Misc::fileTimeToString(std::filesystem::file_time_type::clock::now(), " %Y-%m-%d %H-%M-%S");
    name += filepath.extension();
    std::filesystem::copy_file(filepath, folder / name, std::filesystem::copy_options::overwrite_existing);
    Log(Debug::Info) << "Copied " << filepath << " to " << folder / name << " before saving over it";
}

void MWState::StateManager::requestQuit()
{
    mQuitRequest = true;
}

bool MWState::StateManager::hasQuitRequest() const
{
    return mQuitRequest;
}

void MWState::StateManager::askLoadRecent()
{
    if (MWBase::Environment::get().getWindowManager()->getMode() == MWGui::GM_MainMenu)
        return;

    if (!mAskLoadRecent)
    {
        if (mLastSavegame.empty()) // no saves
        {
            MWBase::Environment::get().getWindowManager()->pushGuiMode(MWGui::GM_MainMenu);
        }
        else
        {
            std::string saveName = Files::pathToUnicodeString(mLastSavegame.filename());
            // Assume the last saved game belongs to the current character's slot list.
            const Character* character = getCurrentCharacter();
            if (character)
            {
                for (const auto& slot : *character)
                {
                    if (slot.mPath == mLastSavegame)
                    {
                        saveName = slot.mProfile.mDescription;
                        break;
                    }
                }
            }

            std::vector<std::string> buttons;
            buttons.emplace_back("#{Interface:Yes}");
            buttons.emplace_back("#{Interface:No}");
            auto l10n = MWBase::Environment::get().getL10nManager()->getContext("OMWEngine");
            std::string message = l10n->formatMessage("AskLoadLastSave", { "save" }, { L10n::toUnicode(saveName) });
            MWBase::Environment::get().getWindowManager()->interactiveMessageBox(message, buttons);
            mAskLoadRecent = true;
        }
    }
}

MWState::StateManager::State MWState::StateManager::getState() const
{
    return mState;
}

void MWState::StateManager::newGame(bool bypass)
{
    cleanup();

    if (!bypass)
        MWBase::Environment::get().getWindowManager()->setNewGame(true);

    try
    {
        Log(Debug::Info) << "Starting a new game";
        MWBase::Environment::get().getScriptManager()->getGlobalScripts().addStartup();
        MWBase::Environment::get().getWorld()->startNewGame(bypass);

        mState = State_Running;
        MWBase::Environment::get().getLuaManager()->gameLoaded();

        MWBase::Environment::get().getWindowManager()->fadeScreenOut(0);
        MWBase::Environment::get().getWindowManager()->fadeScreenIn(1);
    }
    catch (std::exception& e)
    {
        std::stringstream error;
        error << "Failed to start new game: " << e.what();

        Log(Debug::Error) << error.str();
        cleanup(true);

        MWBase::Environment::get().getWindowManager()->pushGuiMode(MWGui::GM_MainMenu);

        std::vector<std::string> buttons;
        buttons.emplace_back("#{Interface:OK}");
        MWBase::Environment::get().getWindowManager()->interactiveMessageBox(error.str(), buttons);
    }
}

void MWState::StateManager::endGame()
{
    mState = State_Ended;
    MWBase::Environment::get().getLuaManager()->gameEnded();
}

void MWState::StateManager::resumeGame()
{
    mState = State_Running;
    MWBase::Environment::get().getLuaManager()->gameLoaded();
}

void MWState::StateManager::saveGame(std::string_view description, const Slot* slot)
{
    saveGame(description, slot, SaveFolder::Own);
}

void MWState::StateManager::saveGame(std::string_view description, const Slot* slot, SaveFolder folder)
{
    MWBase::Environment::get().getLuaManager()->applyDelayedActions();

    const bool toOpenMW = folder == SaveFolder::OpenMW;
    MWState::Character* character = nullptr;

    try
    {
        const auto start = std::chrono::steady_clock::now();

        MWBase::Environment::get().getWindowManager()->asyncPrepareSaveMap();

        character = findSaveCharacter(folder);
        if (!character)
        {
            // A game loaded from one folder keeps its character folder's name in the other.
            CharacterManager& characters = toOpenMW ? openMWCharacters() : mCharacterManager;
            const std::filesystem::path folderName = currentFolderName();
            if (folderName.empty())
            {
                MWWorld::ConstPtr player = MWMechanics::getPlayer();
                character = characters.createCharacter(player.get<ESM::NPC>()->mBase->mName);
            }
            else
                character = characters.findOrCreateCharacter(folderName);
            if (!toOpenMW)
                mCharacterManager.setCurrentCharacter(character);
        }

        ESM::SavedGame profile;

        MWBase::World& world = *MWBase::Environment::get().getWorld();

        MWWorld::Ptr player = world.getPlayerPtr();

        profile.mContentFiles = world.getContentFiles();

        profile.mPlayerName = player.get<ESM::NPC>()->mBase->mName;
        profile.mPlayerLevel = player.getClass().getNpcStats(player).getLevel();

        const ESM::RefId& classId = player.get<ESM::NPC>()->mBase->mClass;
        if (world.getStore().get<ESM::Class>().isDynamic(classId))
            profile.mPlayerClassName = world.getStore().get<ESM::Class>().find(classId)->mName;
        else
            profile.mPlayerClassId = classId;

        const MWMechanics::CreatureStats& stats = player.getClass().getCreatureStats(player);

        profile.mPlayerCellName = world.getCellName();
        profile.mInGameTime = world.getTimeManager()->getEpochTimeStamp();
        profile.mTimePlayed = mTimePlayed;
        profile.mDescription = description;
        profile.mCurrentDay = world.getTimeManager()->getTimeStamp().getDay();
        profile.mCurrentHealth = stats.getHealth().getCurrent();
        profile.mMaximumHealth = stats.getHealth().getModified();

        Log(Debug::Info) << "Making a screenshot for saved game '" << description << "'";
        writeScreenshot(profile.mScreenshot);

        if (!slot)
            slot = character->createSlot(profile);
        else
            slot = character->updateSlot(slot, profile);

        // Make sure the animation state held by references is up to date before saving the game.
        MWBase::Environment::get().getMechanicsManager()->persistAnimationStates();

        Log(Debug::Info) << "Writing saved game '" << description << "' for character '" << profile.mPlayerName << "'";

        // Write to a memory stream first. If there is an exception during the save process, we don't want to trash the
        // existing save file we are overwriting.
        std::stringstream stream;

        ESM::ESMWriter writer;

        for (const std::string& contentFile : MWBase::Environment::get().getWorld()->getContentFiles())
            writer.addMaster(contentFile, 0); // not using the size information anyway -> use value of 0

        writer.setFormatVersion(ESM::CurrentSaveGameFormatVersion);

        // all unused
        writer.setVersion(0);
        writer.setType(0);
        writer.setAuthor("");
        writer.setDescription("");

        size_t recordCount = 1 // saved game header
            + MWBase::Environment::get().getJournal()->countSavedGameRecords()
            + MWBase::Environment::get().getLuaManager()->countSavedGameRecords()
            + MWBase::Environment::get().getWorld()->countSavedGameRecords()
            + MWBase::Environment::get().getScriptManager()->getGlobalScripts().countSavedGameRecords()
            + MWBase::Environment::get().getDialogueManager()->countSavedGameRecords()
            + MWBase::Environment::get().getMechanicsManager()->countSavedGameRecords()
            + MWBase::Environment::get().getInputManager()->countSavedGameRecords()
            + MWBase::Environment::get().getWindowManager()->countSavedGameRecords()
            + 1; // the controller menus' record
        writer.setRecordCount(static_cast<int>(recordCount));

        writer.save(stream);

        Loading::Listener& listener = *MWBase::Environment::get().getWindowManager()->getLoadingScreen();
        // Using only Cells for progress information, since they typically have the largest records by far
        listener.setProgressRange(MWBase::Environment::get().getWorld()->countSavedGameCells());
        listener.setLabel("#{OMWEngine:SavingInProgress}", true);

        Loading::ScopedLoad load(&listener);

        writer.startRecord(ESM::REC_SAVE);
        slot->mProfile.save(writer);
        writer.endRecord(ESM::REC_SAVE);

        MWBase::Environment::get().getJournal()->write(writer, listener);
        MWBase::Environment::get().getDialogueManager()->write(writer, listener);
        // LuaManager::write should be called before World::write because world also saves
        // local scripts that depend on LuaManager.
        MWBase::Environment::get().getLuaManager()->write(writer, listener);
        MWBase::Environment::get().getWorld()->write(writer, listener);
        MWBase::Environment::get().getScriptManager()->getGlobalScripts().write(writer, listener);
        MWBase::Environment::get().getMechanicsManager()->write(writer, listener);
        MWBase::Environment::get().getInputManager()->write(writer, listener);
        MWBase::Environment::get().getWindowManager()->write(writer, listener);

        // What the controller menus remember goes in a record of its own, which other OpenMW versions skip. It comes
        // last, so the journal is loaded by the time it is read.
        ESM::UiState uiState;
        MWBase::Environment::get().getWorld()->getPlayer().writeUiState(uiState);
        MWBase::Environment::get().getJournal()->writeUiState(uiState);
        writer.startRecord(ESM::REC_UIST);
        uiState.save(writer);
        writer.endRecord(ESM::REC_UIST);

        // Ensure we have written the number of records that was estimated
        if (static_cast<size_t>(writer.getRecordCount()) != recordCount + 1) // 1 extra for TES3 record
            Log(Debug::Warning) << "Warning: number of written savegame records does not match. Estimated: "
                                << recordCount + 1 << ", written: " << writer.getRecordCount();

        writer.close();

        if (stream.fail())
            throw std::runtime_error(
                "Write operation failed (memory stream): " + std::generic_category().message(errno));

        // A save in OpenMW's folder may hold a game played there, so it is kept before being saved over. If the copy
        // fails, the save is left as it was.
        if (toOpenMW && std::filesystem::exists(slot->mPath))
            backUpOpenMWSave(slot->mPath);

        // All good, write to file
        std::ofstream filestream(slot->mPath, std::ios::binary);
        filestream << stream.rdbuf();

        if (filestream.fail())
            throw std::runtime_error("Write operation failed (file stream): " + std::generic_category().message(errno));

        writePinnedQuests(slot->mPath.parent_path(), uiState.mPinnedQuests);

        // Saving into OpenMW's folder leaves this game's own saves as the ones to continue from.
        if (!toOpenMW)
        {
            Settings::saves().mCharacter.set(Files::pathToUnicodeString(slot->mPath.parent_path().filename()));
            mLastSavegame = slot->mPath;
        }
        mPlayedSinceSave = 0;

        const auto finish = std::chrono::steady_clock::now();

        Log(Debug::Info) << '\'' << description << "' is saved in "
                         << std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(finish - start).count()
                         << "ms";
    }
    catch (const std::exception& e)
    {
        std::stringstream error;
        error << "Failed to save game: " << e.what();

        Log(Debug::Error) << error.str();

        std::vector<std::string> buttons;
        buttons.emplace_back("#{Interface:OK}");
        MWBase::Environment::get().getWindowManager()->interactiveMessageBox(error.str(), buttons);

        // If no file was written, clean up the slot
        if (character && slot && !std::filesystem::exists(slot->mPath))
        {
            character->deleteSlot(slot);
            character->cleanup();
        }
    }
}

void MWState::StateManager::quickSave(std::string name)
{
    if (!(mState == State_Running
            && MWBase::Environment::get().getWorld()->getGlobalInt(MWWorld::Globals::sCharGenState) == -1 // char gen
            && MWBase::Environment::get().getWindowManager()->isSavingAllowed()))
    {
        // You can not save your game right now
        MWBase::Environment::get().getWindowManager()->messageBox("#{OMWEngine:SaveGameDenied}");
        return;
    }

    if (Settings::saves().mQuicksaveHistory)
    {
        // Each quicksave is a new save, and older ones thin out once it is written.
        const std::size_t count = findSaves(name).size();
        saveGame(name, nullptr);
        if (findSaves(name).size() <= count)
            return;
        std::vector<std::filesystem::path> paths;
        for (const Slot* slot : findQuickSavesToDelete(findSaves(name)))
            paths.push_back(slot->mPath);
        // Deleting a save moves the others in memory, so each is found again by its file.
        for (const std::filesystem::path& path : paths)
        {
            for (const Slot* slot : findSaves(name))
            {
                if (slot->mPath == path)
                {
                    deleteGame(getCurrentCharacter(), slot);
                    break;
                }
            }
        }
        return;
    }

    Character* currentCharacter = getCurrentCharacter(); // Get current character
    QuickSaveManager saveFinder(name, Settings::saves().mMaxQuicksaves);

    if (currentCharacter)
    {
        for (auto& save : *currentCharacter)
        {
            // Visiting slots allows the quicksave finder to find the oldest quicksave
            saveFinder.visitSave(&save);
        }
    }

    // Once all the saves have been visited, the save finder can tell us which
    // one to replace (or create)
    saveGame(name, saveFinder.getNextQuickSaveSlot());
}

std::vector<const MWState::Slot*> MWState::StateManager::findSaves(std::string_view description)
{
    std::vector<const Slot*> saves;
    if (const Character* character = getCurrentCharacter())
    {
        for (const Slot& slot : *character)
        {
            if (slot.mProfile.mDescription == description)
                saves.push_back(&slot);
        }
    }
    return saves;
}

void MWState::StateManager::loadGame(const std::filesystem::path& filepath)
{
    for (const auto& character : mCharacterManager)
    {
        for (const auto& slot : character)
        {
            if (std::filesystem::equivalent(slot.mPath, filepath))
            {
                loadGame(&character, slot.mPath);
                return;
            }
        }
    }

    MWState::Character* character = getCurrentCharacter();
    loadGame(character, filepath);
}

struct SaveFormatVersionError : public std::exception
{
    using std::exception::exception;

    SaveFormatVersionError(ESM::FormatVersion savegameFormat, const std::string& message)
        : mSavegameFormat(savegameFormat)
        , mErrorMessage(message)
    {
    }

    const char* what() const noexcept override { return mErrorMessage.c_str(); }
    ESM::FormatVersion getFormatVersion() const { return mSavegameFormat; }

protected:
    ESM::FormatVersion mSavegameFormat = ESM::DefaultFormatVersion;
    std::string mErrorMessage;
};

struct SaveVersionTooOldError : SaveFormatVersionError
{
    SaveVersionTooOldError(ESM::FormatVersion savegameFormat)
        : SaveFormatVersionError(savegameFormat, "format version " + std::to_string(savegameFormat) + " is too old")
    {
    }
};

struct SaveVersionTooNewError : SaveFormatVersionError
{
    SaveVersionTooNewError(ESM::FormatVersion savegameFormat)
        : SaveFormatVersionError(savegameFormat, "format version " + std::to_string(savegameFormat) + " is too new")
    {
    }
};

void MWState::StateManager::loadGame(const Character* character, const std::filesystem::path& filepath)
{
    try
    {
        cleanup();

        // A game loaded from OpenMW's folder goes on in this game's character folder of the same name. If there is
        // none yet, the first save here creates it.
        std::filesystem::path openMWFolderName;
        const bool fromOpenMW = isOpenMWSave(filepath);
        if (fromOpenMW)
        {
            character = mCharacterManager.findCharacter(filepath.parent_path().filename());
            if (character == nullptr)
                openMWFolderName = filepath.parent_path().filename();
        }

        Log(Debug::Info) << "Reading save file " << filepath.filename();

        ESM::ESMReader reader;
        reader.open(filepath);

        // Formats 42 and 43 are this game's earlier saves. In OpenMW's folder, they would be from a newer OpenMW.
        ESM::FormatVersion version = reader.getFormatVersion();
        if (version > (fromOpenMW ? ESM::CurrentSaveGameFormatVersion : ESM::MaxModernMorrowSaveGameFormatVersion))
            throw SaveVersionTooNewError(version);
        else if (version < ESM::MinSupportedSaveGameFormatVersion)
            throw SaveVersionTooOldError(version);

        std::map<int, int> contentFileMap = buildContentFileIndexMap(reader);
        reader.setContentFileMapping(&contentFileMap);
        MWBase::Environment::get().getLuaManager()->setContentFileMapping(contentFileMap);

        ESM::ActorIdConverter actorIdConverter;
        if (version <= ESM::MaxActorIdSaveGameFormatVersion)
            reader.mActorIdConverter = &actorIdConverter;

        Loading::Listener& listener = *MWBase::Environment::get().getWindowManager()->getLoadingScreen();

        listener.setProgressRange(100);
        listener.setLabel("#{OMWEngine:LoadingInProgress}");

        Loading::ScopedLoad load(&listener);

        bool firstPersonCam = false;

        size_t total = reader.getFileSize();
        int currentPercent = 0;
        std::optional<ESM::UiState> uiState;
        while (reader.hasMoreRecs())
        {
            ESM::NAME n = reader.getRecName();
            reader.getRecHeader();

            switch (n.toInt())
            {
                case ESM::REC_SAVE:
                {
                    ESM::SavedGame profile;
                    profile.load(reader);
                    const auto& selectedContentFiles = MWBase::Environment::get().getWorld()->getContentFiles();
                    auto missingFiles = profile.getMissingContentFiles(selectedContentFiles);
                    if (!missingFiles.empty() && !confirmLoading(missingFiles))
                    {
                        cleanup(true);
                        MWBase::Environment::get().getWindowManager()->pushGuiMode(MWGui::GM_MainMenu);
                        return;
                    }
                    mTimePlayed = profile.mTimePlayed;
                    Log(Debug::Info) << "Loading saved game '" << profile.mDescription << "' for character '"
                                     << profile.mPlayerName << "'";
                }
                break;

                case ESM::REC_JOUR:
                case ESM::REC_QUES:

                    MWBase::Environment::get().getJournal()->readRecord(reader, n.toInt());
                    break;

                case ESM::REC_DIAS:

                    MWBase::Environment::get().getDialogueManager()->readRecord(reader, n.toInt());
                    break;

                case ESM::REC_ALCH:
                case ESM::REC_MISC:
                case ESM::REC_ACTI:
                case ESM::REC_ARMO:
                case ESM::REC_BOOK:
                case ESM::REC_CLAS:
                case ESM::REC_CLOT:
                case ESM::REC_ENCH:
                case ESM::REC_NPC_:
                case ESM::REC_SPEL:
                case ESM::REC_WEAP:
                case ESM::REC_GLOB:
                case ESM::REC_PLAY:
                case ESM::REC_CSTA:
                case ESM::REC_WTHR:
                case ESM::REC_DYNA:
                case ESM::REC_ACTC:
                case ESM::REC_PROJ:
                case ESM::REC_MPRJ:
                case ESM::REC_ENAB:
                case ESM::REC_LEVC:
                case ESM::REC_LEVI:
                case ESM::REC_LIGH:
                case ESM::REC_CREA:
                case ESM::REC_CONT:
                case ESM::REC_RAND:
                case ESM::REC_STAT:
                case ESM::REC_DOOR:
                case ESM::REC_PROB:
                case ESM::REC_INGR:
                    MWBase::Environment::get().getWorld()->readRecord(reader, n.toInt());
                    break;

                case ESM::REC_CAM_:
                    reader.getHNT(firstPersonCam, "FIRS");
                    break;

                case ESM::REC_GSCR:

                    MWBase::Environment::get().getScriptManager()->getGlobalScripts().readRecord(reader, n.toInt());
                    break;

                case ESM::REC_GMAP:
                case ESM::REC_KEYS:
                case ESM::REC_ASPL:
                case ESM::REC_MARK:

                    MWBase::Environment::get().getWindowManager()->readRecord(reader, n.toInt());
                    break;

                case ESM::REC_DCOU:
                case ESM::REC_STLN:

                    MWBase::Environment::get().getMechanicsManager()->readRecord(reader, n.toInt());
                    break;

                case ESM::REC_INPU:
                    MWBase::Environment::get().getInputManager()->readRecord(reader, n.toInt());
                    break;

                case ESM::REC_LUAM:
                    MWBase::Environment::get().getLuaManager()->readRecord(reader, n.toInt());
                    break;

                case ESM::REC_UIST:
                    uiState.emplace();
                    uiState->load(reader);
                    break;

                default:

                    // ignore invalid records
                    Log(Debug::Warning) << "Warning: Ignoring unknown record: " << n.toStringView();
                    reader.skipRecord();
            }
            int progressPercent = static_cast<int>(float(reader.getFileOffset()) / total * 100);
            if (progressPercent > currentPercent)
            {
                listener.increaseProgress(progressPercent - currentPercent);
                currentPercent = progressPercent;
            }
        }

        // A save that another OpenMW version wrote has no record of the controller menus, so its pins come from the
        // character's file beside the saves. Saves at formats 42 and 43 have the menus' data in their other records.
        if (!uiState && version <= ESM::CurrentSaveGameFormatVersion)
        {
            uiState.emplace();
            uiState->mPinnedQuests = readPinnedQuests(filepath.parent_path());
        }
        if (uiState)
        {
            MWBase::Environment::get().getWorld()->getPlayer().readUiState(*uiState);
            MWBase::Environment::get().getJournal()->readUiState(*uiState);
        }

        mCharacterManager.setCurrentCharacter(character);
        mOpenMWFolderName = openMWFolderName;

        mState = State_Running;

        if (character)
            Settings::saves().mCharacter.set(Files::pathToUnicodeString(character->getPath().filename()));
        mLastSavegame = filepath;

        MWBase::Environment::get().getWindowManager()->setNewGame(false);
        MWBase::Environment::get().getWorld()->saveLoaded(reader);
        actorIdConverter.apply();
        MWBase::Environment::get().getWorld()->setupPlayer();
        MWBase::Environment::get().getWorld()->renderPlayer();
        MWBase::Environment::get().getWindowManager()->updatePlayer();
        MWBase::Environment::get().getMechanicsManager()->playerLoaded();
        MWBase::Environment::get().getWorld()->toggleVanityMode(false);

        if (firstPersonCam != MWBase::Environment::get().getWorld()->isFirstPerson())
            MWBase::Environment::get().getWorld()->togglePOV();

        MWWorld::ConstPtr ptr = MWMechanics::getPlayer();

        if (ptr.isInCell())
        {
            const ESM::RefId cellId = ptr.getCell()->getCell()->getId();

            // Use detectWorldSpaceChange=false, otherwise some of the data we just loaded would be cleared again
            MWBase::Environment::get().getWorld()->changeToCell(cellId, ptr.getRefData().getPosition(), false, false);
        }
        else
        {
            // Cell no longer exists (i.e. changed game files), choose a default cell
            Log(Debug::Warning) << "Player character's cell no longer exists, changing to the default cell";
            ESM::ExteriorCellLocation cellIndex(0, 0, ESM::Cell::sDefaultWorldspaceId);
            MWWorld::CellStore& cell = MWBase::Environment::get().getWorldModel()->getExterior(cellIndex);
            const osg::Vec2f posFromIndex = ESM::indexToPosition(cellIndex, false);
            ESM::Position pos;
            pos.pos[0] = posFromIndex.x();
            pos.pos[1] = posFromIndex.y();
            pos.pos[2] = 0; // should be adjusted automatically (adjustPlayerPos=true)
            pos.rot[0] = 0;
            pos.rot[1] = 0;
            pos.rot[2] = 0;
            MWBase::Environment::get().getWorld()->changeToCell(cell.getCell()->getId(), pos, true, false);
        }

        MWBase::Environment::get().getWorld()->updateProjectilesCasters();

        // Vanilla MW will restart startup scripts when a save game is loaded. This is unintuitive,
        // but some mods may be using it as a reload detector.
        MWBase::Environment::get().getScriptManager()->getGlobalScripts().addStartup();

        // Since we passed "changeEvent=false" to changeCell, we shouldn't have triggered the cell change flag.
        // But make sure the flag is cleared anyway in case it was set from an earlier game.
        MWBase::Environment::get().getWorldScene()->markCellAsUnchanged();

        MWBase::Environment::get().getLuaManager()->gameLoaded();
        for (int actorId : actorIdConverter.mGraveyard)
        {
            auto mapped = actorIdConverter.mMappings.find(actorId);
            if (mapped != actorIdConverter.mMappings.end())
                MWBase::Environment::get().getMechanicsManager()->cleanupSummonedCreature(mapped->second);
        }
    }
    catch (const SaveVersionTooNewError& e)
    {
        std::string error = "#{OMWEngine:LoadingRequiresNewVersionError}";
        printSavegameFormatError(e.what(), error);
    }
    catch (const SaveVersionTooOldError& e)
    {
        const char* release;
        // Report the last version still capable of reading this save
        if (e.getFormatVersion() < ESM::OpenMW0_49MinSaveGameFormatVersion)
            release = "OpenMW 0.48.0";
        else
        {
            // Insert additional else if statements above to cover future releases
            static_assert(ESM::MinSupportedSaveGameFormatVersion <= ESM::OpenMW0_49MinSaveGameFormatVersion);
            release = "OpenMW 0.53.0";
        }
        auto l10n = MWBase::Environment::get().getL10nManager()->getContext("OMWEngine");
        std::string error = l10n->formatMessage("LoadingRequiresOldVersionError", { "version" }, { release });
        printSavegameFormatError(e.what(), error);
    }
    catch (const std::exception& e)
    {
        std::string error = "#{OMWEngine:LoadingFailed}: " + std::string(e.what());
        printSavegameFormatError(e.what(), error);
    }
}

void MWState::StateManager::printSavegameFormatError(
    const std::string& exceptionText, const std::string& messageBoxText)
{
    Log(Debug::Error) << "Failed to load saved game: " << exceptionText;

    cleanup(true);

    MWBase::Environment::get().getWindowManager()->pushGuiMode(MWGui::GM_MainMenu);

    std::vector<std::string> buttons;
    buttons.emplace_back("#{Interface:OK}");

    MWBase::Environment::get().getWindowManager()->interactiveMessageBox(messageBoxText, buttons);
}

void MWState::StateManager::quickLoad()
{
    if (Character* currentCharacter = getCurrentCharacter())
    {
        if (currentCharacter->begin() == currentCharacter->end())
            return;
        // use requestLoad, otherwise we can crash by loading during the wrong part of the frame
        requestLoad(currentCharacter, currentCharacter->begin()->mPath);
    }
}

void MWState::StateManager::deleteGame(const MWState::Character* character, const MWState::Slot* slot)
{
    const std::filesystem::path savePath = slot->mPath;
    mCharacterManager.deleteSlot(slot, character);
    if (mLastSavegame == savePath)
    {
        if (character != nullptr)
            mLastSavegame = character->begin()->mPath;
        else
            mLastSavegame.clear();
    }
}

MWState::Character* MWState::StateManager::getCurrentCharacter()
{
    return mCharacterManager.getCurrentCharacter();
}

MWState::StateManager::CharacterIterator MWState::StateManager::characterBegin()
{
    return mCharacterManager.begin();
}

MWState::StateManager::CharacterIterator MWState::StateManager::characterEnd()
{
    return mCharacterManager.end();
}

namespace
{
    // The characters of OpenMW's folder when it has none.
    const std::list<MWState::Character> sNoCharacters;
}

MWState::StateManager::CharacterIterator MWState::StateManager::characterBegin(SaveFolder folder)
{
    if (folder == SaveFolder::Own)
        return mCharacterManager.begin();
    return mOpenMWCharacters ? mOpenMWCharacters->begin() : sNoCharacters.begin();
}

MWState::StateManager::CharacterIterator MWState::StateManager::characterEnd(SaveFolder folder)
{
    if (folder == SaveFolder::Own)
        return mCharacterManager.end();
    return mOpenMWCharacters ? mOpenMWCharacters->end() : sNoCharacters.end();
}

void MWState::StateManager::update(float duration)
{
    mTimePlayed += duration;

    // Note: It would be nicer to trigger this from InputManager, i.e. the very beginning of the frame update.
    if (mAskLoadRecent)
    {
        int iButton = MWBase::Environment::get().getWindowManager()->readPressedButton();
        MWState::Character* curCharacter = getCurrentCharacter();
        if (iButton == 0 && curCharacter)
        {
            mAskLoadRecent = false;
            // Load last saved game for current character
            // loadGame resets the game state along with mLastSavegame so we want to preserve it
            const std::filesystem::path filePath = std::move(mLastSavegame);
            loadGame(curCharacter, filePath);
        }
        else if (iButton == 1)
        {
            mAskLoadRecent = false;
            MWBase::Environment::get().getWindowManager()->pushGuiMode(MWGui::GM_MainMenu);
        }
    }

    if (mNewGameRequest.has_value())
    {
        MWBase::Environment::get().getWindowManager()->removeGuiMode(MWGui::GM_MainMenu);
        newGame(mNewGameRequest->mBypass);
        mNewGameRequest = std::nullopt;
    }

    if (mLoadRequest)
    {
        MWBase::Environment::get().getWindowManager()->removeGuiMode(MWGui::GM_MainMenu);
        const Character* character = mLoadRequest->first;
        // The character may have been deleted after the request was made
        const bool validCharacter = std::ranges::find_if(mCharacterManager, [=](const Character& c) {
            return &c == character;
        }) != mCharacterManager.end();
        if (!validCharacter)
            character = getCurrentCharacter();
        loadGame(character, mLoadRequest->second);
        mLoadRequest = std::nullopt;
    }

    updateTimedAutosave(duration);
}

void MWState::StateManager::updateTimedAutosave(float duration)
{
    const int interval = Settings::saves().mAutosaveInterval;
    MWBase::WindowManager& windowManager = *MWBase::Environment::get().getWindowManager();
    if (mState != State_Running || interval <= 0 || windowManager.isGuiMode())
        return;
    mPlayedSinceSave += duration;
    if (mPlayedSinceSave < interval * 60.0)
        return;

    // The same checks as quicksave, without its message when saving isn't allowed, and none during combat.
    MWBase::World& world = *MWBase::Environment::get().getWorld();
    const MWWorld::Ptr player = world.getPlayerPtr();
    if (world.getGlobalInt(MWWorld::Globals::sCharGenState) != -1 || !windowManager.isSavingAllowed()
        || player.getClass().getCreatureStats(player).isDead() || world.getPlayer().isInCombat())
        return;

    // If the save fails, it waits for another interval rather than trying again on the next frame.
    mPlayedSinceSave = 0;
    quickSave("Autosave");
}

bool MWState::StateManager::confirmLoading(const std::vector<std::string_view>& missingFiles) const
{
    std::ostringstream stream;
    for (auto& contentFile : missingFiles)
    {
        Log(Debug::Warning) << "Warning: Saved game dependency " << contentFile << " is missing.";
        stream << contentFile << "\n";
    }

    auto fullList = stream.str();
    if (!fullList.empty())
        fullList.pop_back();

    constexpr size_t missingPluginsDisplayLimit = 12;

    std::vector<std::string> buttons;
    buttons.emplace_back("#{Interface:Yes}");
    buttons.emplace_back("#{Interface:Copy}");
    buttons.emplace_back("#{Interface:No}");
    std::string message = "#{OMWEngine:MissingContentFilesConfirmation}";

    auto l10n = MWBase::Environment::get().getL10nManager()->getContext("OMWEngine");
    message += l10n->formatMessage("MissingContentFilesList", { "files" }, { static_cast<int>(missingFiles.size()) });
    auto cappedSize = std::min(missingFiles.size(), missingPluginsDisplayLimit);
    if (cappedSize == missingFiles.size())
    {
        message += fullList;
    }
    else
    {
        for (size_t i = 0; i < cappedSize - 1; ++i)
        {
            message += missingFiles[i];
            message += "\n";
        }

        message += "...";
    }

    message
        += l10n->formatMessage("MissingContentFilesListCopy", { "files" }, { static_cast<int>(missingFiles.size()) });

    int selectedButton = -1;
    while (true)
    {
        auto windowManager = MWBase::Environment::get().getWindowManager();
        windowManager->interactiveMessageBox(message, buttons, true, selectedButton);
        selectedButton = windowManager->readPressedButton();
        if (selectedButton == 0)
            break;

        if (selectedButton == 1)
        {
            SDL_SetClipboardText(fullList.c_str());
            continue;
        }

        return false;
    }

    return true;
}

void MWState::StateManager::writeScreenshot(std::vector<char>& imageData) const
{
    int screenshotW = 259 * 2, screenshotH = 133 * 2; // *2 to get some nice antialiasing

    imageData = MWBase::Environment::get().getWorld()->screenshot(screenshotW, screenshotH);
}
