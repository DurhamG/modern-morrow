#include "questlog.hpp"

#include <algorithm>
#include <charconv>
#include <climits>
#include <map>
#include <optional>
#include <span>
#include <sstream>
#include <string_view>
#include <unordered_map>

#include <MyGUI_LanguageManager.h>

#include <components/esm3/loadcell.hpp>
#include <components/esm3/loadcrea.hpp>
#include <components/esm3/loaddial.hpp>
#include <components/esm3/loadfact.hpp>
#include <components/esm3/loadnpc.hpp>
#include <components/esm3/loadregn.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/misc/strings/lower.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/journal.hpp"
#include "../mwbase/world.hpp"

#include "../mwdialogue/journalentry.hpp"
#include "../mwdialogue/quest.hpp"

#include "../mwworld/datetimemanager.hpp"
#include "../mwworld/esmstore.hpp"

#include "controllerscreen.hpp"

namespace MWGui::QuestLog
{
    namespace
    {
        const MWWorld::ESMStore& store()
        {
            return *MWBase::Environment::get().getESMStore();
        }

        // A conversation line that moves a quest to a stage, and who says it.
        struct ConversationSource
        {
            int mStage = INT_MAX;
            ESM::RefId mActor;
            ESM::RefId mFaction;
            ESM::RefId mCell;
        };

        // For each quest, the conversation line that sets its earliest stage. Content can't change while the game
        // runs, so this is worked out once.
        std::optional<std::unordered_map<ESM::RefId, ConversationSource>> sConversationSources;

        const std::unordered_map<ESM::RefId, ConversationSource>& conversationSources()
        {
            if (sConversationSources)
                return *sConversationSources;
            std::unordered_map<ESM::RefId, ConversationSource>& result = sConversationSources.emplace();
            for (const ESM::Dialogue& dialogue : store().get<ESM::Dialogue>())
            {
                if (dialogue.mType == ESM::Dialogue::Journal)
                    continue;
                for (const ESM::DialInfo& info : dialogue.mInfo)
                {
                    if (info.mResultScript.empty()
                        || (info.mActor.empty() && info.mFaction.empty() && info.mCell.empty()))
                        continue;
                    for (const auto& [quest, stage] : journalCommands(info.mResultScript))
                    {
                        ConversationSource& source = result[quest];
                        if (stage <= 0 || stage >= source.mStage)
                            continue;
                        source = ConversationSource{ stage, info.mActor, info.mFaction, info.mCell };
                    }
                }
            }
            return result;
        }

        std::string regionName(const ESM::RefId& id)
        {
            const ESM::Region* region = store().get<ESM::Region>().search(id);
            return region != nullptr ? region->mName : std::string();
        }

        // The town a place belongs to. Interiors are named after their town, such as "Balmora, Guild of Fighters"
        // or "Mournhold Temple: Reception Area". Exteriors without a name belong to their region.
        void describeCell(const ESM::Cell& cell, std::string& place, std::string& town)
        {
            if (cell.isExterior() && cell.mName.empty())
            {
                place = regionName(cell.mRegion);
                town = place;
                return;
            }
            place = cell.mName;
            town = townOf(place);
        }

        // A conversation can be limited to cells whose names start with some text, such as "Balmora".
        void describeCellFilter(const ESM::RefId& filter, std::string& place, std::string& town)
        {
            const std::string name = filter.getRefIdString();
            if (const ESM::Cell* cell = store().get<ESM::Cell>().search(name))
            {
                describeCell(*cell, place, town);
                return;
            }
            const MWWorld::Store<ESM::Cell>& cells = store().get<ESM::Cell>();
            for (auto it = cells.extBegin(); it != cells.extEnd(); ++it)
                if (Misc::StringUtils::ciEqual(it->mName, name))
                {
                    describeCell(*it, place, town);
                    return;
                }
            for (auto it = cells.intBegin(); it != cells.intEnd(); ++it)
                if (Misc::StringUtils::ciStartsWith(it->mName, name))
                {
                    // The filter is the start of the name, so it is spelled as in the cell's name.
                    place = it->mName.substr(0, name.size());
                    town = townOf(place);
                    return;
                }
        }

