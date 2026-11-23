#include "questtargets.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <string>
#include <variant>

#include <components/esm3/loadacti.hpp>
#include <components/esm3/loadalch.hpp>
#include <components/esm3/loadappa.hpp>
#include <components/esm3/loadarmo.hpp>
#include <components/esm3/loadbook.hpp>
#include <components/esm3/loadclot.hpp>
#include <components/esm3/loadcont.hpp>
#include <components/esm3/loadcrea.hpp>
#include <components/esm3/loaddial.hpp>
#include <components/esm3/loaddoor.hpp>
#include <components/esm3/loadinfo.hpp>
#include <components/esm3/loadingr.hpp>
#include <components/esm3/loadligh.hpp>
#include <components/esm3/loadlock.hpp>
#include <components/esm3/loadmisc.hpp>
#include <components/esm3/loadnpc.hpp>
#include <components/esm3/loadprob.hpp>
#include <components/esm3/loadrepa.hpp>
#include <components/esm3/loadscpt.hpp>
#include <components/esm3/loadweap.hpp>
#include <components/misc/strings/lower.hpp>

#include "../mwworld/esmstore.hpp"

namespace MWGui::QuestTargets
{
    namespace
    {
        // The words of a script, lowercase, without comments or commas, one line at a time. Text in quotes is one
        // word, without the quotes.
        std::vector<std::vector<std::string>> scriptWords(std::string_view script)
        {
            std::vector<std::vector<std::string>> lines(1);
            std::string word;
            bool quoted = false;
            bool comment = false;
            const auto endWord = [&] {
                if (!word.empty())
                    lines.back().push_back(std::move(word));
                word.clear();
            };
            for (const char c : script)
            {
                if (c == '\n' || c == '\r')
                {
                    endWord();
                    quoted = false;
                    comment = false;
                    if (!lines.back().empty())
                        lines.emplace_back();
                    continue;
                }
                if (comment)
                    continue;
                if (c == '"')
                {
                    if (quoted)
                    {
                        lines.back().push_back(std::move(word));
                        word.clear();
                    }
                    else
                        endWord();
                    quoted = !quoted;
                    continue;
                }
                if (quoted)
                {
                    word += Misc::StringUtils::toLower(c);
                    continue;
                }
                if (c == ';')
                {
                    endWord();
                    comment = true;
                    continue;
                }
                if (c == ',' || c == '(' || c == ')' || std::isspace(static_cast<unsigned char>(c)))
                {
                    endWord();
                    continue;
                }
                word += Misc::StringUtils::toLower(c);
            }
            endWord();
            return lines;
        }

        // A command, run on the object itself or on another one, such as "journal" or "fargoth->journal".
        bool isCommand(std::string_view word, std::string_view command)
        {
            if (word == command)
                return true;
            const std::size_t arrow = word.rfind("->");
            return arrow != std::string_view::npos && word.substr(arrow + 2) == command;
        }

        float valueOf(const ESM::DialogueCondition& condition)
        {
            return std::visit([](auto value) { return static_cast<float>(value); }, condition.mValue);
        }

        // How many a condition on a count needs at least, such as "Dead >= 3", or 0 when it needs none.
        int countNeeded(const ESM::DialogueCondition& condition)
        {
            const float value = valueOf(condition);
            switch (condition.mComparison)
            {
                case ESM::DialogueCondition::Comp_Eq:
                case ESM::DialogueCondition::Comp_Ge:
                    return std::max(0, static_cast<int>(value));
                case ESM::DialogueCondition::Comp_Gt:
                    return std::max(0, static_cast<int>(value) + 1);
                default:
                    return 0;
            }
        }

        bool sameQuest(const std::string& variable, const ESM::RefId& quest)
        {
            return ESM::RefId::stringRefId(variable) == quest;
        }

        template <class T>
        void addCarriers(const MWWorld::ESMStore& store, bool actor,
            std::unordered_map<ESM::RefId, std::vector<Index::Carrier>>& carriers)
        {
            for (const T& record : store.get<T>())
                if (!record.mScript.empty())
                    carriers[record.mScript].push_back(Index::Carrier{ record.mId, actor });
        }
    }

