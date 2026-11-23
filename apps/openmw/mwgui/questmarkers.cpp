#include "questmarkers.hpp"

#include <algorithm>
#include <chrono>

#include <components/debug/debuglog.hpp>
#include <components/esm3/loadcell.hpp>
#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/journal.hpp"
#include "../mwbase/world.hpp"

#include "../mwdialogue/quest.hpp"

#include "../mwmechanics/creaturestats.hpp"

#include "../mwworld/cellstore.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/containerstore.hpp"
#include "../mwworld/doorroute.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/worldmodel.hpp"

namespace MWGui
{
    namespace
    {
        // How often the journal is looked at, and the markers worked out again, in seconds.
        constexpr float sUpdateInterval = 0.5f;

        // An object placed more often than this is too common to mark, such as a common ingredient.
        constexpr int sMaxPlacements = 8;

        // An exterior cell, for routes that start outdoors.
        const ESM::RefId sOutdoors = ESM::RefId::esm3ExteriorCell(0, 0);

        bool isAlive(const MWWorld::Ptr& ptr)
        {
            if (!ptr.getRefData().isEnabled() || ptr.getCellRef().getCount() <= 0)
                return false;
            return !ptr.getClass().isActor() || !ptr.getClass().getCreatureStats(ptr).isDead();
        }

        std::string objectiveText(QuestTargets::Task task, std::string_view name)
        {
            switch (task)
            {
                case QuestTargets::Task::Talk:
                    return "#{Interface:QuestTalk} " + std::string(name);
                case QuestTargets::Task::Kill:
                    return "#{Interface:QuestKill} " + std::string(name);
                case QuestTargets::Task::Fetch:
                    return "#{Interface:QuestFetch} " + std::string(name);
                case QuestTargets::Task::Visit:
                    return "#{Interface:QuestVisit} " + std::string(name);
            }
            return std::string(name);
        }
    }

    QuestMarkers::QuestMarkers() = default;

    QuestMarkers::~QuestMarkers() = default;

    void QuestMarkers::clear()
    {
        mTracked.clear();
        mStages.clear();
        mJournalSeen = false;
        mLocalMarkers.clear();
        mWorldMarkers.clear();
        mLastPlayerCell = ESM::RefId();
        mTimer = 0.f;
        ++mGeneration;
    }

    bool QuestMarkers::isTracked(const ESM::RefId& quest) const
    {
        return mTracked.contains(quest);
    }

    void QuestMarkers::setTracked(const std::vector<ESM::RefId>& quests, bool tracked)
    {
        for (const ESM::RefId& quest : quests)
        {
            if (tracked)
                mTracked.insert(quest);
            else
                mTracked.erase(quest);
        }
        mTimer = sUpdateInterval;
    }

    void QuestMarkers::update(float dt)
    {
        MWBase::World* world = MWBase::Environment::get().getWorld();
        const MWWorld::Ptr player = world->getPlayerPtr();
        if (player.isEmpty() || player.getCell() == nullptr)
            return;
        // Going through a door moves the markers to the new place's doors at once.
        const ESM::RefId playerCell = player.getCell()->getCell()->getId();
        if (playerCell != mLastPlayerCell)
        {
            mLastPlayerCell = playerCell;
            mTimer = sUpdateInterval;
        }
        mTimer += dt;
        if (mTimer < sUpdateInterval)
            return;
        mTimer = 0.f;
        pollJournal();
        computeMarkers();
    }

    void QuestMarkers::pollJournal()
    {
        MWBase::Journal* journal = MWBase::Environment::get().getJournal();
        const bool autoTrack = Settings::gui().mQuestMarkersAutoTrack;
        for (const auto& [id, quest] : journal->getQuests())
        {
            const int stage = quest.getIndex();
            const auto it = mStages.find(id);
            const bool moved = it == mStages.end() || it->second != stage;
            mStages[id] = stage;
            if (quest.isFinished())
                mTracked.erase(id);
            else if (moved && mJournalSeen && autoTrack)
                mTracked.insert(id);
        }
        if (mJournalSeen)
            return;
        // When a game starts or loads, the quest with the newest entry is tracked.
        mJournalSeen = true;
        if (!autoTrack)
            return;
        const auto& entries = journal->getEntries();
        for (auto it = entries.rbegin(); it != entries.rend(); ++it)
        {
            const auto quest = journal->getQuests().find(it->mTopic);
            if (quest != journal->getQuests().end() && !quest->second.isFinished())
            {
                mTracked.insert(it->mTopic);
                break;
            }
        }
    }

    std::vector<MWWorld::Ptr> QuestMarkers::findPlaced(const ESM::RefId& id) const
    {
        std::vector<MWWorld::Ptr> found;
        const MWWorld::ESMStore& store = *MWBase::Environment::get().getESMStore();
        const int count = store.getRefCount(id);
        if (count == 0 || count > sMaxPlacements)
            return found;
        MWWorld::WorldModel& worldModel = *MWBase::Environment::get().getWorldModel();
        // A single object can have moved, such as a person a script sent elsewhere, and the world model finds it.
        if (count == 1)
        {
            const MWWorld::Ptr ptr = worldModel.getPtrByRefId(id);
            if (!ptr.isEmpty())
                found.push_back(ptr);
            return found;
        }
        for (const ESM::Cell* cell : store.getRefCells(id))
        {
            worldModel.getCell(cell->mId).forEach([&](const MWWorld::Ptr& ptr) {
                if (ptr.getCellRef().getRefId() == id)
                    found.push_back(ptr);
                return true;
            });
        }
        return found;
    }

