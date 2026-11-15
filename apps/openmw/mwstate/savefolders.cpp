#include "savefolders.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <system_error>

#include <components/debug/debuglog.hpp>
#include <components/files/conversion.hpp>
#include <components/files/fixedpath.hpp>
#include <components/misc/timeconvert.hpp>

namespace MWState
{
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

    std::vector<OpenMWInstall> openMWInstalls()
    {
        std::vector<OpenMWInstall> installs;
#if defined(__linux__)
        if (const char* home = std::getenv("HOME"))
        {
            const std::filesystem::path flatpak = std::filesystem::path(home) / ".var/app/org.openmw.OpenMW";
            installs.push_back({ flatpak / "config/openmw", flatpak / "data/openmw/saves" });
        }
#endif
        const Files::FixedPath<> openmw("openmw");
        installs.push_back({ openmw.getUserConfigPath(), openmw.getUserDataPath() / "saves" });
        return installs;
    }

    std::filesystem::path chooseOpenMWSaves(const std::string& setting, const std::vector<OpenMWInstall>& installs,
        const std::filesystem::path& ownSaves)
    {
        if (!setting.empty())
            return Files::pathFromUnicodeString(setting);

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

    bool isSaveInFolder(const std::filesystem::path& save, const std::filesystem::path& saves)
    {
        std::error_code ec;
        return !saves.empty() && std::filesystem::equivalent(save.parent_path().parent_path(), saves, ec);
    }

    std::filesystem::path backUpSave(
        const std::filesystem::path& save, const std::filesystem::path& backups, std::filesystem::file_time_type now)
    {
        const std::filesystem::path folder = backups / save.parent_path().filename();
        std::filesystem::create_directories(folder);
        std::filesystem::path name = save.stem();
        name += Misc::fileTimeToString(now, " %Y-%m-%d %H-%M-%S");
        name += save.extension();
        std::filesystem::copy_file(save, folder / name, std::filesystem::copy_options::overwrite_existing);
        Log(Debug::Info) << "Copied " << save << " to " << folder / name << " before saving over it";
        return folder / name;
    }

    bool isSaveFormatTooNew(ESM::FormatVersion version, bool fromOpenMW)
    {
        return version > (fromOpenMW ? ESM::CurrentSaveGameFormatVersion : ESM::MaxModernMorrowSaveGameFormatVersion);
    }

    bool usesQuestListFiles(bool hasUiState, ESM::FormatVersion version)
    {
        return !hasUiState && version <= ESM::CurrentSaveGameFormatVersion;
    }

    void writeSaveFile(
        std::istream& save, const std::filesystem::path& path, const std::optional<std::filesystem::path>& backups)
    {
        if (backups && std::filesystem::exists(path))
            backUpSave(path, *backups, std::filesystem::file_time_type::clock::now());

        std::ofstream filestream(path, std::ios::binary);
        filestream << save.rdbuf();

        if (filestream.fail())
            throw std::runtime_error("Write operation failed (file stream): " + std::generic_category().message(errno));
    }
}