    std::vector<std::pair<ESM::RefId, int>> journalChanges(std::string_view script)
    {
        std::vector<std::pair<ESM::RefId, int>> changes;
        for (const std::vector<std::string>& words : scriptWords(script))
        {
            for (std::size_t i = 0; i + 2 < words.size(); ++i)
            {
                if (!isCommand(words[i], "journal") && !isCommand(words[i], "setjournalindex"))
                    continue;
                const std::string& stage = words[i + 2];
                int value = 0;
                const auto [end, error] = std::from_chars(stage.data(), stage.data() + stage.size(), value);
                if (error != std::errc() || end != stage.data() + stage.size())
                    continue;
                changes.emplace_back(ESM::RefId::stringRefId(words[i + 1]), value);
            }
        }
        return changes;
    }

    ScriptEvent scriptEvent(std::string_view script)
    {
        for (const std::vector<std::string>& words : scriptWords(script))
        {
            for (const std::string& word : words)
            {
                if (isCommand(word, "ondeath") || isCommand(word, "gethealth"))
                    return ScriptEvent::Death;
                if (isCommand(word, "onpcadd"))
                    return ScriptEvent::PickUp;
            }
        }
        return ScriptEvent::Other;
    }

    std::vector<ESM::RefId> deadCountTargets(std::string_view script)
    {
        std::vector<ESM::RefId> targets;
        for (const std::vector<std::string>& words : scriptWords(script))
            for (std::size_t i = 0; i + 1 < words.size(); ++i)
                if (isCommand(words[i], "getdeadcount"))
                {
                    const ESM::RefId id = ESM::RefId::stringRefId(words[i + 1]);
                    if (std::find(targets.begin(), targets.end(), id) == targets.end())
                        targets.push_back(id);
                }
        return targets;
    }

    bool allowsStage(const std::vector<ESM::DialogueCondition>& conditions, const ESM::RefId& quest, int stage)
    {
        for (const ESM::DialogueCondition& condition : conditions)
        {
            if (condition.mFunction != ESM::DialogueCondition::Function_Journal
                || !sameQuest(condition.mVariable, quest))
                continue;
            const float value = valueOf(condition);
            const float at = static_cast<float>(stage);
            bool allowed = true;
            switch (condition.mComparison)
            {
                case ESM::DialogueCondition::Comp_Eq:
                    allowed = at == value;
                    break;
                case ESM::DialogueCondition::Comp_Ne:
                    allowed = at != value;
                    break;
                case ESM::DialogueCondition::Comp_Gt:
                    allowed = at > value;
                    break;
                case ESM::DialogueCondition::Comp_Ge:
                    allowed = at >= value;
                    break;
                case ESM::DialogueCondition::Comp_Ls:
                    allowed = at < value;
                    break;
                case ESM::DialogueCondition::Comp_Le:
                    allowed = at <= value;
                    break;
                default:
                    break;
            }
            if (!allowed)
                return false;
        }
        return true;
    }

    void Index::addDialogue(const ESM::DialInfo& info)
    {
        for (const auto& [quest, stage] : journalChanges(info.mResultScript))
        {
            Source source;
            source.mToStage = stage;
            source.mDialogue = true;
            source.mConditions = info.mSelects;
            source.mSpeaker = info.mActor;
            source.mFaction = info.mFaction;
            source.mCell = info.mCell;
            mSources[quest].push_back(std::move(source));
            if (!info.mActor.empty())
                mSpeakers[quest].insert(info.mActor);
        }
    }

    void Index::addScript(std::string_view text, const std::vector<Carrier>& carriers)
    {
        const std::vector<std::pair<ESM::RefId, int>> changes = journalChanges(text);
        if (changes.empty())
            return;
        const ScriptEvent event = scriptEvent(text);
        const std::vector<ESM::RefId> deadCounts
            = carriers.empty() ? deadCountTargets(text) : std::vector<ESM::RefId>();
        for (const auto& [quest, stage] : changes)
        {
            Source source;
            source.mToStage = stage;
            source.mCarriers = carriers;
            source.mEvent = event;
            source.mDeadCountTargets = deadCounts;
            mSources[quest].push_back(std::move(source));
        }
    }

