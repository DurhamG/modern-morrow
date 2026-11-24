#include "questsheets.hpp"

#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>

#include <components/esm3/loadcell.hpp>
#include <components/esm3/loaddial.hpp>
#include <components/esm3/loadinfo.hpp>
#include <components/esm3/loadscpt.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/misc/strings/lower.hpp>
#include <components/resource/resourcesystem.hpp>

#include "../mwbase/environment.hpp"

#include "../mwgui/questcorrections.hpp"
#include "../mwgui/questreview.hpp"
#include "../mwgui/questtargets.hpp"

#include "../mwworld/esmstore.hpp"

namespace MWAutomation
{
    namespace
    {
        using namespace MWGui;

        const char* taskName(QuestTargets::Task task)
        {
            switch (task)
            {
                case QuestTargets::Task::Talk:
                    return "talk";
                case QuestTargets::Task::Kill:
                    return "kill";
                case QuestTargets::Task::Fetch:
                    return "fetch";
                case QuestTargets::Task::Visit:
                    return "visit";
            }
            return "";
        }

        std::string comparison(ESM::DialogueCondition::Comparison comparison)
        {
            switch (comparison)
            {
                case ESM::DialogueCondition::Comp_Eq:
                    return "==";
                case ESM::DialogueCondition::Comp_Ne:
                    return "!=";
                case ESM::DialogueCondition::Comp_Gt:
                    return ">";
                case ESM::DialogueCondition::Comp_Ge:
                    return ">=";
                case ESM::DialogueCondition::Comp_Ls:
                    return "<";
                case ESM::DialogueCondition::Comp_Le:
                    return "<=";
                default:
                    return "?";
            }
        }

        // A condition of a line, such as "Journal A1_2_AntabolisInformant < 1". Functions without a variable are
        // named by the few that matter here, or by their number.
        std::string conditionText(const ESM::DialogueCondition& condition)
        {
            using C = ESM::DialogueCondition;
            std::string name;
            switch (condition.mFunction)
            {
                case C::Function_Global:
                    name = "Global";
                    break;
                case C::Function_Local:
                    name = "Local";
                    break;
                case C::Function_Journal:
                    name = "Journal";
                    break;
                case C::Function_Item:
                    name = "Item";
                    break;
                case C::Function_Dead:
                    name = "Dead";
                    break;
                case C::Function_NotId:
                    name = "NotId";
                    break;
                case C::Function_NotFaction:
                    name = "NotFaction";
                    break;
                case C::Function_NotClass:
                    name = "NotClass";
                    break;
                case C::Function_NotRace:
                    name = "NotRace";
                    break;
                case C::Function_NotCell:
                    name = "NotCell";
                    break;
                case C::Function_NotLocal:
                    name = "NotLocal";
                    break;
                case C::Function_Choice:
                    name = "Choice";
                    break;
                case C::Function_RankRequirement:
                    name = "RankRequirement";
                    break;
                case C::Function_PcLevel:
                    name = "PcLevel";
                    break;
                case C::Function_PcExpelled:
                    name = "PcExpelled";
                    break;
                case C::Function_SameFaction:
                    name = "SameFaction";
                    break;
                case C::Function_PcReputation:
                    name = "PcReputation";
                    break;
                case C::Function_TalkedToPc:
                    name = "TalkedToPc";
                    break;
                default:
                    name = "Function" + std::to_string(static_cast<int>(condition.mFunction));
                    break;
            }
            if (!condition.mVariable.empty())
                name += " " + condition.mVariable;
            const std::string value = std::visit([](auto v) { return std::to_string(v); }, condition.mValue);
            return name + " " + comparison(condition.mComparison) + " " + value;
        }

        // The cells an object is placed in, by name, separated by semicolons.
        std::string cellsOf(const MWWorld::ESMStore& store, const ESM::RefId& id)
        {
            std::string result;
            for (const ESM::Cell* cell : store.getRefCells(id))
            {
                result += result.empty() ? "" : "; ";
                result += cell->isExterior() && cell->mName.empty()
                    ? std::to_string(cell->getGridX()) + "," + std::to_string(cell->getGridY())
                    : cell->mName;
            }
            return result;
        }

