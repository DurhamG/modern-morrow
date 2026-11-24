#ifndef OPENMW_MWAUTOMATION_QUESTSHEETS_H
#define OPENMW_MWAUTOMATION_QUESTSHEETS_H

#include <filesystem>
#include <string>
#include <vector>

namespace MWAutomation
{
    /// Writes a review sheet for each quest whose ID starts with one of the prefixes, or for every quest without
    /// any, as <dir>/<quest>.md. A sheet has the quest's journal entries, what the quest markers show at each stage,
    /// the ways forward the analysis found, the lines and scripts that move the quest on, and a review record of what
    /// the markers show now, for the reviewer to judge and edit. The sheets hold the game's text, so they belong in a
    /// folder git ignores. Returns how many sheets were written.
    std::size_t writeQuestSheets(const std::filesystem::path& dir, const std::vector<std::string>& prefixes);
}

#endif
