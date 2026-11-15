#ifndef GAME_STATE_SAVEFOLDERS_H
#define GAME_STATE_SAVEFOLDERS_H

#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include <components/esm3/formatversion.hpp>

namespace MWState
{
    /// When a file in the saves folder was last written, or the oldest possible time when it has no saves.
    std::filesystem::file_time_type newestSave(const std::filesystem::path& saves);

    /// Where an OpenMW install keeps its settings and its saves. Its launcher and setup wizard make the config
    /// folder, and its first save makes the saves folder.
    struct OpenMWInstall
    {
        std::filesystem::path mConfig;
        std::filesystem::path mSaves;
    };

    /// The places an OpenMW install can be on this computer. On Linux, that is the Flatpak, which app stores such as
    /// the Steam Deck's Discover install, and then OpenMW's usual folder. Elsewhere, it is OpenMW's usual folder.
    std::vector<OpenMWInstall> openMWInstalls();

    /// Chooses the saves folder of an OpenMW install. The "openmw saves folder" setting can name it. Otherwise it is
    /// an install that exists, other than this game's own saves folder. When more than one exists, it is the one saved
    /// to most recently. Returns an empty path when there is none.
    std::filesystem::path chooseOpenMWSaves(const std::string& setting, const std::vector<OpenMWInstall>& installs,
        const std::filesystem::path& ownSaves);

    /// Whether a save is in a character's folder inside the saves folder.
    bool isSaveInFolder(const std::filesystem::path& save, const std::filesystem::path& saves);

    /// Copies a save into the character's folder under the backups folder, with the time in its name, and returns the
    /// copy's path. The backups go in this game's own folder, since OpenMW would list a file in its character folders
    /// as a save. Throws when the copy fails.
    std::filesystem::path backUpSave(const std::filesystem::path& save, const std::filesystem::path& backups,
        std::filesystem::file_time_type now);

    /// Whether a save is too new to load. Formats 42 and 43 are this game's earlier saves. In OpenMW's folder, they
    /// would be from a newer OpenMW.
    bool isSaveFormatTooNew(ESM::FormatVersion version, bool fromOpenMW);

    /// Whether a loaded save's pinned and archived quests come from the files beside the saves. A save that another
    /// OpenMW version wrote has no record of the controller menus. Saves at formats 42 and 43 have the menus' data in
    /// their other records.
    bool usesQuestListFiles(bool hasUiState, ESM::FormatVersion version);

    /// Writes a save that is already in memory to its file. With a backups folder, an existing file there is backed
    /// up first, and if the copy fails, the file is left as it was. Throws when the copy or the write fails.
    void writeSaveFile(
        std::istream& save, const std::filesystem::path& path, const std::optional<std::filesystem::path>& backups);
}

#endif