        // Guild and House quests have the faction in their name, such as "Fighters Guild: Exterminator" or "House
        // Hlaalu: The Twin Lamps", where the faction is Great House Hlaalu.
        ESM::RefId factionFromName(std::string_view name)
        {
            const std::size_t colon = name.find(':');
            if (colon == std::string_view::npos)
                return {};
            const std::string_view prefix = name.substr(0, colon);
            for (const ESM::Faction& faction : store().get<ESM::Faction>())
            {
                const std::string_view factionName = faction.mName;
                if (Misc::StringUtils::ciEqual(factionName, prefix)
                    || (factionName.size() > prefix.size() + 1 && Misc::StringUtils::ciEndsWith(factionName, prefix)
                        && factionName[factionName.size() - prefix.size() - 1] == ' '))
                    return faction.mId;
            }
            return {};
        }

        // Whether a journal ID without a quest name is a note. A note's ID has no entry that finishes it in the game's
        // data, so as a quest it would stay in progress forever. Most have a single entry, such as a faction's rules.
        bool isNote(const ESM::RefId& id)
        {
            const ESM::Dialogue* dialogue = store().get<ESM::Dialogue>().search(id);
            if (dialogue == nullptr)
                return false;
            return std::none_of(dialogue->mInfo.begin(), dialogue->mInfo.end(),
                [](const ESM::DialInfo& info) { return info.mQuestStatus == ESM::DialInfo::QS_Finished; });
        }

        void findSource(Quest& quest)
        {
            // The entries recorded since the game started recording the actor and cell come first.
            for (const MWDialogue::StampedJournalEntry* entry : quest.mEntries)
            {
                if (quest.mGiver.empty() && !entry->mActor.empty())
                    quest.mGiver = entry->mActor;
                if (quest.mPlace.empty() && !entry->mCell.empty())
                    if (const ESM::Cell* cell = store().get<ESM::Cell>().search(entry->mCell))
                        describeCell(*cell, quest.mPlace, quest.mTown);
            }

            // Otherwise, the conversation that sets the earliest stage of any of the quest's IDs.
            ConversationSource source;
            for (const MWDialogue::StampedJournalEntry* entry : quest.mEntries)
            {
                const auto& sources = conversationSources();
                const auto it = sources.find(entry->mTopic);
                if (it != sources.end() && it->second.mStage < source.mStage)
                    source = it->second;
            }
            if (quest.mGiver.empty())
                quest.mGiver = source.mActor;
            if (quest.mPlace.empty() && !quest.mGiver.empty())
            {
                const std::span<const ESM::Cell* const> cells = store().getRefCells(quest.mGiver);
                if (!cells.empty())
                    describeCell(*cells.front(), quest.mPlace, quest.mTown);
            }
            if (quest.mPlace.empty() && !source.mCell.empty())
                describeCellFilter(source.mCell, quest.mPlace, quest.mTown);

            quest.mFaction = factionFromName(quest.mName);
            if (quest.mFaction.empty())
                if (const ESM::NPC* npc = store().get<ESM::NPC>().search(quest.mGiver))
                    quest.mFaction = npc->mFaction;
            if (quest.mFaction.empty())
                quest.mFaction = source.mFaction;
        }
    }

    std::vector<std::pair<ESM::RefId, int>> journalCommands(std::string_view script)
    {
        std::vector<std::pair<ESM::RefId, int>> commands;
        std::istringstream lines{ Misc::StringUtils::lowerCase(script) };
        std::string line;
        while (std::getline(lines, line))
        {
            line = line.substr(0, line.find(';'));
            std::replace(line.begin(), line.end(), ',', ' ');
            std::replace(line.begin(), line.end(), '"', ' ');
            std::istringstream words{ line };
            std::vector<std::string> tokens;
            for (std::string word; words >> word;)
                tokens.push_back(std::move(word));
            for (std::size_t i = 0; i + 2 < tokens.size(); ++i)
            {
                const std::string_view command = tokens[i];
                if (command != "journal" && !command.ends_with("->journal"))
                    continue;
                int stage = 0;
                const std::string& number = tokens[i + 2];
                if (std::from_chars(number.data(), number.data() + number.size(), stage).ec == std::errc())
                    commands.emplace_back(ESM::RefId::stringRefId(tokens[i + 1]), stage);
            }
        }
        return commands;
    }

