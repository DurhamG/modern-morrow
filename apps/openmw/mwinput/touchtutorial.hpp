#ifndef MWINPUT_TOUCHTUTORIAL_H
#define MWINPUT_TOUCHTUTORIAL_H

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>

namespace MWInput
{
    /// The tutorial shows again for a character after this long without playing with touch.
    constexpr std::chrono::seconds sTutorialAgain = std::chrono::hours(24 * 14);

    /// The touch tutorial's record is a line "played <seconds since 1970>", for when the game was last played with
    /// touch, and a line "seen <folder>" for each character, by its folder of saves, that has seen the tutorial.
    struct TutorialRecord
    {
        std::int64_t mLastPlayed = 0;
        std::set<std::string> mCharacters;
    };

    /// Reads the record. A missing file reads as a game never played with touch.
    TutorialRecord readTutorialRecord(const std::filesystem::path& file);

    void writeTutorialRecord(const std::filesystem::path& file, const TutorialRecord& record);

    /// Whether the tutorial shows when play with touch starts, at a time in seconds since 1970. It shows for a new
    /// game's character, which has no folder yet, for a character that hasn't seen it, and for any character after
    /// two weeks away.
    bool tutorialDue(const TutorialRecord& record, std::string_view character, std::int64_t now);
}

#endif
