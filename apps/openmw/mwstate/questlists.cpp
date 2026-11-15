#include "questlists.hpp"

#include <fstream>
#include <string>
#include <system_error>

#include <components/debug/debuglog.hpp>

namespace MWState
{
    std::filesystem::path questListPath(const std::filesystem::path& characterDir, std::string_view list)
    {
        std::filesystem::path name = characterDir.filename();
        name += "." + std::string(list) + ".txt";
        return characterDir.parent_path() / name;
    }

    void writeQuestList(
        const std::filesystem::path& characterDir, std::string_view list, const std::vector<ESM::RefId>& quests)
    {
        const std::filesystem::path path = questListPath(characterDir, list);
        // An empty list needs no file, which keeps the saves folder tidy.
        if (quests.empty())
        {
            std::error_code ec;
            std::filesystem::remove(path, ec);
            return;
        }
        std::ofstream file(path, std::ios::trunc);
        for (const ESM::RefId& quest : quests)
            file << quest.serializeText() << '\n';
        if (file.fail())
            Log(Debug::Warning) << "Failed to write the quest list " << path;
    }

    std::vector<ESM::RefId> readQuestList(const std::filesystem::path& characterDir, std::string_view list)
    {
        const std::filesystem::path path = questListPath(characterDir, list);
        std::vector<ESM::RefId> quests;
        std::ifstream file(path);
        std::string line;
        try
        {
            while (std::getline(file, line))
                if (!line.empty())
                    quests.push_back(ESM::RefId::deserializeText(line));
        }
        catch (const std::exception& e)
        {
            Log(Debug::Warning) << "Failed to read the quest list " << path << ": " << e.what();
        }
        return quests;
    }
}
