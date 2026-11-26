#include "lockouts.hpp"

#include <algorithm>
#include <charconv>

#include <yaml-cpp/yaml.h>

#include <components/misc/strings/algorithm.hpp>

namespace MWDialogue::Lockouts
{
    namespace
    {
        // The words of a script line, split at spaces and commas, with quoted words kept whole and their quotes
        // removed. A semicolon outside quotes starts a comment.
        std::vector<std::string> words(std::string_view line)
        {
            std::vector<std::string> result;
            std::string word;
            bool quoted = false;
            bool inWord = false;
            for (const char c : line)
            {
                if (c == '"')
                {
                    quoted = !quoted;
                    inWord = true;
                    continue;
                }
                if (!quoted && c == ';')
                    break;
                if (!quoted && (c == ' ' || c == '\t' || c == ','))
                {
                    if (inWord)
                        result.push_back(std::move(word));
                    word.clear();
                    inWord = false;
                    continue;
                }
                word += c;
                inWord = true;
            }
            if (inWord)
                result.push_back(std::move(word));
            return result;
        }

        std::optional<int> number(std::string_view text)
        {
            int value = 0;
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            if (error != std::errc() || end != text.data() + text.size())
                return std::nullopt;
            return value;
        }
    }

    Effects readEffects(std::string_view script)
    {
        Effects effects;
        std::size_t start = 0;
        while (start <= script.size())
        {
            std::size_t end = script.find_first_of("\r\n", start);
            if (end == std::string_view::npos)
                end = script.size();
            std::string_view line = script.substr(start, end - start);
            start = end + 1;

            // A command on an object, such as player->PCJoinFaction, is read as the command.
            if (const std::size_t arrow = line.find("->"); arrow != std::string_view::npos)
                line = line.substr(arrow + 2);
            const std::vector<std::string> parts = words(line);
            if (parts.empty())
                continue;
            if (Misc::StringUtils::ciEqual(parts[0], "PCJoinFaction"))
                effects.mJoins.push_back(parts.size() > 1 ? ESM::RefId::stringRefId(parts[1]) : ESM::RefId());
            else if ((Misc::StringUtils::ciEqual(parts[0], "Journal")
                         || Misc::StringUtils::ciEqual(parts[0], "SetJournalIndex"))
                && parts.size() > 2)
            {
                if (const std::optional<int> stage = number(parts[2]))
                    effects.mStages.emplace_back(ESM::RefId::stringRefId(parts[1]), *stage);
            }
        }
        return effects;
    }

    Data parse(std::string_view yaml)
    {
        Data data;
        const YAML::Node root = YAML::Load(std::string(yaml));
        for (const YAML::Node& node : root["factionGroups"])
        {
            FactionGroup group;
            for (const YAML::Node& faction : node["factions"])
                group.mFactions.push_back(ESM::RefId::stringRefId(faction.as<std::string>()));
            if (node["warning"])
                group.mWarning = node["warning"].as<std::string>();
            if (!group.mFactions.empty() && !group.mWarning.empty())
                data.mFactionGroups.push_back(std::move(group));
        }
        for (const YAML::Node& node : root["questStages"])
        {
            if (!node["quest"] || !node["stage"] || !node["warning"])
                continue;
            data.mQuestStages.push_back(QuestStage{ ESM::RefId::stringRefId(node["quest"].as<std::string>()),
                node["stage"].as<int>(), node["warning"].as<std::string>() });
        }
        return data;
    }

    std::optional<std::string> warning(const Data& data, const Effects& effects, const ESM::RefId& speakerFaction,
        const std::function<bool(const ESM::RefId&)>& isMember,
        const std::function<int(const ESM::RefId&)>& journalIndex)
    {
        for (const ESM::RefId& join : effects.mJoins)
        {
            const ESM::RefId& faction = join.empty() ? speakerFaction : join;
            for (const FactionGroup& group : data.mFactionGroups)
            {
                const auto& factions = group.mFactions;
                if (std::find(factions.begin(), factions.end(), faction) != factions.end()
                    && std::none_of(factions.begin(), factions.end(), isMember))
                    return group.mWarning;
            }
        }
        for (const auto& [quest, stage] : effects.mStages)
            for (const QuestStage& entry : data.mQuestStages)
                if (entry.mQuest == quest && entry.mStage == stage && journalIndex(quest) < stage)
                    return entry.mWarning;
        return std::nullopt;
    }
}
