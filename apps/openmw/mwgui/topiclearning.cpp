#include "topiclearning.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <variant>

#include <components/misc/strings/algorithm.hpp>
#include <components/misc/strings/lower.hpp>

namespace MWGui
{
    namespace
    {
        // The first three letters of a text, in lowercase, as one number.
        std::uint32_t startOf(std::string_view text)
        {
            std::uint32_t key = 0;
            for (std::size_t i = 0; i < 3; ++i)
                key = (key << 8) | static_cast<unsigned char>(Misc::StringUtils::toLower(text[i]));
            return key;
        }

        // The topics that AddTopic commands in a script or a line's result add, lowercase, such as
        // `AddTopic "latest rumors"` or `addtopic necromancers`.
        std::vector<std::string> addedTopics(std::string_view script)
        {
            constexpr std::string_view command = "addtopic";
            std::vector<std::string> topics;
            if (Misc::StringUtils::ciFind(script, command) == std::string_view::npos)
                return topics;
            const std::string text = Misc::StringUtils::lowerCase(script);
            for (std::size_t at = text.find(command); at != std::string::npos; at = text.find(command, at + 1))
            {
                if (at > 0 && std::string_view(" \t\r\n,>").find(text[at - 1]) == std::string_view::npos)
                    continue;
                std::size_t i = at + command.size();
                while (i < text.size() && (text[i] == ' ' || text[i] == '\t' || text[i] == ','))
                    ++i;
                std::string topic;
                if (i < text.size() && text[i] == '"')
                {
                    const std::size_t end = text.find('"', i + 1);
                    if (end == std::string::npos)
                        continue;
                    topic = text.substr(i + 1, end - i - 1);
                }
                else
                {
                    const std::size_t end = text.find_first_of(" \t\r\n;", i);
                    topic = text.substr(i, end == std::string::npos ? std::string::npos : end - i);
                }
                if (!topic.empty())
                    topics.push_back(std::move(topic));
            }
            return topics;
        }

        // The lowest stage of each quest that a line's conditions allow, from its conditions on quests' stages. Quests
        // the line allows at any stage are left out.
        std::vector<std::pair<ESM::RefId, int>> lowestStages(const std::vector<ESM::DialogueCondition>& conditions)
        {
            std::vector<std::pair<ESM::RefId, int>> result;
            for (const ESM::DialogueCondition& condition : conditions)
            {
                if (condition.mFunction != ESM::DialogueCondition::Function_Journal)
                    continue;
                const float value = std::visit([](auto v) { return static_cast<float>(v); }, condition.mValue);
                int lowest = 0;
                switch (condition.mComparison)
                {
                    case ESM::DialogueCondition::Comp_Eq:
                    case ESM::DialogueCondition::Comp_Ge:
                        lowest = static_cast<int>(std::ceil(value));
                        break;
                    case ESM::DialogueCondition::Comp_Gt:
                        lowest = static_cast<int>(std::floor(value)) + 1;
                        break;
                    default:
                        break;
                }
                if (lowest <= 0)
                    continue;
                const ESM::RefId quest = ESM::RefId::stringRefId(condition.mVariable);
                const auto known = std::find_if(
                    result.begin(), result.end(), [&](const auto& entry) { return entry.first == quest; });
                if (known == result.end())
                    result.emplace_back(quest, lowest);
                else
                    known->second = std::max(known->second, lowest);
            }
            return result;
        }
    }

    void TopicLearning::addTopic(std::string_view topic)
    {
        std::string name = Misc::StringUtils::lowerCase(topic);
        if (name.empty() || mTopics.contains(name))
            return;
        Topic& added = mTopics[name];
        if (name.size() < 3)
        {
            mShortTopics.emplace_back(name, &added);
            return;
        }
        const std::uint32_t start = startOf(name);
        mByStart[start].emplace_back(name, &added);
        mStarts[start] = true;
    }

    void TopicLearning::learn(const std::string& topic, const std::vector<ESM::DialogueCondition>* conditions)
    {
        const auto it = mTopics.find(topic);
        if (it != mTopics.end())
            learn(it->second, conditions);
    }

    void TopicLearning::learn(Topic& learned, const std::vector<ESM::DialogueCondition>* conditions)
    {
        if (learned.mAnyTime)
            return;
        if (conditions == nullptr || conditions->empty())
        {
            learned.mAnyTime = true;
            learned.mNeeds.clear();
            return;
        }
        ++learned.mLines;
        for (const auto& [quest, lowest] : lowestStages(*conditions))
        {
            Needs& needs = learned.mNeeds[quest];
            needs.mLowest = needs.mLines == 0 ? lowest : std::min(needs.mLowest, lowest);
            ++needs.mLines;
        }
    }

    void TopicLearning::addLine(
        std::string_view text, std::string_view result, const std::vector<ESM::DialogueCondition>& conditions)
    {
        // The game finds a topic's name where a word starts: at the start of the text, or after a space, a quote or an
        // opening bracket. It doesn't care where the name ends.
        static constexpr std::array<bool, 256> separators = [] {
            std::array<bool, 256> result{};
            for (const char c : std::string_view("\n\r \t'\"(["))
                result[static_cast<unsigned char>(c)] = true;
            return result;
        }();
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            if (i > 0 && !separators[static_cast<unsigned char>(text[i - 1])])
                continue;
            const std::string_view rest = text.substr(i);
            const auto look = [&](const std::vector<std::pair<std::string, Topic*>>& topics) {
                for (const auto& [name, topic] : topics)
                    if (!topic->mAnyTime && Misc::StringUtils::ciStartsWith(rest, name))
                        learn(*topic, &conditions);
            };
            look(mShortTopics);
            if (rest.size() < 3)
                continue;
            const std::uint32_t start = startOf(rest);
            if (mStarts[start])
                look(mByStart.find(start)->second);
        }
        for (const std::string& topic : addedTopics(result))
            learn(topic, &conditions);
    }

    void TopicLearning::addScript(std::string_view text)
    {
        for (const std::string& topic : addedTopics(text))
            learn(topic, nullptr);
    }

    int TopicLearning::learnedAt(std::string_view topic, const ESM::RefId& quest) const
    {
        const auto it = mTopics.find(Misc::StringUtils::lowerCase(topic));
        if (it == mTopics.end() || it->second.mAnyTime)
            return 0;
        // A line that allows every stage of the quest teaches the topic at any stage.
        const auto needs = it->second.mNeeds.find(quest);
        if (needs == it->second.mNeeds.end() || needs->second.mLines < it->second.mLines)
            return 0;
        return needs->second.mLowest;
    }
}