    std::vector<Objective> Index::objectivesOf(const ESM::RefId& quest, const Source& source) const
    {
        std::vector<Objective> objectives;
        if (!source.mDialogue)
        {
            for (const Carrier& carrier : source.mCarriers)
            {
                Objective objective;
                objective.mObject = carrier.mId;
                if (carrier.mActor && source.mEvent == ScriptEvent::Death)
                    objective.mTask = Task::Kill;
                else if (!carrier.mActor && source.mEvent == ScriptEvent::PickUp)
                    objective.mTask = Task::Fetch;
                else
                    objective.mTask = Task::Visit;
                objectives.push_back(objective);
            }
            for (const ESM::RefId& target : source.mDeadCountTargets)
                objectives.push_back(Objective{ .mTask = Task::Kill, .mObject = target });
            return objectives;
        }

        const auto speakers = mSpeakers.find(quest);
        for (const ESM::DialogueCondition& condition : source.mConditions)
        {
            const int count = countNeeded(condition);
            if (count <= 0)
                continue;
            const ESM::RefId id = ESM::RefId::stringRefId(condition.mVariable);
            if (condition.mFunction == ESM::DialogueCondition::Function_Dead)
            {
                // Needing someone who speaks in the quest dead is a failure branch, such as the quest giver dying.
                if (speakers != mSpeakers.end() && speakers->second.contains(id))
                    return {};
                objectives.push_back(Objective{ .mTask = Task::Kill, .mObject = id, .mCount = count });
            }
            else if (condition.mFunction == ESM::DialogueCondition::Function_Item
                && id != ESM::RefId::stringRefId("gold_001"))
                objectives.push_back(Objective{ .mTask = Task::Fetch, .mObject = id, .mCount = count });
        }
        if (!source.mSpeaker.empty())
            objectives.push_back(Objective{ .mTask = Task::Talk, .mObject = source.mSpeaker });
        else if (!source.mFaction.empty())
            objectives.push_back(Objective{ .mTask = Task::Talk, .mFaction = source.mFaction });
        else if (!source.mCell.empty())
            objectives.push_back(Objective{ .mTask = Task::Talk, .mCell = source.mCell });
        return objectives;
    }

    std::vector<Way> Index::waysForward(const ESM::RefId& quest, int stage, Rule rule) const
    {
        std::vector<Way> ways;
        const auto it = mSources.find(quest);
        if (it == mSources.end())
            return ways;
        for (const Source& source : it->second)
        {
            if (source.mToStage <= stage)
                continue;
            if (source.mDialogue && !allowsStage(source.mConditions, quest, stage))
                continue;
            Way way{ source.mToStage, objectivesOf(quest, source) };
            if (way.mObjectives.empty())
                continue;
            const bool known = std::any_of(ways.begin(), ways.end(), [&](const Way& other) {
                return other.mToStage == way.mToStage && other.mObjectives == way.mObjectives;
            });
            if (!known)
                ways.push_back(std::move(way));
        }
        if (rule == Rule::Lowest && !ways.empty())
        {
            const int lowest = std::min_element(ways.begin(), ways.end(), [](const Way& l, const Way& r) {
                return l.mToStage < r.mToStage;
            })->mToStage;
            std::erase_if(ways, [&](const Way& way) { return way.mToStage != lowest; });
        }
        std::stable_sort(ways.begin(), ways.end(), [](const Way& l, const Way& r) { return l.mToStage < r.mToStage; });
        return ways;
    }

    Index Index::build(const MWWorld::ESMStore& store)
    {
        Index index;
        for (const ESM::Dialogue& dialogue : store.get<ESM::Dialogue>())
        {
            if (dialogue.mType == ESM::Dialogue::Journal)
                continue;
            for (const ESM::DialInfo& info : dialogue.mInfo)
                index.addDialogue(info);
        }

        std::unordered_map<ESM::RefId, std::vector<Carrier>> carriers;
        addCarriers<ESM::NPC>(store, true, carriers);
        addCarriers<ESM::Creature>(store, true, carriers);
        addCarriers<ESM::Activator>(store, false, carriers);
        addCarriers<ESM::Container>(store, false, carriers);
        addCarriers<ESM::Door>(store, false, carriers);
        addCarriers<ESM::Miscellaneous>(store, false, carriers);
        addCarriers<ESM::Book>(store, false, carriers);
        addCarriers<ESM::Weapon>(store, false, carriers);
        addCarriers<ESM::Armor>(store, false, carriers);
        addCarriers<ESM::Clothing>(store, false, carriers);
        addCarriers<ESM::Ingredient>(store, false, carriers);
        addCarriers<ESM::Light>(store, false, carriers);
        addCarriers<ESM::Lockpick>(store, false, carriers);
        addCarriers<ESM::Probe>(store, false, carriers);
        addCarriers<ESM::Repair>(store, false, carriers);
        addCarriers<ESM::Apparatus>(store, false, carriers);
        addCarriers<ESM::Potion>(store, false, carriers);
        static const std::vector<Carrier> none;
        for (const ESM::Script& script : store.get<ESM::Script>())
        {
            const auto it = carriers.find(script.mId);
            index.addScript(script.mScriptText, it != carriers.end() ? it->second : none);
        }
        return index;
    }
}
