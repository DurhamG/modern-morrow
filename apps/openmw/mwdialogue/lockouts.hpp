#ifndef OPENMW_MWDIALOGUE_LOCKOUTS_H
#define OPENMW_MWDIALOGUE_LOCKOUTS_H

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <components/esm/refid.hpp>

namespace MWDialogue::Lockouts
{
    /// What a dialogue line's result script does that can close off other content: the factions it makes the player
    /// join, and the quest stages it sets.
    struct Effects
    {
        /// The factions the player joins. An empty ID is the speaker's own faction, for PCJoinFaction with no
        /// argument.
        std::vector<ESM::RefId> mJoins;
        /// The quests and stages set with Journal or SetJournalIndex.
        std::vector<std::pair<ESM::RefId, int>> mStages;
    };

    Effects readEffects(std::string_view script);

    /// Factions of which the player can only join one, such as the three Great Houses.
    struct FactionGroup
    {
        std::vector<ESM::RefId> mFactions;
        /// The key of the warning in the Interface localisation context.
        std::string mWarning;
    };

    /// A quest stage that closes off other content, such as an order to kill another guild's leaders.
    struct QuestStage
    {
        ESM::RefId mQuest;
        int mStage = 0;
        std::string mWarning;
    };

    /// The choices to warn about, read from lockouts.yaml.
    struct Data
    {
        std::vector<FactionGroup> mFactionGroups;
        std::vector<QuestStage> mQuestStages;
    };

    /// Reads the data from YAML. Entries that are incomplete are left out. Throws on YAML that can't be parsed.
    Data parse(std::string_view yaml);

    /// The warning for a dialogue line with these effects, said by someone of speakerFaction, or nothing if the line
    /// closes off nothing. Joining a faction warns when the player is in none of its group yet. Setting a quest stage
    /// warns when the quest hasn't reached that stage yet.
    std::optional<std::string> warning(const Data& data, const Effects& effects, const ESM::RefId& speakerFaction,
        const std::function<bool(const ESM::RefId&)>& isMember,
        const std::function<int(const ESM::RefId&)>& journalIndex);
}

#endif
