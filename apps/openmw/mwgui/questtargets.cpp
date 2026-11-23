#include "questtargets.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <optional>
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

    namespace
    {
        // The conditions of the if blocks around a line, innermost first. In an elseif block, that line's condition
        // counts, and the conditions of the chain's other branches don't. An else block adds none.
        std::vector<std::vector<std::string>> enclosingConditions(
            const std::vector<std::vector<std::string>>& lines, std::size_t line)
        {
            std::vector<std::vector<std::string>> conditions;
            int depth = 0;
            bool inChain = false;
            for (std::size_t j = line; j-- > 0;)
            {
                if (lines[j].empty())
                    continue;
                const std::string& first = lines[j][0];
                if (first == "endif" || first == "endwhile")
                    ++depth;
                else if (first == "if" || first == "while")
                {
                    if (depth > 0)
                        --depth;
                    else
                    {
                        if (!inChain)
                            conditions.push_back(lines[j]);
                        inChain = false;
                    }
                }
                else if ((first == "elseif" || first == "else") && depth == 0)
                {
                    if (!inChain && first == "elseif")
                        conditions.push_back(lines[j]);
                    inChain = true;
                }
            }
            return conditions;
        }

        // The object a command runs on, such as "player" in "player->getdistance", or empty for the script's own.
        std::string_view caller(std::string_view word)
        {
            const std::size_t arrow = word.rfind("->");
            return arrow == std::string_view::npos ? std::string_view() : word.substr(0, arrow);
        }

        // The early returns at the top level of a script: an if block that holds a return, and no elseif or else.
        // Each is the line its block ends on, and its condition.
        std::vector<std::pair<std::size_t, std::vector<std::string>>> earlyReturns(
            const std::vector<std::vector<std::string>>& lines)
        {
            std::vector<std::pair<std::size_t, std::vector<std::string>>> guards;
            int depth = 0;
            for (std::size_t l = 0; l < lines.size(); ++l)
            {
                if (lines[l].empty())
                    continue;
                const std::string& first = lines[l][0];
                if (first == "if" || first == "while")
                {
                    if (depth == 0)
                    {
                        bool returns = false;
                        bool branches = false;
                        int inner = 0;
                        std::size_t end = l + 1;
                        for (; end < lines.size(); ++end)
                        {
                            if (lines[end].empty())
                                continue;
                            const std::string& word = lines[end][0];
                            if (word == "if" || word == "while")
                                ++inner;
                            else if (word == "endif" || word == "endwhile")
                            {
                                if (inner == 0)
                                    break;
                                --inner;
                            }
                            else if (inner == 0 && (word == "elseif" || word == "else"))
                                branches = true;
                            else if (inner == 0 && word == "return")
                                returns = true;
                        }
                        if (returns && !branches && first == "if")
                            guards.emplace_back(end, lines[l]);
                    }
                    ++depth;
                }
                else if (first == "endif" || first == "endwhile")
                    depth = std::max(0, depth - 1);
            }
            return guards;
        }

        // A comparison as a script writes it, such as ">=", turned around when the condition must not hold.
        std::optional<ESM::DialogueCondition::Comparison> comparisonOf(std::string_view op, bool negated)
        {
            using C = ESM::DialogueCondition;
            if (op == "==" || op == "=")
                return negated ? C::Comp_Ne : C::Comp_Eq;
            if (op == "!=")
                return negated ? C::Comp_Eq : C::Comp_Ne;
            if (op == "<")
                return negated ? C::Comp_Ge : C::Comp_Ls;
            if (op == "<=")
                return negated ? C::Comp_Gt : C::Comp_Le;
            if (op == ">")
                return negated ? C::Comp_Le : C::Comp_Gt;
            if (op == ">=")
                return negated ? C::Comp_Ls : C::Comp_Ge;
            return std::nullopt;
        }

        void addStageChecks(
            const std::vector<std::string>& words, bool negated, std::vector<ESM::DialogueCondition>& checks)
        {
            for (std::size_t i = 0; i + 3 < words.size(); ++i)
            {
                if (!isCommand(words[i], "getjournalindex"))
                    continue;
                const std::optional<ESM::DialogueCondition::Comparison> comparison
                    = comparisonOf(words[i + 2], negated);
                const std::string& number = words[i + 3];
                int value = 0;
                const auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), value);
                if (!comparison || error != std::errc() || end != number.data() + number.size())
                    continue;
                ESM::DialogueCondition check;
                check.mFunction = ESM::DialogueCondition::Function_Journal;
                check.mVariable = words[i + 1];
                check.mComparison = *comparison;
                check.mValue = value;
                checks.push_back(std::move(check));
            }
        }

        void addUnique(std::vector<ESM::RefId>& ids, const ESM::RefId& id)
        {
            if (std::find(ids.begin(), ids.end(), id) == ids.end())
                ids.push_back(id);
        }
    }

    std::vector<ScriptJournalCall> scriptJournalCalls(std::string_view script)
    {
        const std::vector<std::vector<std::string>> lines = scriptWords(script);
        const std::vector<std::pair<std::size_t, std::vector<std::string>>> guards = earlyReturns(lines);
        std::vector<ScriptJournalCall> calls;
        for (std::size_t l = 0; l < lines.size(); ++l)
        {
            const std::vector<std::string>& words = lines[l];
            for (std::size_t i = 0; i + 2 < words.size(); ++i)
            {
                if (!isCommand(words[i], "journal") && !isCommand(words[i], "setjournalindex"))
                    continue;
                const std::string& stage = words[i + 2];
                int value = 0;
                const auto [end, error] = std::from_chars(stage.data(), stage.data() + stage.size(), value);
                if (error != std::errc() || end != stage.data() + stage.size())
                    continue;
                ScriptJournalCall call{ ESM::RefId::stringRefId(words[i + 1]), value, enclosingConditions(lines, l),
                    {} };
                for (const auto& [guardEnd, condition] : guards)
                    if (guardEnd < l)
                        call.mGuards.push_back(condition);
                calls.push_back(std::move(call));
            }
        }
        return calls;
    }

    std::vector<std::pair<ESM::RefId, int>> journalChanges(std::string_view script)
    {
        std::vector<std::pair<ESM::RefId, int>> changes;
        for (const ScriptJournalCall& call : scriptJournalCalls(script))
            changes.emplace_back(call.mQuest, call.mStage);
        return changes;
    }

    CallTargets callTargets(const std::vector<std::vector<std::string>>& conditions)
    {
        CallTargets targets;
        for (const std::vector<std::string>& words : conditions)
        {
            for (std::size_t i = 0; i < words.size(); ++i)
            {
                const std::string& word = words[i];
                const std::string_view on = caller(word);
                const std::string* argument = i + 1 < words.size() ? &words[i + 1] : nullptr;
                if (targets.mEvent == ScriptEvent::Other
                    && (isCommand(word, "ondeath") || isCommand(word, "gethealth")))
                    targets.mEvent = ScriptEvent::Death;
                else if (targets.mEvent == ScriptEvent::Other && isCommand(word, "onpcadd"))
                    targets.mEvent = ScriptEvent::PickUp;
                else if (isCommand(word, "getdistance") && argument != nullptr)
                {
                    // The player's distance to something, or something's distance to the player.
                    std::string_view near;
                    if (on == "player")
                        near = *argument;
                    else if (*argument == "player")
                        near = on;
                    else if (on.empty())
                        near = *argument;
                    if (!near.empty() && near != "player")
                        addUnique(targets.mNear, ESM::RefId::stringRefId(near));
                }
                else if (isCommand(word, "getpccell") && argument != nullptr)
                    addUnique(targets.mCells, ESM::RefId::stringRefId(*argument));
                else if (isCommand(word, "getdeadcount") && argument != nullptr)
                    addUnique(targets.mKills, ESM::RefId::stringRefId(*argument));
                else if (isCommand(word, "getitemcount") && argument != nullptr && on == "player")
                    addUnique(targets.mFetches, ESM::RefId::stringRefId(*argument));
            }
        }
        return targets;
    }

    std::vector<ESM::DialogueCondition> stageChecks(const ScriptJournalCall& call)
    {
        std::vector<ESM::DialogueCondition> checks;
        for (const std::vector<std::string>& words : call.mConditions)
            addStageChecks(words, false, checks);
        for (const std::vector<std::string>& words : call.mGuards)
            addStageChecks(words, true, checks);
        return checks;
    }

    bool compare(const ESM::DialogueCondition& condition, float value)
    {
        const float wanted = valueOf(condition);
        switch (condition.mComparison)
        {
            case ESM::DialogueCondition::Comp_Eq:
                return value == wanted;
            case ESM::DialogueCondition::Comp_Ne:
                return value != wanted;
            case ESM::DialogueCondition::Comp_Gt:
                return value > wanted;
            case ESM::DialogueCondition::Comp_Ge:
                return value >= wanted;
            case ESM::DialogueCondition::Comp_Ls:
                return value < wanted;
            case ESM::DialogueCondition::Comp_Le:
                return value <= wanted;
            default:
                return true;
        }
    }

    bool allowsStage(const std::vector<ESM::DialogueCondition>& conditions, const ESM::RefId& quest, int stage)
    {
        for (const ESM::DialogueCondition& condition : conditions)
            if (condition.mFunction == ESM::DialogueCondition::Function_Journal && sameQuest(condition.mVariable, quest)
                && !compare(condition, static_cast<float>(stage)))
                return false;
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
            source.mRank = info.mData.mRank;
            mSources[quest].push_back(std::move(source));
            if (!info.mActor.empty())
                mSpeakers[quest].insert(info.mActor);
        }
    }

    void Index::addScript(std::string_view text, const std::vector<Carrier>& carriers)
    {
        const std::vector<ScriptJournalCall> calls = scriptJournalCalls(text);
        if (calls.empty())
            return;
        // A Journal command outside any if block, such as one in a script that a dialogue starts, waits for whatever
        // the whole script waits for. One inside if blocks waits only for what those conditions say, so a counter
        // or a flag doesn't turn a friendly actor's OnDeath elsewhere in the script into a kill.
        const CallTargets whole = callTargets(scriptWords(text));
        for (const ScriptJournalCall& call : calls)
        {
            Source source;
            source.mToStage = call.mStage;
            source.mCarriers = carriers;
            source.mTargets = call.mConditions.empty() ? whole : callTargets(call.mConditions);
            // Checks on quests' stages work as a dialogue line's do: on this quest they rule stages out, and on
            // others the game checks them while playing.
            source.mConditions = stageChecks(call);
            mSources[call.mQuest].push_back(std::move(source));
        }
    }

    void Index::addHolder(const ESM::RefId& item, const ESM::RefId& holder)
    {
        addUnique(mHolders[item], holder);
    }

    void Index::addMember(const ESM::RefId& faction, const ESM::RefId& person, int rank)
    {
        mMembers[faction].push_back(Member{ person, rank });
    }

    const std::vector<ESM::RefId>& Index::holdersOf(const ESM::RefId& item) const
    {
        static const std::vector<ESM::RefId> none;
        const auto it = mHolders.find(item);
        return it != mHolders.end() ? it->second : none;
    }

    std::vector<ESM::RefId> Index::membersOf(const ESM::RefId& faction, int rank) const
    {
        std::vector<ESM::RefId> members;
        const auto it = mMembers.find(faction);
        if (it != mMembers.end())
            for (const Member& member : it->second)
                if (member.mRank >= rank)
                    members.push_back(member.mId);
        return members;
    }

    std::vector<Objective> Index::objectivesOf(const ESM::RefId& quest, const Source& source) const
    {
        std::vector<Objective> objectives;
        if (!source.mDialogue)
        {
            // What the conditions around the Journal command name comes first: where to go, what to get, what to
            // kill. Without any, the objects the script is on are the targets.
            const CallTargets& targets = source.mTargets;
            for (const ESM::RefId& cell : targets.mCells)
                objectives.push_back(Objective{ .mTask = Task::Visit, .mCell = cell });
            for (const ESM::RefId& near : targets.mNear)
                objectives.push_back(Objective{ .mTask = Task::Visit, .mObject = near });
            for (const ESM::RefId& item : targets.mFetches)
                objectives.push_back(Objective{ .mTask = Task::Fetch, .mObject = item });
            for (const ESM::RefId& target : targets.mKills)
                objectives.push_back(Objective{ .mTask = Task::Kill, .mObject = target });
            if (!objectives.empty())
                return objectives;
            for (const Carrier& carrier : source.mCarriers)
            {
                Objective objective;
                objective.mObject = carrier.mId;
                if (carrier.mActor && targets.mEvent == ScriptEvent::Death)
                    objective.mTask = Task::Kill;
                else if (!carrier.mActor && targets.mEvent == ScriptEvent::PickUp)
                    objective.mTask = Task::Fetch;
                else
                    objective.mTask = Task::Visit;
                objectives.push_back(objective);
            }
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
            objectives.push_back(Objective{
                .mTask = Task::Talk, .mFaction = source.mFaction, .mCell = source.mCell, .mRank = source.mRank });
        else if (!source.mCell.empty())
            objectives.push_back(Objective{ .mTask = Task::Talk, .mCell = source.mCell });
        return objectives;
    }

    std::vector<Way> chooseWays(std::vector<Way> ways, Rule rule)
    {
        // An answer in a conversation only comes up while talking to that person already, so it isn't somewhere to
        // go unless there's nothing else.
        if (std::any_of(ways.begin(), ways.end(), [](const Way& way) { return !way.mChoice; }))
            std::erase_if(ways, [](const Way& way) { return way.mChoice; });
        if (rule != Rule::All && !ways.empty())
        {
            const auto [low, high] = std::minmax_element(
                ways.begin(), ways.end(), [](const Way& l, const Way& r) { return l.mToStage < r.mToStage; });
            const int lowest = low->mToStage;
            const int highest = rule == Rule::LowestAndHighest ? high->mToStage : lowest;
            std::erase_if(ways, [&](const Way& way) { return way.mToStage != lowest && way.mToStage != highest; });
        }
        std::stable_sort(ways.begin(), ways.end(), [](const Way& l, const Way& r) { return l.mToStage < r.mToStage; });
        return ways;
    }

    std::vector<Way> Index::waysForward(const ESM::RefId& quest, int stage, Rule rule) const
    {
        return chooseWays(waysForward(quest, stage), rule);
    }

    std::vector<Way> Index::waysForward(const ESM::RefId& quest, int stage) const
    {
        std::vector<Way> ways;
        const auto it = mSources.find(quest);
        if (it == mSources.end())
            return ways;
        for (const Source& source : it->second)
        {
            if (source.mToStage <= stage)
                continue;
            if (!allowsStage(source.mConditions, quest, stage))
                continue;
            Way way{ source.mToStage, objectivesOf(quest, source) };
            if (way.mObjectives.empty())
                continue;
            for (const ESM::DialogueCondition& condition : source.mConditions)
            {
                if (condition.mFunction == ESM::DialogueCondition::Function_Choice)
                    way.mChoice = true;
                else if (condition.mFunction == ESM::DialogueCondition::Function_Global
                    || condition.mFunction == ESM::DialogueCondition::Function_Local
                    || (condition.mFunction == ESM::DialogueCondition::Function_Journal
                        && !sameQuest(condition.mVariable, quest)))
                    way.mChecks.push_back(condition);
            }
            way.mSpeaker = source.mSpeaker;
            const bool known = std::any_of(ways.begin(), ways.end(), [&](const Way& other) {
                return other.mToStage == way.mToStage && other.mObjectives == way.mObjectives
                    && other.mChoice == way.mChoice;
            });
            if (!known)
                ways.push_back(std::move(way));
        }
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

        for (const ESM::NPC& npc : store.get<ESM::NPC>())
        {
            for (const ESM::ContItem& item : npc.mInventory.mList)
                index.addHolder(item.mItem, npc.mId);
            if (!npc.mFaction.empty())
                index.addMember(npc.mFaction, npc.mId, npc.getFactionRank());
        }
        for (const ESM::Creature& creature : store.get<ESM::Creature>())
            for (const ESM::ContItem& item : creature.mInventory.mList)
                index.addHolder(item.mItem, creature.mId);
        for (const ESM::Container& container : store.get<ESM::Container>())
            for (const ESM::ContItem& item : container.mInventory.mList)
                index.addHolder(item.mItem, container.mId);
        return index;
    }
}
