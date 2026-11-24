#include "questcorrections.hpp"

#include <algorithm>
#include <charconv>
#include <iterator>
#include <optional>
#include <stdexcept>

#include <yaml-cpp/yaml.h>

#include <components/debug/debuglog.hpp>
#include <components/esm3/loadcell.hpp>
#include <components/esm3/loaddial.hpp>
#include <components/esm3/loadglob.hpp>
#include <components/esm3/loadinfo.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/vfs/manager.hpp>
#include <components/vfs/pathutil.hpp>

#include "../mwworld/esmstore.hpp"

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

        std::optional<int> readNumber(const YAML::Node& node)
        {
            if (!node.IsScalar())
                return std::nullopt;
            return readIndex(node.Scalar());
        }

        bool isTrue(const YAML::Node& node)
        {
            return node.IsScalar() && node.Scalar() == "true";
        }

        // A target's condition: { quest: ID, atLeast: N }, { quest: ID, started: true },
        // { quest: ID, finished: true }, { dead: ID }, { carried: ID } with optionally atLeast: N, or
        // { global: NAME, atLeast: N }. Returns the problem with it, or nothing when it was read.
        std::optional<std::string> readCondition(const YAML::Node& node, const std::string& name, Condition& condition)
        {
            if (!node.IsMap())
                return "a \"" + name + "\" that isn't a map";
            int stages = 0;
            bool atLeast = false;
            for (const auto& field : node)
            {
                const std::string key = field.first.as<std::string>();
                const std::string value = field.second.IsScalar() ? field.second.Scalar() : std::string();
                if (key == "quest" && !value.empty())
                    condition.mQuest = ESM::RefId::stringRefId(value);
                else if (key == "atLeast" && readNumber(field.second))
                {
                    condition.mAtLeast = *readNumber(field.second);
                    atLeast = true;
                    ++stages;
                }
                else if (key == "global" && !value.empty())
                    condition.mGlobal = value;
                else if (key == "started" && isTrue(field.second))
                {
                    condition.mAtLeast = 1;
                    ++stages;
                }
                else if (key == "finished" && isTrue(field.second))
                {
                    condition.mFinished = true;
                    ++stages;
                }
                else if (key == "dead" && !value.empty())
                    condition.mDead = ESM::RefId::stringRefId(value);
                else if (key == "carried" && !value.empty())
                    condition.mCarried = ESM::RefId::stringRefId(value);
                else
                    return "a \"" + name + "\" with a wrong \"" + key + "\"";
            }
            const int kinds = (condition.mQuest.empty() ? 0 : 1) + (condition.mDead.empty() ? 0 : 1)
                + (condition.mCarried.empty() ? 0 : 1) + (condition.mGlobal.empty() ? 0 : 1);
            const bool stagesRight = !condition.mQuest.empty() ? stages == 1
                : !condition.mGlobal.empty()                   ? stages == 1 && atLeast
                : !condition.mCarried.empty()                  ? stages == 0 || (stages == 1 && atLeast)
                                                               : stages == 0;
            if (kinds != 1 || !stagesRight)
                return "a \"" + name
                    + "\" without exactly one of a quest with atLeast, started or finished, dead, carried, or a global "
                      "with atLeast";
            return std::nullopt;
        }

        // A target: { task: talk, object: ID } or { task: visit, cell: NAME }, with optionally a count and a done.
        // Returns the problem with it, or nothing when it was read.
        std::optional<std::string> readTarget(const YAML::Node& node, Target& target)
        {
            if (!node.IsMap())
                return "a target that isn't a map";
            bool hasTask = false;
            for (const auto& field : node)
            {
                const std::string key = field.first.as<std::string>();
                const std::string value = field.second.IsScalar() ? field.second.Scalar() : std::string();
                if (key == "task")
                {
                    hasTask = true;
                    if (value == "talk")
                        target.mTask = QuestTargets::Task::Talk;
                    else if (value == "kill")
                        target.mTask = QuestTargets::Task::Kill;
                    else if (value == "fetch")
                        target.mTask = QuestTargets::Task::Fetch;
                    else if (value == "visit")
                        target.mTask = QuestTargets::Task::Visit;
                    else
                        return "a target with an unknown task \"" + value + "\"";
                }
                else if (key == "object" && !value.empty())
                    target.mObject = ESM::RefId::stringRefId(value);
                else if (key == "cell" && !value.empty())
                    target.mCell = ESM::RefId::stringRefId(value);
                else if (key == "count" && readNumber(field.second) && *readNumber(field.second) > 0)
                    target.mCount = *readNumber(field.second);
                else if (key == "optional" && (isTrue(field.second) || value == "false"))
                    target.mOptional = isTrue(field.second);
                else if (key == "done")
                {
                    Condition condition;
                    if (std::optional<std::string> problem = readCondition(field.second, key, condition))
                        return problem;
                    target.mDone = std::move(condition);
                }
                else if (key == "after")
                {
                    // One condition, or a list of them that must all hold.
                    std::vector<YAML::Node> nodes;
                    if (field.second.IsSequence())
                        for (const YAML::Node& item : field.second)
                            nodes.push_back(item);
                    else
                        nodes.push_back(field.second);
                    for (const YAML::Node& item : nodes)
                    {
                        Condition condition;
                        if (std::optional<std::string> problem = readCondition(item, key, condition))
                            return problem;
                        target.mAfter.push_back(std::move(condition));
                    }
                }
                else
                    return "a target with a wrong \"" + key + "\"";
            }
            if (!hasTask)
                return "a target without a task";
            if (target.mObject.empty() == target.mCell.empty())
                return "a target without exactly one of object or cell";
            return std::nullopt;
        }

        // Reads one entry. Returns the problem with it, or nothing when it was read.
        std::optional<std::string> readEntry(const YAML::Node& node, Entry& entry)
        {
            if (!node.IsMap())
                return "isn't a map";
            for (const auto& field : node)
            {
                const std::string key = field.first.as<std::string>();
                if (key == "targets")
                {
                    if (!field.second.IsSequence())
                        return "has \"targets\" that aren't a list";
                    for (const YAML::Node& item : field.second)
                    {
                        Target target;
                        if (std::optional<std::string> problem = readTarget(item, target))
                            return "has " + *problem;
                        entry.mTargets.push_back(std::move(target));
                    }
                    continue;
                }
                if (key == "follow")
                {
                    std::vector<YAML::Node> quests;
                    if (field.second.IsSequence())
                        for (const YAML::Node& quest : field.second)
                            quests.push_back(quest);
                    else
                        quests.push_back(field.second);
                    for (const YAML::Node& quest : quests)
                    {
                        if (!quest.IsScalar() || quest.Scalar().empty())
                            return "has a \"follow\" that isn't a quest or a list of them";
                        entry.mFollow.push_back(ESM::RefId::stringRefId(quest.Scalar()));
                    }
                    continue;
                }
                std::vector<Range>* ranges = nullptr;
                if (key == "stages")
                    ranges = &entry.mStages;
                else if (key == "next")
                    ranges = &entry.mNext;
                else if (key == "optional")
                    ranges = &entry.mOptional;
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
        if (const YAML::Node fingerprints = root["fingerprints"])
        {
            if (!fingerprints.IsMap())
                problems.emplace_back("\"fingerprints\" isn't a map");
            else
                for (const auto& quest : fingerprints)
                {
                    if (!quest.second.IsScalar())
                    {
                        problems.push_back(quest.first.as<std::string>() + "'s fingerprint isn't a text");
                        continue;
                    }
                    corrections.mFingerprints[ESM::RefId::stringRefId(quest.first.as<std::string>())]
                        = quest.second.Scalar();
                }
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

    void check(Corrections& corrections, const Content& content, std::vector<std::string>& problems)
    {
        for (auto quest = corrections.mQuests.begin(); quest != corrections.mQuests.end();)
        {
            const auto stages = content.mStages.find(quest->first);
            if (stages == content.mStages.end())
            {
                problems.push_back(quest->first.getRefIdString() + " isn't in the content");
                quest = corrections.mQuests.erase(quest);
                continue;
            }
            const auto expected = corrections.mFingerprints.find(quest->first);
            if (expected != corrections.mFingerprints.end() && content.mFingerprint
                && content.mFingerprint(quest->first) != expected->second)
            {
                problems.push_back(quest->first.getRefIdString()
                    + " moves on differently in the loaded content than the entries were written for, such as after "
                      "a mod changed it, so its entries are left out");
                quest = corrections.mQuests.erase(quest);
                continue;
            }
            const auto known = [&](const std::vector<Range>& ranges) {
                return std::all_of(ranges.begin(), ranges.end(), [&](const Range& range) {
                    const auto it = stages->second.lower_bound(range.mFirst);
                    return it != stages->second.end() && *it <= range.mLast;
                });
            };
            std::vector<Entry>& entries = quest->second;
            for (std::size_t i = 0; i < entries.size();)
            {
                const Entry& entry = entries[i];
                const std::optional<std::string> problem = [&]() -> std::optional<std::string> {
                    if (!known(entry.mStages) || !known(entry.mNext) || !known(entry.mOptional) || !known(entry.mHide))
                        return "a stage";
                    for (const Target& target : entry.mTargets)
                    {
                        if (!target.mObject.empty() && !(content.mHasObject && content.mHasObject(target.mObject)))
                            return "the object " + target.mObject.getRefIdString();
                        if (!target.mCell.empty() && !(content.mHasCell && content.mHasCell(target.mCell)))
                            return "the cell " + target.mCell.getRefIdString();
                        std::vector<const Condition*> conditions;
                        for (const Condition& condition : target.mAfter)
                            conditions.push_back(&condition);
                        if (target.mDone)
                            conditions.push_back(&*target.mDone);
                        for (const Condition* condition : conditions)
                        {
                            if (!condition->mGlobal.empty()
                                && !(content.mHasGlobal && content.mHasGlobal(condition->mGlobal)))
                                return "the global variable " + condition->mGlobal;
                            if (!condition->mQuest.empty() && !content.mStages.contains(condition->mQuest))
                                return "the quest " + condition->mQuest.getRefIdString();
                            for (const ESM::RefId& object : { condition->mDead, condition->mCarried })
                                if (!object.empty() && !(content.mHasObject && content.mHasObject(object)))
                                    return "the object " + object.getRefIdString();
                        }
                    }
                    for (const ESM::RefId& followed : entry.mFollow)
                        if (!content.mStages.contains(followed))
                            return "the quest " + followed.getRefIdString();
                    return std::nullopt;
                }();
                if (!problem)
                {
                    ++i;
                    continue;
                }
                problems.push_back(quest->first.getRefIdString() + "'s entry for stage "
                    + std::to_string(entry.mStages.front().mFirst) + " names " + *problem
                    + " that the content doesn't have");
                entries.erase(entries.begin() + i);
            }
            if (entries.empty())
                quest = corrections.mQuests.erase(quest);
            else
                ++quest;
        }
    }

    std::string fingerprint(std::vector<std::string> facts)
    {
        std::sort(facts.begin(), facts.end());
        std::uint64_t hash = 14695981039346656037ull;
        for (const std::string& fact : facts)
        {
            // Each fact ends with a newline, so facts that run together differently hash differently.
            for (const char c : fact + "\n")
            {
                hash ^= static_cast<unsigned char>(c);
                hash *= 1099511628211ull;
            }
        }
        constexpr char digits[] = "0123456789abcdef";
        std::string result(16, '0');
        for (int i = 15; i >= 0; --i, hash >>= 4)
            result[i] = digits[hash & 0xf];
        return result;
    }

    std::string fingerprintOf(const ESM::RefId& quest, const std::set<int>& stages, const QuestTargets::Index& index)
    {
        std::vector<std::string> facts = index.shapeOf(quest);
        for (const int stage : stages)
            facts.push_back("stage|" + std::to_string(stage));
        return fingerprint(std::move(facts));
    }

    Content contentOf(const MWWorld::ESMStore& store, const QuestTargets::Index& index)
    {
        Content content;
        content.mHasObject = [&](const ESM::RefId& id) { return store.find(id) != 0; };
        content.mHasGlobal = [&](std::string_view name) {
            return store.get<ESM::Global>().search(ESM::RefId::stringRefId(name)) != nullptr;
        };
        content.mHasCell = [&](const ESM::RefId& id) {
            const std::string& name = id.getRefIdString();
            if (store.get<ESM::Cell>().search(name) != nullptr)
                return true;
            const MWWorld::Store<ESM::Cell>& cells = store.get<ESM::Cell>();
            return std::any_of(cells.extBegin(), cells.extEnd(),
                [&](const ESM::Cell& cell) { return Misc::StringUtils::ciEqual(cell.mName, name); });
        };
        for (const ESM::Dialogue& dialogue : store.get<ESM::Dialogue>())
        {
            if (dialogue.mType != ESM::Dialogue::Journal)
                continue;
            std::set<int>& stages = content.mStages[dialogue.mId];
            for (const ESM::DialInfo& info : dialogue.mInfo)
                stages.insert(info.mData.mJournalIndex);
        }
        content.mFingerprint = [stages = content.mStages, &index](const ESM::RefId& quest) {
            const auto it = stages.find(quest);
            return it == stages.end() ? std::string() : fingerprintOf(quest, it->second, index);
        };
        return content;
    }

    Corrections load(const VFS::Manager& vfs, const MWWorld::ESMStore& store, const QuestTargets::Index& index,
        std::vector<std::string>* problems)
    {
        constexpr VFS::Path::NormalizedView path("questcorrections.yaml");
        if (!vfs.exists(path))
            return {};
        const std::string text(std::istreambuf_iterator<char>(*vfs.get(path)), {});
        std::vector<std::string> found;
        Corrections corrections = parse(text, found);
        check(corrections, contentOf(store, index), found);
        for (const std::string& problem : found)
            Log(Debug::Warning) << "Quest marker corrections: " << problem;
        if (problems != nullptr)
            problems->insert(problems->end(), found.begin(), found.end());
        return corrections;
    }

    std::vector<QuestTargets::Way> chooseWays(
        std::vector<QuestTargets::Way> ways, const Entry* entry, QuestTargets::Rule rule)
    {
        if (entry == nullptr)
            return QuestTargets::chooseWays(std::move(ways), rule);
        const auto setAside = [](const QuestTargets::Way& way) { return way.mFailure || way.mOutOfReach; };
        std::vector<QuestTargets::Way> optional;
        std::vector<QuestTargets::Way> rest;
        for (QuestTargets::Way& way : ways)
        {
            if (contains(entry->mHide, way.mToStage))
                continue;
            if (contains(entry->mOptional, way.mToStage))
            {
                if (!setAside(way))
                {
                    way.mOptional = true;
                    optional.push_back(std::move(way));
                }
                continue;
            }
            rest.push_back(std::move(way));
        }
        std::vector<QuestTargets::Way> chosen;
        if (!entry->mNext.empty())
            for (const QuestTargets::Way& way : rest)
                if (contains(entry->mNext, way.mToStage) && !way.mOutOfReach)
                    chosen.push_back(way);
        if (!std::all_of(chosen.begin(), chosen.end(), [](const QuestTargets::Way& way) { return way.mFailure; }))
            std::erase_if(chosen, [](const QuestTargets::Way& way) { return way.mFailure; });
        if (chosen.empty())
            chosen = QuestTargets::chooseWays(std::move(rest), rule);
        chosen.insert(chosen.end(), std::make_move_iterator(optional.begin()), std::make_move_iterator(optional.end()));
        std::stable_sort(chosen.begin(), chosen.end(),
            [](const QuestTargets::Way& l, const QuestTargets::Way& r) { return l.mToStage < r.mToStage; });
        return chosen;
    }

    bool holds(const Condition& condition, const Progress& progress)
    {
        if (!condition.mDead.empty())
            return progress.mDead && progress.mDead(condition.mDead);
        if (!condition.mCarried.empty())
            return progress.mCarried && progress.mCarried(condition.mCarried) >= std::max(1, condition.mAtLeast);
        if (!condition.mGlobal.empty())
            return progress.mGlobal && progress.mGlobal(condition.mGlobal) >= static_cast<float>(condition.mAtLeast);
        if (progress.mFinished && progress.mFinished(condition.mQuest))
            return true;
        return !condition.mFinished && progress.mStage && progress.mStage(condition.mQuest) >= condition.mAtLeast;
    }

    std::vector<QuestTargets::Way> targetWays(const Entry& entry, const Progress* progress)
    {
        std::vector<QuestTargets::Way> ways;
        for (const Target& target : entry.mTargets)
        {
            if (progress != nullptr
                && (!std::all_of(target.mAfter.begin(), target.mAfter.end(),
                        [&](const Condition& condition) { return holds(condition, *progress); })
                    || (target.mDone && holds(*target.mDone, *progress))))
                continue;
            QuestTargets::Way way;
            QuestTargets::Objective objective;
            objective.mTask = target.mTask;
            objective.mObject = target.mObject;
            objective.mCell = target.mCell;
            objective.mCount = target.mCount;
            way.mObjectives.push_back(std::move(objective));
            way.mOptional = target.mOptional;
            ways.push_back(std::move(way));
        }
        return ways;
    }

    std::vector<ESM::RefId> followedQuests(const Entry& entry, const Progress& progress)
    {
        std::vector<ESM::RefId> result;
        for (const ESM::RefId& quest : entry.mFollow)
            if (progress.mStage(quest) > 0 && !progress.mFinished(quest))
                result.push_back(quest);
        return result;
    }

    std::vector<std::string> entriesWithoutEffect(const Corrections& corrections, const QuestTargets::Index& index,
        const Content& content, QuestTargets::Rule rule)
    {
        // The stages a choice leads to, with whether each way is optional.
        const auto summary = [](const std::vector<QuestTargets::Way>& ways) {
            std::vector<std::pair<int, bool>> result;
            for (const QuestTargets::Way& way : ways)
                result.emplace_back(way.mToStage, way.mOptional);
            std::sort(result.begin(), result.end());
            result.erase(std::unique(result.begin(), result.end()), result.end());
            return result;
        };
        std::vector<std::string> result;
        for (const auto& [quest, entries] : corrections.mQuests)
        {
            const auto stages = content.mStages.find(quest);
            for (const Entry& entry : entries)
            {
                if (!entry.mTargets.empty() || !entry.mFollow.empty())
                    continue;
                bool effect = false;
                if (stages != content.mStages.end())
                    for (const int stage : stages->second)
                    {
                        if (!contains(entry.mStages, stage))
                            continue;
                        const std::vector<QuestTargets::Way> ways = index.waysForward(quest, stage);
                        if (summary(chooseWays(ways, &entry, rule)) != summary(chooseWays(ways, nullptr, rule)))
                        {
                            effect = true;
                            break;
                        }
                    }
                if (!effect)
                    result.push_back(
                        quest.getRefIdString() + "'s entry for stage " + std::to_string(entry.mStages.front().mFirst));
            }
        }
        std::sort(result.begin(), result.end());
        return result;
    }
}
