#include "questcorrections.hpp"

#include <algorithm>
#include <charconv>
#include <optional>
#include <stdexcept>

#include <yaml-cpp/yaml.h>

namespace MWGui::QuestCorrections
{
    namespace
    {
        std::optional<int> readIndex(std::string_view text)
        {
            int value = 0;
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            if (error != std::errc() || end != text.data() + text.size() || value < 0)
                return std::nullopt;
            return value;
        }

        // A journal index, such as "10", or a range of them, such as "10-20".
        std::optional<Range> readRange(const YAML::Node& node)
        {
            if (!node.IsScalar())
                return std::nullopt;
            const std::string& text = node.Scalar();
            const std::size_t dash = text.find('-');
            if (dash == std::string::npos)
            {
                const std::optional<int> index = readIndex(text);
                if (!index)
                    return std::nullopt;
                return Range{ *index, *index };
            }
            const std::optional<int> first = readIndex(std::string_view(text).substr(0, dash));
            const std::optional<int> last = readIndex(std::string_view(text).substr(dash + 1));
            if (!first || !last || *first > *last)
                return std::nullopt;
            return Range{ *first, *last };
        }

        // An index or a range, or a list of them.
        std::optional<std::vector<Range>> readRanges(const YAML::Node& node)
        {
            std::vector<Range> result;
            if (node.IsSequence())
            {
                for (const YAML::Node& item : node)
                {
                    const std::optional<Range> range = readRange(item);
                    if (!range)
                        return std::nullopt;
                    result.push_back(*range);
                }
            }
            else
            {
                const std::optional<Range> range = readRange(node);
                if (!range)
                    return std::nullopt;
                result.push_back(*range);
            }
            if (result.empty())
                return std::nullopt;
            return result;
        }

        bool overlaps(const std::vector<Range>& l, const std::vector<Range>& r)
        {
            return std::any_of(l.begin(), l.end(), [&](const Range& a) {
                return std::any_of(
                    r.begin(), r.end(), [&](const Range& b) { return a.mFirst <= b.mLast && b.mFirst <= a.mLast; });
            });
        }

        // Reads one entry. Returns the problem with it, or nothing when it was read.
        std::optional<std::string> readEntry(const YAML::Node& node, Entry& entry)
        {
            if (!node.IsMap())
                return "isn't a map";
            for (const auto& field : node)
            {
                const std::string key = field.first.as<std::string>();
                std::vector<Range>* ranges = nullptr;
                if (key == "stages")
                    ranges = &entry.mStages;
                else if (key == "next")
                    ranges = &entry.mNext;
                else if (key == "hide")
                    ranges = &entry.mHide;
                else
                    return "has an unknown field \"" + key + "\"";
                std::optional<std::vector<Range>> read = readRanges(field.second);
                if (!read)
                    return "has a \"" + key + "\" that isn't a journal index, a range such as 10-20, or a list of them";
                *ranges = std::move(*read);
            }
            if (entry.mStages.empty())
                return "has no \"stages\"";
            return std::nullopt;
        }
    }

    bool contains(const std::vector<Range>& ranges, int index)
    {
        return std::any_of(ranges.begin(), ranges.end(), [&](const Range& range) { return range.contains(index); });
    }

    const Entry* Corrections::find(const ESM::RefId& quest, int stage) const
    {
        const auto it = mQuests.find(quest);
        if (it == mQuests.end())
            return nullptr;
        for (const Entry& entry : it->second)
            if (contains(entry.mStages, stage))
                return &entry;
        return nullptr;
    }

    Corrections parse(const std::string& text, std::vector<std::string>& problems)
    {
        Corrections corrections;
        YAML::Node root;
        try
        {
            root = YAML::Load(text);
        }
        catch (const YAML::Exception& e)
        {
            problems.push_back(std::string("the file isn't YAML: ") + e.what());
            return corrections;
        }
        if (!root.IsMap())
        {
            problems.emplace_back("the file isn't a map");
            return corrections;
        }
        const YAML::Node version = root["version"];
        if (!version || !version.IsScalar() || version.Scalar() != "1")
        {
            problems.emplace_back("the file isn't version 1");
            return corrections;
        }
        const YAML::Node quests = root["quests"];
        if (!quests)
            return corrections;
        if (!quests.IsMap())
        {
            problems.emplace_back("\"quests\" isn't a map");
            return corrections;
        }
        for (const auto& quest : quests)
        {
            const std::string name = quest.first.as<std::string>();
            if (!quest.second.IsSequence())
            {
                problems.push_back(name + " isn't a list of entries");
                continue;
            }
            std::vector<Entry> entries;
            std::size_t number = 0;
            for (const YAML::Node& node : quest.second)
            {
                ++number;
                Entry entry;
                std::optional<std::string> problem = readEntry(node, entry);
                if (!problem && std::any_of(entries.begin(), entries.end(), [&](const Entry& other) {
                        return overlaps(other.mStages, entry.mStages);
                    }))
                    problem = "has stages that an earlier entry has";
                if (problem)
                {
                    problems.push_back(name + "'s entry " + std::to_string(number) + " " + *problem);
                    continue;
                }
                entries.push_back(std::move(entry));
            }
            if (entries.empty())
                continue;
            std::vector<Entry>& known = corrections.mQuests[ESM::RefId::stringRefId(name)];
            if (!known.empty())
            {
                problems.push_back(name + " is in the file twice");
                continue;
            }
            known = std::move(entries);
        }
        return corrections;
    }
}
