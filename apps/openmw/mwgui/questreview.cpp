#include "questreview.hpp"

#include <algorithm>
#include <sstream>

#include <yaml-cpp/yaml.h>

#include <components/misc/strings/lower.hpp>

namespace MWGui::QuestReview
{
    namespace
    {
        bool readTargets(const YAML::Node& node, std::set<std::string>& targets)
        {
            if (!node)
                return true;
            if (!node.IsSequence())
                return false;
            for (const YAML::Node& item : node)
            {
                if (!item.IsScalar() || item.Scalar().empty())
                    return false;
                targets.insert(Misc::StringUtils::lowerCase(item.Scalar()));
            }
            return true;
        }

        // A list of targets on one line, each in double quotes, as YAML reads it.
        std::string writeTargets(const std::set<std::string>& targets)
        {
            std::string result = "[";
            for (const std::string& target : targets)
            {
                if (result.size() > 1)
                    result += ", ";
                result += '"';
                for (const char c : target)
                {
                    if (c == '"' || c == '\\')
                        result += '\\';
                    result += c;
                }
                result += '"';
            }
            return result + "]";
        }
    }

    std::string targetOf(const QuestTargets::Objective& objective)
    {
        if (!objective.mObject.empty())
            return Misc::StringUtils::lowerCase(objective.mObject.getRefIdString());
        if (!objective.mFaction.empty())
            return "faction:" + Misc::StringUtils::lowerCase(objective.mFaction.getRefIdString());
        if (!objective.mCell.empty())
            return "cell:" + Misc::StringUtils::lowerCase(objective.mCell.getRefIdString());
        return {};
    }

    Shown shownAt(const QuestTargets::Index& index, const QuestCorrections::Corrections& corrections,
        const ESM::RefId& quest, int stage, QuestTargets::Rule rule)
    {
        const QuestCorrections::Entry* entry = corrections.find(quest, stage);
        std::vector<QuestTargets::Way> ways
            = QuestCorrections::chooseWays(index.waysForward(quest, stage), entry, rule);
        if (entry != nullptr)
        {
            QuestCorrections::Progress none;
            none.mStage = [](const ESM::RefId&) { return 0; };
            none.mFinished = [](const ESM::RefId&) { return false; };
            for (QuestTargets::Way& way : QuestCorrections::targetWays(*entry, none))
                ways.push_back(std::move(way));
        }
        Shown shown;
        for (const QuestTargets::Way& way : ways)
            for (const QuestTargets::Objective& objective : way.mObjectives)
            {
                const std::string target = targetOf(objective);
                if (!target.empty())
                    (way.mOptional ? shown.mOptional : shown.mMarked).insert(target);
            }
        for (const std::string& target : shown.mMarked)
            shown.mOptional.erase(target);
        return shown;
    }

    Record parseRecord(const std::string& text, std::vector<std::string>& problems)
    {
        Record record;
        YAML::Node root;
        try
        {
            root = YAML::Load(text);
        }
        catch (const YAML::Exception& e)
        {
            problems.push_back(std::string("the record isn't YAML: ") + e.what());
            return record;
        }
        if (!root.IsMap() || !root["quest"] || !root["quest"].IsScalar())
        {
            problems.emplace_back("the record doesn't name its quest");
            return record;
        }
        record.mQuest = ESM::RefId::stringRefId(root["quest"].Scalar());
        const YAML::Node stages = root["stages"];
        if (!stages)
            return record;
        if (!stages.IsSequence())
        {
            problems.emplace_back("the record's stages aren't a list");
            return record;
        }
        for (const YAML::Node& node : stages)
        {
            Stage stage;
            bool read = node.IsMap() && node["stage"] && node["stage"].IsScalar();
            if (read)
            {
                try
                {
                    stage.mStage = node["stage"].as<int>();
                }
                catch (const YAML::Exception&)
                {
                    read = false;
                }
            }
            for (const auto& field : node.IsMap() ? node : YAML::Node())
            {
                const std::string key = field.first.as<std::string>();
                if (key != "stage" && key != "verdict" && key != "marked" && key != "optional")
                    read = false;
            }
            if (read && node["verdict"])
            {
                stage.mVerdict = node["verdict"].IsScalar() ? node["verdict"].Scalar() : std::string();
                read = stage.mVerdict == "right" || stage.mVerdict == "corrected" || stage.mVerdict == "unfixable";
            }
            read = read && readTargets(node["marked"], stage.mShown.mMarked)
                && readTargets(node["optional"], stage.mShown.mOptional);
            if (!read)
            {
                problems.push_back(record.mQuest.getRefIdString() + " has a stage that can't be read");
                continue;
            }
            record.mStages.push_back(std::move(stage));
        }
        return record;
    }

    std::string writeRecord(const Record& record)
    {
        std::ostringstream stream;
        stream << "quest: " << record.mQuest.getRefIdString() << "\nstages:\n";
        for (const Stage& stage : record.mStages)
        {
            stream << "  - stage: " << stage.mStage << "\n";
            if (!stage.mVerdict.empty())
                stream << "    verdict: " << stage.mVerdict << "\n";
            stream << "    marked: " << writeTargets(stage.mShown.mMarked) << "\n";
            if (!stage.mShown.mOptional.empty())
                stream << "    optional: " << writeTargets(stage.mShown.mOptional) << "\n";
        }
        return stream.str();
    }

    std::vector<Change> compare(const Record& record, const QuestTargets::Index& index,
        const QuestCorrections::Corrections& corrections, QuestTargets::Rule rule)
    {
        std::vector<Change> changes;
        for (const Stage& stage : record.mStages)
        {
            Shown actual = shownAt(index, corrections, record.mQuest, stage.mStage, rule);
            if (actual != stage.mShown)
                changes.push_back(Change{ stage.mStage, stage.mShown, std::move(actual) });
        }
        return changes;
    }
}