    std::string townOf(std::string_view place)
    {
        return std::string(place.substr(0, place.find_first_of(",:")));
    }

    std::string untitledName(std::string text)
    {
        constexpr std::size_t maxLength = 48;
        const std::size_t sentenceEnd = text.find_first_of(".!?");
        if (sentenceEnd != std::string::npos)
            text.resize(sentenceEnd);
        if (text.size() > maxLength)
        {
            const std::size_t space = text.rfind(' ', maxLength);
            text = text.substr(0, space != std::string::npos ? space : maxLength) + "...";
        }
        return text;
    }

    void resetContentCache()
    {
        sConversationSources.reset();
    }

    std::vector<Quest> collect()
    {
        const MWBase::Journal& journal = *MWBase::Environment::get().getJournal();
        const MWBase::Journal::TQuestContainer& questIds = journal.getQuests();

        std::vector<Quest> quests;
        std::map<std::string, std::size_t> byName;
        const MWBase::Journal::TEntryContainer& entries = journal.getEntries();
        for (std::size_t i = 0; i < entries.size(); ++i)
        {
            const MWDialogue::StampedJournalEntry& entry = entries[i];
            const auto questIt = questIds.find(entry.mTopic);
            std::string name = questIt != questIds.end() ? std::string(questIt->second.getName()) : std::string();
            // Quests without a name are kept apart by their ID.
            const std::string key
                = name.empty() ? "#" + entry.mTopic.toDebugString() : Misc::StringUtils::lowerCase(name);
            auto [it, inserted] = byName.emplace(key, quests.size());
            if (inserted)
            {
                quests.emplace_back();
                quests.back().mNote = name.empty() && isNote(entry.mTopic);
                quests.back().mName = name.empty() ? untitledName(entry.getText()) : std::move(name);
            }
            Quest& quest = quests[it->second];
            quest.mEntries.push_back(&entry);
            quest.mLastActivity = i;
        }

        // A quest is finished when any of its IDs is, as in the journal book. It is pinned or archived when any of its
        // IDs is.
        for (const auto& [id, questId] : questIds)
        {
            const std::string_view name = questId.getName();
            const auto it = byName.find(name.empty() ? "#" + id.toDebugString() : Misc::StringUtils::lowerCase(name));
            if (it == byName.end())
                continue;
            Quest& quest = quests[it->second];
            quest.mIds.push_back(id);
            quest.mFinished = quest.mFinished || questId.isFinished();
            quest.mPinned = quest.mPinned || questId.isPinned();
            quest.mArchived = quest.mArchived || questId.isArchived();
        }

        for (Quest& quest : quests)
            findSource(quest);
        std::sort(quests.begin(), quests.end(),
            [](const Quest& a, const Quest& b) { return a.mLastActivity > b.mLastActivity; });
        return quests;
    }

    void setPinned(Quest& quest, bool pinned)
    {
        MWBase::Journal& journal = *MWBase::Environment::get().getJournal();
        for (const ESM::RefId& id : quest.mIds)
            if (MWDialogue::Quest* questId = journal.getQuestOrNull(id))
            {
                questId->setPinned(pinned);
                if (pinned)
                    questId->setArchived(false);
            }
        quest.mPinned = pinned;
        if (pinned)
            quest.mArchived = false;
    }

    void setArchived(Quest& quest, bool archived)
    {
        MWBase::Journal& journal = *MWBase::Environment::get().getJournal();
        for (const ESM::RefId& id : quest.mIds)
            if (MWDialogue::Quest* questId = journal.getQuestOrNull(id))
            {
                questId->setArchived(archived);
                if (archived)
                    questId->setPinned(false);
            }
        quest.mArchived = archived;
        if (archived)
            quest.mPinned = false;
    }

