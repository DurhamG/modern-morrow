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

#include "topiclearning.hpp"

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

        // Whether a condition is on the gold the player carries, which the game tests while playing. Gold isn't
        // something to fetch from a place, so it is a check, not an objective.
        bool isGoldCondition(const ESM::DialogueCondition& condition)
        {
            return condition.mFunction == ESM::DialogueCondition::Function_Item
                && ESM::RefId::stringRefId(condition.mVariable) == ESM::RefId::stringRefId("gold_001");
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

        // Whether the comparison at words[op] holds only when the command before it answers no or none, such as the
        // "== 0" in `GetPCCell "Bal Isra" == 0`. A test like that waits for something not to be so, which isn't a
        // place to go, someone to kill or something to carry.
        bool testsForNone(const std::vector<std::string>& words, std::size_t op)
        {
            if (op + 1 >= words.size())
                return false;
            const std::string& comparison = words[op];
            const std::string& value = words[op + 1];
            if (value == "0")
                return comparison == "==" || comparison == "=" || comparison == "<=";
            if (value == "1")
                return comparison == "<" || comparison == "!=";
            return false;
        }

        // Whether the comparison at words[op] holds for small values only, such as the "< 512" in
        // `GetDistance Player < 512`.
        bool testsForAtMost(const std::vector<std::string>& words, std::size_t op)
        {
            return op < words.size() && (words[op] == "<" || words[op] == "<=");
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

        // Adds the actors a script tells to follow or escort the player, such as `AiFollow Player 0 0 0 0`. The
        // command without an object is run on `self`, the speaker or the actors the script is on.
        void addCompanions(
            std::string_view script, const std::vector<ESM::RefId>& self, std::unordered_set<ESM::RefId>& companions)
        {
            for (const std::vector<std::string>& words : scriptWords(script))
                for (std::size_t i = 0; i + 1 < words.size(); ++i)
                {
                    if (words[i + 1] != "player"
                        || (!isCommand(words[i], "aifollow") && !isCommand(words[i], "aiescort")))
                        continue;
                    // A quoted object comes out as a word of its own, before the "->".
                    std::string_view on = caller(words[i]);
                    if (on.empty() && words[i].starts_with("->") && i > 0)
                        on = words[i - 1];
                    if (on.empty())
                        companions.insert(self.begin(), self.end());
                    else
                        companions.insert(ESM::RefId::stringRefId(on));
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

    std::vector<int> choicesOffered(std::string_view script)
    {
        std::vector<int> choices;
        for (const std::vector<std::string>& words : scriptWords(script))
        {
            // Bloodmoon's lines often write the command with a colon, such as `Choice: "Yes" 1`, which the game accepts.
            if (words.empty() || (words[0] != "choice" && words[0] != "choice:"))
                continue;
            // The answers' texts and numbers alternate.
            for (std::size_t i = 2; i < words.size(); i += 2)
            {
                int number = 0;
                const std::string& word = words[i];
                const auto [end, error] = std::from_chars(word.data(), word.data() + word.size(), number);
                if (error == std::errc() && end == word.data() + word.size())
                    choices.push_back(number);
            }
        }
        return choices;
    }

    std::vector<ESM::DialInfo> withOfferingConditions(const std::vector<ESM::DialInfo>& lines)
    {
        const auto same = [](const ESM::DialogueCondition& l, const ESM::DialogueCondition& r) {
            return l.mFunction == r.mFunction && l.mVariable == r.mVariable && l.mComparison == r.mComparison
                && l.mValue == r.mValue;
        };
        std::vector<ESM::DialInfo> result = lines;
        for (ESM::DialInfo& answer : result)
        {
            const auto choice = std::find_if(
                answer.mSelects.begin(), answer.mSelects.end(), [](const ESM::DialogueCondition& condition) {
                    return condition.mFunction == ESM::DialogueCondition::Function_Choice;
                });
            if (choice == answer.mSelects.end())
                continue;
            const int number = static_cast<int>(valueOf(*choice));
            std::vector<const ESM::DialInfo*> offering;
            for (const ESM::DialInfo& line : result)
            {
                const std::vector<int> offered = choicesOffered(line.mResultScript);
                if (&line != &answer && std::find(offered.begin(), offered.end(), number) != offered.end())
                    offering.push_back(&line);
            }
            if (offering.empty())
                continue;
            // The conditions every offering line has, as far as they are known by now.
            std::vector<ESM::DialogueCondition> shared;
            // A quest whose stage an offering line changes is at that stage when the player answers, and the answer
            // only comes in that conversation. So the answer needs the quest at that stage, when every offering line
            // sets the same one.
            std::vector<ESM::RefId> moved;
            std::map<ESM::RefId, std::set<int>> setStages;
            for (const ESM::DialInfo* line : offering)
                for (const auto& [quest, stage] : journalChanges(line->mResultScript))
                {
                    moved.push_back(quest);
                    setStages[quest].insert(stage);
                }
            for (const auto& [quest, stages] : setStages)
            {
                const bool everyLine = std::all_of(offering.begin(), offering.end(), [&](const ESM::DialInfo* line) {
                    const auto changes = journalChanges(line->mResultScript);
                    return std::any_of(
                        changes.begin(), changes.end(), [&](const auto& change) { return change.first == quest; });
                });
                if (stages.size() != 1 || !everyLine)
                    continue;
                ESM::DialogueCondition atStage;
                atStage.mFunction = ESM::DialogueCondition::Function_Journal;
                atStage.mVariable = quest.getRefIdString();
                atStage.mComparison = ESM::DialogueCondition::Comp_Eq;
                atStage.mValue = *stages.begin();
                const bool known = std::any_of(answer.mSelects.begin(), answer.mSelects.end(),
                    [&](const ESM::DialogueCondition& other) { return same(atStage, other); });
                if (!known)
                    answer.mSelects.push_back(std::move(atStage));
            }
            for (const ESM::DialogueCondition& condition : offering.front()->mSelects)
            {
                if (condition.mFunction == ESM::DialogueCondition::Function_Choice)
                    continue;
                if (condition.mFunction == ESM::DialogueCondition::Function_Journal
                    && std::find(moved.begin(), moved.end(), ESM::RefId::stringRefId(condition.mVariable))
                        != moved.end())
                    continue;
                const bool everywhere
                    = std::all_of(offering.begin() + 1, offering.end(), [&](const ESM::DialInfo* line) {
                          return std::any_of(line->mSelects.begin(), line->mSelects.end(),
                              [&](const ESM::DialogueCondition& other) { return same(condition, other); });
                      });
                const bool known = std::any_of(answer.mSelects.begin(), answer.mSelects.end(),
                    [&](const ESM::DialogueCondition& other) { return same(condition, other); });
                if (everywhere && !known)
                    shared.push_back(condition);
            }
            answer.mSelects.insert(answer.mSelects.end(), shared.begin(), shared.end());
            const auto inherit = [&](ESM::RefId ESM::DialInfo::*field) {
                if (!(answer.*field).empty())
                    return;
                const ESM::RefId& first = offering.front()->*field;
                if (std::all_of(offering.begin(), offering.end(),
                        [&](const ESM::DialInfo* line) { return line->*field == first; }))
                    answer.*field = first;
            };
            inherit(&ESM::DialInfo::mActor);
            inherit(&ESM::DialInfo::mFaction);
            inherit(&ESM::DialInfo::mCell);
        }
        return result;
    }

    std::vector<std::pair<ESM::RefId, int>> itemsTaken(std::string_view script)
    {
        std::vector<std::pair<ESM::RefId, int>> taken;
        for (const std::vector<std::string>& words : scriptWords(script))
            for (std::size_t i = 0; i + 2 < words.size(); ++i)
            {
                if (!isCommand(words[i], "removeitem") || caller(words[i]) != "player")
                    continue;
                const std::string& number = words[i + 2];
                int count = 0;
                const auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), count);
                if (error == std::errc() && end == number.data() + number.size() && count > 0)
                    taken.emplace_back(ESM::RefId::stringRefId(words[i + 1]), count);
            }
        return taken;
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
                if (targets.mEvent == ScriptEvent::Other && isCommand(word, "ondeath") && !testsForNone(words, i + 1))
                    targets.mEvent = ScriptEvent::Death;
                else if (targets.mEvent == ScriptEvent::Other && isCommand(word, "gethealth")
                    && testsForAtMost(words, i + 1))
                    targets.mEvent = ScriptEvent::Death;
                else if (targets.mEvent == ScriptEvent::Other && isCommand(word, "onpcadd")
                    && !testsForNone(words, i + 1))
                    targets.mEvent = ScriptEvent::PickUp;
                else if (isCommand(word, "getdistance") && argument != nullptr && testsForAtMost(words, i + 2))
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
                else if (argument == nullptr || testsForNone(words, i + 2))
                    continue;
                else if (isCommand(word, "getpccell"))
                    addUnique(targets.mCells, ESM::RefId::stringRefId(*argument));
                else if (isCommand(word, "getdeadcount"))
                    addUnique(targets.mKills, ESM::RefId::stringRefId(*argument));
                else if (isCommand(word, "getitemcount") && on == "player")
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

    void Index::addDialogue(const ESM::DialInfo& info, std::string_view topic)
    {
        const std::vector<std::pair<ESM::RefId, int>> taken = itemsTaken(info.mResultScript);
        for (const auto& [quest, stage] : journalChanges(info.mResultScript))
        {
            Source source;
            source.mToStage = stage;
            source.mDialogue = true;
            source.mTopic = topic;
            source.mConditions = info.mSelects;
            source.mSpeaker = info.mActor;
            source.mFaction = info.mFaction;
            source.mCell = info.mCell;
            source.mRank = info.mData.mRank;
            // A line that names a rank but no faction for the player means the speaker's faction.
            source.mPlayerFaction
                = info.mPcFaction.empty() && info.mData.mPCrank != -1 ? info.mFaction : info.mPcFaction;
            source.mPlayerRank = info.mData.mPCrank;
            source.mTaken = taken;
            mSources[quest].push_back(std::move(source));
            if (!info.mActor.empty())
                mSpeakers[quest].insert(info.mActor);
        }
        std::vector<ESM::RefId> speaker;
        if (!info.mActor.empty())
            speaker.push_back(info.mActor);
        addCompanions(info.mResultScript, speaker, mCompanions);
    }

    void Index::learnTopics(const TopicLearning& learning)
    {
        for (auto& [quest, sources] : mSources)
            for (Source& source : sources)
                if (!source.mTopic.empty())
                    source.mLearnedAt = learning.learnedAt(source.mTopic, quest);
    }

    void Index::addScript(std::string_view text, const std::vector<Carrier>& carriers)
    {
        std::vector<ESM::RefId> actors;
        for (const Carrier& carrier : carriers)
            if (carrier.mActor)
                actors.push_back(carrier.mId);
        addCompanions(text, actors, mCompanions);
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

        const ESM::RefId gold = ESM::RefId::stringRefId("gold_001");
        for (const ESM::DialogueCondition& condition : source.mConditions)
        {
            const int count = countNeeded(condition);
            if (count <= 0)
                continue;
            const ESM::RefId id = ESM::RefId::stringRefId(condition.mVariable);
            if (condition.mFunction == ESM::DialogueCondition::Function_Dead)
                objectives.push_back(Objective{ .mTask = Task::Kill, .mObject = id, .mCount = count });
            else if (condition.mFunction == ESM::DialogueCondition::Function_Item && id != gold)
                objectives.push_back(Objective{ .mTask = Task::Fetch, .mObject = id, .mCount = count });
        }
        // A line can take an item from the player without a condition that the player has it, such as Therana
        // taking Auriel's Bow.
        for (const auto& [item, count] : source.mTaken)
        {
            const bool known = std::any_of(objectives.begin(), objectives.end(), [&](const Objective& objective) {
                return objective.mTask == Task::Fetch && objective.mObject == item;
            });
            if (!known && item != gold)
                objectives.push_back(Objective{ .mTask = Task::Fetch, .mObject = item, .mCount = count });
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

    bool marksTheDead(Task task)
    {
        return task == Task::Visit || task == Task::Fetch;
    }

    bool playerRankAllows(const Way& way, const std::map<ESM::RefId, int>& ranks)
    {
        if (way.mPlayerFaction.empty())
            return true;
        const auto it = ranks.find(way.mPlayerFaction);
        return it != ranks.end() && it->second >= way.mPlayerRank;
    }

    std::vector<Way> chooseWays(std::vector<Way> ways, Rule rule)
    {
        const auto setAside = [](const Way& way) { return way.mFailure || way.mOutOfReach; };
        if (!std::all_of(ways.begin(), ways.end(), setAside))
            std::erase_if(ways, setAside);
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
            if (source.mToStage <= stage || source.mLearnedAt > stage)
                continue;
            if (!allowsStage(source.mConditions, quest, stage))
                continue;
            Way way{ source.mToStage, objectivesOf(quest, source) };
            if (way.mObjectives.empty())
                continue;
            const auto speakers = mSpeakers.find(quest);
            way.mFailure = std::any_of(way.mObjectives.begin(), way.mObjectives.end(), [&](const Objective& objective) {
                return objective.mTask == Task::Kill
                    && (mCompanions.contains(objective.mObject)
                        || (speakers != mSpeakers.end() && speakers->second.contains(objective.mObject)));
            });
            for (const ESM::DialogueCondition& condition : source.mConditions)
            {
                if (condition.mFunction == ESM::DialogueCondition::Function_Choice)
                    way.mChoice = true;
                else if (condition.mFunction == ESM::DialogueCondition::Function_Global
                    || condition.mFunction == ESM::DialogueCondition::Function_Local
                    || (condition.mFunction == ESM::DialogueCondition::Function_Journal
                        && !sameQuest(condition.mVariable, quest))
                    || isGoldCondition(condition))
                    way.mChecks.push_back(condition);
            }
            way.mSpeaker = source.mSpeaker;
            way.mPlayerFaction = source.mPlayerFaction;
            way.mPlayerRank = source.mPlayerFaction.empty() ? -1 : source.mPlayerRank;
            // Lines with other checks are other ways, since the game can leave out one and not the other.
            const bool known = std::any_of(ways.begin(), ways.end(), [&](const Way& other) {
                return other.mToStage == way.mToStage && other.mObjectives == way.mObjectives
                    && other.mChoice == way.mChoice && other.mFailure == way.mFailure
                    && other.mPlayerFaction == way.mPlayerFaction && other.mPlayerRank == way.mPlayerRank
                    && std::equal(other.mChecks.begin(), other.mChecks.end(), way.mChecks.begin(), way.mChecks.end(),
                        [](const ESM::DialogueCondition& l, const ESM::DialogueCondition& r) {
                            return l.mFunction == r.mFunction && l.mVariable == r.mVariable
                                && l.mComparison == r.mComparison && l.mValue == r.mValue;
                        });
            });
            if (!known)
                ways.push_back(std::move(way));
        }
        return ways;
    }

    std::vector<std::string> Index::shapeOf(const ESM::RefId& quest) const
    {
        const auto lower = [](const ESM::RefId& id) { return Misc::StringUtils::lowerCase(id.getRefIdString()); };
        std::vector<std::string> facts;
        const auto it = mSources.find(quest);
        if (it == mSources.end())
            return facts;
        for (const Source& source : it->second)
        {
            std::string fact = source.mDialogue ? "line|" + Misc::StringUtils::lowerCase(source.mTopic) + "|"
                    + lower(source.mSpeaker) + "|" + lower(source.mFaction) + "|" + lower(source.mCell)
                                                : std::string("script|");
            if (!source.mDialogue)
            {
                std::vector<std::string> carriers;
                for (const Carrier& carrier : source.mCarriers)
                    carriers.push_back(lower(carrier.mId));
                std::sort(carriers.begin(), carriers.end());
                for (const std::string& carrier : carriers)
                    fact += carrier + ",";
            }
            fact += "|" + std::to_string(source.mToStage);
            std::vector<std::string> conditions;
            for (const ESM::DialogueCondition& condition : source.mConditions)
                if (condition.mFunction == ESM::DialogueCondition::Function_Journal
                    && sameQuest(condition.mVariable, quest))
                    conditions.push_back(std::string(1, static_cast<char>(condition.mComparison))
                        + std::to_string(static_cast<int>(valueOf(condition))));
            std::sort(conditions.begin(), conditions.end());
            for (const std::string& condition : conditions)
                fact += "|" + condition;
            facts.push_back(std::move(fact));
        }
        std::sort(facts.begin(), facts.end());
        return facts;
    }

    std::set<int> Index::stagesSet(const ESM::RefId& quest) const
    {
        std::set<int> stages;
        const auto it = mSources.find(quest);
        if (it != mSources.end())
            for (const Source& source : it->second)
                stages.insert(source.mToStage);
        return stages;
    }

    Index Index::build(const MWWorld::ESMStore& store)
    {
        Index index;
        TopicLearning learning;
        for (const ESM::Dialogue& dialogue : store.get<ESM::Dialogue>())
        {
            if (dialogue.mType == ESM::Dialogue::Journal)
                continue;
            const bool topic = dialogue.mType == ESM::Dialogue::Topic;
            const std::vector<ESM::DialInfo> lines
                = withOfferingConditions(std::vector<ESM::DialInfo>(dialogue.mInfo.begin(), dialogue.mInfo.end()));
            for (const ESM::DialInfo& info : lines)
            {
                index.addDialogue(info, topic ? std::string_view(dialogue.mStringId) : std::string_view());
                if (topic && !journalChanges(info.mResultScript).empty())
                    learning.addTopic(dialogue.mStringId);
            }
        }
        // Only the topics noted above matter, so the lines are looked at once all are noted.
        for (const ESM::Dialogue& dialogue : store.get<ESM::Dialogue>())
            if (dialogue.mType != ESM::Dialogue::Journal)
                for (const ESM::DialInfo& info : dialogue.mInfo)
                    learning.addLine(info.mResponse, info.mResultScript, info.mSelects);
        for (const ESM::Script& script : store.get<ESM::Script>())
            learning.addScript(script.mScriptText);
        index.learnTopics(learning);

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