        // Where an objective's target is placed, by the names of the cells, or nothing. An item that isn't placed
        // anywhere can be carried, such as by a creature or in a container, and the markers then mark the holder.
        std::string placeOf(
            const MWWorld::ESMStore& store, const QuestTargets::Index& index, const QuestTargets::Objective& objective)
        {
            if (objective.mObject.empty())
                return {};
            const int count = store.getRefCount(objective.mObject);
            if (count == 0)
            {
                std::string holders;
                for (const ESM::RefId& holder : index.holdersOf(objective.mObject))
                {
                    holders += holders.empty() ? "; carried by " : ", ";
                    holders += holder.getRefIdString();
                    const std::string cells = cellsOf(store, holder);
                    holders += cells.empty() ? " (not placed)" : " in " + cells;
                }
                return " (not placed" + holders + ")";
            }
            std::string result = " (" + cellsOf(store, objective.mObject);
            if (count > 1)
                result += ", placed " + std::to_string(count) + " times";
            return result + ")";
        }

        std::string rangesText(const std::vector<QuestCorrections::Range>& ranges)
        {
            std::string result;
            for (const QuestCorrections::Range& range : ranges)
            {
                if (!result.empty())
                    result += ", ";
                result += std::to_string(range.mFirst);
                if (range.mLast != range.mFirst)
                    result += "-" + std::to_string(range.mLast);
            }
            return result;
        }

        std::string targetsText(const std::set<std::string>& targets)
        {
            std::string result;
            for (const std::string& target : targets)
                result += (result.empty() ? "" : ", ") + target;
            return result.empty() ? "nothing" : result;
        }

        // A block of the game's text, quoted, one line of the sheet per line of the text.
        void quote(std::ostream& out, std::string_view text)
        {
            std::istringstream lines{ std::string(text) };
            std::string line;
            while (std::getline(lines, line))
            {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                out << "> " << line << "\n";
            }
        }