    std::string giverName(const Quest& quest)
    {
        if (const ESM::NPC* npc = store().get<ESM::NPC>().search(quest.mGiver))
            return npc->mName;
        if (const ESM::Creature* creature = store().get<ESM::Creature>().search(quest.mGiver))
            return creature->mName;
        return {};
    }

    std::string factionName(const Quest& quest)
    {
        const ESM::Faction* faction = store().get<ESM::Faction>().search(quest.mFaction);
        return faction != nullptr ? faction->mName : std::string();
    }

    std::string timestamp(const MWDialogue::StampedJournalEntry& entry)
    {
        const std::string day = MyGUI::LanguageManager::getInstance().replaceTags("#{sDay}");
        const std::string_view month
            = MWBase::Environment::get().getWorld()->getTimeManager()->getMonthName(entry.mMonth);
        return std::to_string(entry.mDayOfMonth) + " " + std::string(month) + " (" + day + " "
            + std::to_string(entry.mDay) + ")";
    }

    std::string groupOf(const Quest& quest, Tab tab)
    {
        if (quest.mArchived)
            return "#{Interface:JournalArchived}";
        if (quest.mPinned)
            return "#{Interface:JournalPinned}";
        if (quest.mFinished)
            return "#{Interface:JournalFinished}";
        switch (tab)
        {
            case Tab::Places:
                return quest.mTown.empty() ? "#{Interface:JournalUnknownPlace}" : ControllerUi::escaped(quest.mTown);
            case Tab::Factions:
            {
                const std::string faction = factionName(quest);
                return faction.empty() ? "#{Interface:JournalNoFaction}" : ControllerUi::escaped(faction);
            }
            case Tab::Notes:
                return "#{Interface:JournalTabNotes}";
            default:
                return "#{Interface:JournalInProgress}";
        }
    }

    std::vector<int> order(const std::vector<Quest>& quests, Tab tab)
    {
        // The quests are already in order of the most recent activity. Pinned quests come first, then quests in
        // progress, then finished quests, then archived ones. On the Places and Factions tabs, the quests in progress
        // are grouped by name, with quests that have no place or faction after the rest. The Notes tab lists only the
        // notes, and the other tabs only the quests, apart from pinned notes, which stay in reach on every tab.
        std::vector<int> result;
        for (std::size_t i = 0; i < quests.size(); ++i)
        {
            const Quest& quest = quests[i];
            if (tab == Tab::Notes ? quest.mNote : !quest.mNote || quest.mPinned)
                result.push_back(static_cast<int>(i));
        }
        const auto groupKey = [&](const Quest& quest) -> std::pair<int, std::string> {
            if (quest.mArchived)
                return { 3, {} };
            if (quest.mPinned)
                return { -1, {} };
            if (quest.mFinished)
                return { 2, {} };
            if (tab == Tab::Places)
                return { quest.mTown.empty() ? 1 : 0, Misc::StringUtils::lowerCase(quest.mTown) };
            if (tab == Tab::Factions)
            {
                const std::string faction = factionName(quest);
                return { faction.empty() ? 1 : 0, Misc::StringUtils::lowerCase(faction) };
            }
            return { 0, {} };
        };
        std::stable_sort(
            result.begin(), result.end(), [&](int a, int b) { return groupKey(quests[a]) < groupKey(quests[b]); });
        return result;
    }

    std::vector<Row> buildRows(const std::vector<Quest>& quests, const std::vector<int>& order, Tab tab)
    {
        std::vector<Row> rows;
        std::string group;
        for (std::size_t i = 0; i < order.size(); ++i)
        {
            std::string header = groupOf(quests[order[i]], tab);
            if (rows.empty() || header != group)
            {
                group = header;
                rows.push_back(Row{ std::move(header), -1 });
            }
            rows.push_back(Row{ {}, static_cast<int>(i) });
        }
        return rows;
    }

    std::vector<int> groupStarts(const std::vector<Row>& rows)
    {
        // The first entry under each group header.
        std::vector<int> starts;
        for (std::size_t i = 0; i + 1 < rows.size(); ++i)
            if (rows[i].mEntry == -1 && rows[i + 1].mEntry >= 0)
                starts.push_back(rows[i + 1].mEntry);
        return starts;
    }
}
