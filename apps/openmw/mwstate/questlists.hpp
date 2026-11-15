#ifndef GAME_STATE_QUESTLISTS_H
#define GAME_STATE_QUESTLISTS_H

#include <filesystem>
#include <string_view>
#include <vector>

#include <components/esm/refid.hpp>

namespace MWState
{
    // The quests pinned, or archived, in the character's latest save from this game, one quest ID to a line. OpenMW
    // versions without the controller menus drop the save's record of them, so a save they wrote takes them from
    // here. The files sit in the folder of all the characters, named after the character's folder, such as
    // "Lendias.pinned-quests.txt" and "Lendias.archived-quests.txt". A character's folder holds only saves, and the
    // save list reads every file in it.
    constexpr std::string_view sPinnedQuests = "pinned-quests";
    constexpr std::string_view sArchivedQuests = "archived-quests";

    std::filesystem::path questListPath(const std::filesystem::path& characterDir, std::string_view list);

    /// Writes the list beside the character's folder. An empty list removes the file instead.
    void writeQuestList(
        const std::filesystem::path& characterDir, std::string_view list, const std::vector<ESM::RefId>& quests);

    /// Reads the list beside the character's folder. A missing file reads as an empty list.
    std::vector<ESM::RefId> readQuestList(const std::filesystem::path& characterDir, std::string_view list);
}

#endif