        void writeSheet(std::ostream& out, const MWWorld::ESMStore& store, const QuestTargets::Index& index,
            const QuestCorrections::Corrections& corrections, const ESM::Dialogue& quest)
        {
            std::string name;
            for (const ESM::DialInfo& info : quest.mInfo)
                if (info.mQuestStatus == ESM::DialInfo::QS_Name)
                    name = info.mResponse;
            out << "# " << quest.mStringId << (name.empty() ? "" : ": " + name) << "\n\n";
            out << "What the markers show is worked out by the default rule, with the stage's correction, before the "
                   "checks made while playing. A way's checks are the conditions only the running game can test.\n\n";

            QuestReview::Record record;
            record.mQuest = quest.mId;
            out << "## Stages\n\n";
            // The journal's stages, and those that a line or script sets without a journal entry.
            std::map<int, const ESM::DialInfo*> stages;
            for (const int stage : index.stagesSet(quest.mId))
                stages[stage] = nullptr;
            for (const ESM::DialInfo& info : quest.mInfo)
                if (info.mQuestStatus != ESM::DialInfo::QS_Name)
                    stages[info.mData.mJournalIndex] = &info;
            for (const auto& [stage, info] : stages)
            {
                const bool finished = info != nullptr && info->mQuestStatus == ESM::DialInfo::QS_Finished;
                out << "### " << stage << (finished ? " (finishes the quest)" : "")
                    << (info == nullptr ? " (no journal entry, but a line or script sets it)" : "") << "\n\n";
                if (info != nullptr)
                {
                    quote(out, info->mResponse);
                    out << "\n";
                }
                if (finished)
                    continue;
                const QuestReview::Shown shown
                    = QuestReview::shownAt(index, corrections, quest.mId, stage, QuestTargets::Rule::LowestAndHighest);
                record.mStages.push_back(QuestReview::Stage{ stage, {}, shown });
                out << "Shows now: " << targetsText(shown.mMarked);
                if (!shown.mOptional.empty())
                    out << ". Optional: " << targetsText(shown.mOptional);
                out << ".\n\n";
                if (const QuestCorrections::Entry* entry = corrections.find(quest.mId, stage))
                {
                    out << "Correction for stages " << rangesText(entry->mStages) << ":";
                    if (!entry->mNext.empty())
                        out << " next " << rangesText(entry->mNext) << ";";
                    if (!entry->mOptional.empty())
                        out << " optional " << rangesText(entry->mOptional) << ";";
                    if (!entry->mHide.empty())
                        out << " hide " << rangesText(entry->mHide) << ";";
                    for (const QuestCorrections::Target& target : entry->mTargets)
                        out << " target " << taskName(target.mTask) << " "
                            << (target.mObject.empty() ? target.mCell : target.mObject).getRefIdString() << ";";
                    for (const ESM::RefId& followed : entry->mFollow)
                        out << " follows " << followed.getRefIdString() << ";";
                    out << "\n\n";
                }
                const std::vector<QuestTargets::Way> ways = index.waysForward(quest.mId, stage);
                if (ways.empty())
                    out << "No ways forward.\n\n";
                for (const QuestTargets::Way& way : ways)
                {
                    out << "- to " << way.mToStage << ":";
                    for (const QuestTargets::Objective& objective : way.mObjectives)
                        out << " " << taskName(objective.mTask) << " " << QuestReview::targetOf(objective)
                            << placeOf(store, index, objective) << ";";
                    if (way.mChoice)
                        out << " an answer in a conversation;";
                    if (way.mFailure)
                        out << " a failure;";
                    for (const ESM::DialogueCondition& check : way.mChecks)
                        out << " check " << conditionText(check) << ";";
                    out << "\n";
                }
                out << "\n";
            }

            out << "## Lines that move the quest on\n\n";
            for (const ESM::Dialogue& dialogue : store.get<ESM::Dialogue>())
            {
                if (dialogue.mType == ESM::Dialogue::Journal)
                    continue;
                for (const ESM::DialInfo& info : dialogue.mInfo)
                {
                    const auto changes = QuestTargets::journalChanges(info.mResultScript);
                    if (std::none_of(changes.begin(), changes.end(),
                            [&](const auto& change) { return change.first == quest.mId; }))
                        continue;
                    out << "### Topic \"" << dialogue.mStringId << "\"";
                    if (!info.mActor.empty())
                        out << ", said by " << info.mActor.getRefIdString();
                    if (!info.mFaction.empty())
                        out << ", by the faction " << info.mFaction.getRefIdString() << " at rank "
                            << static_cast<int>(info.mData.mRank) << " or higher";
                    if (!info.mCell.empty())
                        out << ", in " << info.mCell.getRefIdString();
                    out << "\n\n";
                    for (const ESM::DialogueCondition& condition : info.mSelects)
                        out << "- " << conditionText(condition) << "\n";
                    if (!info.mSelects.empty())
                        out << "\n";
                    quote(out, info.mResponse);
                    out << "\n```\n" << info.mResultScript << "\n```\n\n";
                }
            }

            out << "## Scripts that move the quest on\n\n";
            for (const ESM::Script& script : store.get<ESM::Script>())
            {
                const auto changes = QuestTargets::journalChanges(script.mScriptText);
                if (std::none_of(
                        changes.begin(), changes.end(), [&](const auto& change) { return change.first == quest.mId; }))
                    continue;
                out << "### " << script.mId.getRefIdString() << "\n\n```\n" << script.mScriptText << "\n```\n\n";
            }

            out << "## Review record\n\nWhat the markers show now, for each stage that doesn't finish the quest. Add a "
                   "verdict to each stage judged, and save it as ui-rework/quests/reviewed/"
                << quest.mStringId << ".yaml.\n\n```yaml\n"
                << QuestReview::writeRecord(record) << "```\n";
        }
    }

    std::size_t writeQuestSheets(const std::filesystem::path& dir, const std::vector<std::string>& prefixes)
    {
        const MWWorld::ESMStore& store = *MWBase::Environment::get().getESMStore();
        const QuestTargets::Index index = QuestTargets::Index::build(store);
        const QuestCorrections::Corrections corrections
            = QuestCorrections::load(*MWBase::Environment::get().getResourceSystem()->getVFS(), store, index);
        std::filesystem::create_directories(dir);
        std::size_t written = 0;
        for (const ESM::Dialogue& dialogue : store.get<ESM::Dialogue>())
        {
            if (dialogue.mType != ESM::Dialogue::Journal)
                continue;
            if (!prefixes.empty() && std::none_of(prefixes.begin(), prefixes.end(), [&](const std::string& prefix) {
                    return Misc::StringUtils::ciStartsWith(dialogue.mStringId, prefix);
                }))
                continue;
            std::ofstream stream(dir / (Misc::StringUtils::lowerCase(dialogue.mStringId) + ".md"));
            if (!stream)
                throw std::runtime_error("can't write the sheets in " + dir.string());
            writeSheet(stream, store, index, corrections, dialogue);
            ++written;
        }
        return written;
    }
}