    void QuestMarkers::addTargets(const ESM::RefId& quest, int stage, std::vector<Target>& targets)
    {
        using QuestTargets::Task;
        const MWWorld::Ptr player = MWBase::Environment::get().getWorld()->getPlayerPtr();
        const QuestTargets::Rule rule
            = Settings::gui().mQuestMarkerRule == 1 ? QuestTargets::Rule::All : QuestTargets::Rule::Lowest;
        for (const QuestTargets::Way& way : mIndex->waysForward(quest, stage, rule))
        {
            // Kills, fetches and visits come first. The talk they lead to is marked once they're done.
            bool before = false;
            for (const QuestTargets::Objective& objective : way.mObjectives)
            {
                if (objective.mObject.empty())
                    continue;
                if (objective.mTask == Task::Talk && before)
                    continue;
                if (objective.mTask == Task::Fetch
                    && player.getClass().getContainerStore(player).count(objective.mObject) >= objective.mCount)
                    continue;
                bool added = false;
                for (const MWWorld::Ptr& ptr : findPlaced(objective.mObject))
                {
                    if (!isAlive(ptr) || ptr.getCell() == nullptr)
                        continue;
                    targets.push_back(Target{ objective.mTask, std::string(ptr.getClass().getName(ptr)),
                        ptr.getRefData().getPosition().asVec3(), ptr.getCell()->getCell()->getId() });
                    added = true;
                }
                if (added && objective.mTask != Task::Talk)
                    before = true;
            }
        }
    }

    void QuestMarkers::computeMarkers()
    {
        std::vector<Marker> local;
        std::vector<Marker> outdoors;
        if (Settings::gui().mQuestMarkers && !mTracked.empty())
        {
            const MWWorld::ESMStore& store = *MWBase::Environment::get().getESMStore();
            if (!mIndex)
            {
                const auto start = std::chrono::steady_clock::now();
                mIndex = QuestTargets::Index::build(store);
                Log(Debug::Info)
                    << "Worked out the quests' ways forward in "
                    << std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start).count()
                    << " ms";
            }
            MWBase::Journal* journal = MWBase::Environment::get().getJournal();
            const std::span<const MWWorld::DoorLink> doors = store.getTeleportDoors();
            for (const ESM::RefId& id : mTracked)
            {
                const auto quest = journal->getQuests().find(id);
                if (quest == journal->getQuests().end())
                    continue;
                std::vector<Target> targets;
                addTargets(id, quest->second.getIndex(), targets);
                const std::string questName(quest->second.getName());
                for (const Target& target : targets)
                {
                    Marker marker{ questName, objectiveText(target.mTask, target.mName), target.mTask, target.mPos,
                        target.mCell, false };
                    // In the player's place, the marker goes on the target, or on the first door on the way to it.
                    if (const auto route = MWWorld::findDoorRoute(doors, mLastPlayerCell, target.mCell))
                    {
                        Marker here = marker;
                        if (!route->empty())
                        {
                            here.mPos = route->front()->mPos;
                            here.mCell = route->front()->mCell;
                            here.mDoor = true;
                        }
                        local.push_back(here);
                    }
                    // Outdoors, the marker goes on the target, or on the door into the place it is in.
                    if (const auto route = MWWorld::findDoorRoute(doors, sOutdoors, target.mCell))
                    {
                        Marker outside = marker;
                        if (!route->empty())
                        {
                            outside.mPos = route->front()->mPos;
                            outside.mCell = route->front()->mCell;
                            outside.mDoor = true;
                        }
                        outdoors.push_back(outside);
                    }
                }
            }
        }
        // Several targets behind one door share its marker.
        const auto dedupe = [](std::vector<Marker>& markers) {
            std::vector<Marker> unique;
            for (Marker& marker : markers)
            {
                const bool known = std::any_of(unique.begin(), unique.end(), [&](const Marker& other) {
                    return other.mDoor && marker.mDoor && other.mQuest == marker.mQuest
                        && (other.mPos - marker.mPos).length2() < 1.f;
                });
                if (!known)
                    unique.push_back(std::move(marker));
            }
            markers = std::move(unique);
        };
        dedupe(local);
        dedupe(outdoors);
        const auto same = [](const std::vector<Marker>& l, const std::vector<Marker>& r) {
            return std::equal(l.begin(), l.end(), r.begin(), r.end(), [](const Marker& a, const Marker& b) {
                return a.mQuest == b.mQuest && a.mObjective == b.mObjective && (a.mPos - b.mPos).length2() < 1.f;
            });
        };
        if (same(local, mLocalMarkers) && same(outdoors, mWorldMarkers))
            return;
        mLocalMarkers = std::move(local);
        mWorldMarkers = std::move(outdoors);
        ++mGeneration;
    }
}
