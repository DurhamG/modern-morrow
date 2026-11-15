#include "touchtutorial.hpp"

#include <cstdlib>
#include <fstream>

#include <components/debug/debuglog.hpp>

namespace MWInput
{
    TutorialRecord readTutorialRecord(const std::filesystem::path& file)
    {
        TutorialRecord record;
        std::ifstream stream(file);
        std::string line;
        while (std::getline(stream, line))
        {
            if (line.starts_with("played "))
                record.mLastPlayed = std::atoll(line.c_str() + 7);
            else if (line.starts_with("seen "))
                record.mCharacters.insert(line.substr(5));
        }
        return record;
    }

    void writeTutorialRecord(const std::filesystem::path& file, const TutorialRecord& record)
    {
        std::ofstream stream(file);
        stream << "played " << record.mLastPlayed << '\n';
        for (const std::string& character : record.mCharacters)
            stream << "seen " << character << '\n';
        if (!stream)
            Log(Debug::Warning) << "Failed to write the touch tutorial's record to " << file;
    }

    bool tutorialDue(const TutorialRecord& record, std::string_view character, std::int64_t now)
    {
        const bool away = record.mLastPlayed > 0 && now - record.mLastPlayed >= sTutorialAgain.count();
        return character.empty() || !record.mCharacters.contains(std::string(character)) || away;
    }
}
