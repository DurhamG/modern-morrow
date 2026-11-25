#include "questmarkers.hpp"

#include <algorithm>
#include <chrono>
#include <limits>
#include <optional>

#include <components/debug/debuglog.hpp>
#include <components/esm/util.hpp>
#include <components/esm3/loadcell.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/journal.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwdialogue/quest.hpp"

#include "../mwmechanics/creaturestats.hpp"
#include "../mwmechanics/npcstats.hpp"

#include "../mwscript/locals.hpp"

#include "../mwworld/cellstore.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/containerstore.hpp"
#include "../mwworld/doorroute.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/manualref.hpp"
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

        bool isPresent(const MWWorld::Ptr& ptr)
        {
            return ptr.getRefData().isEnabled() && ptr.getCellRef().getCount() > 0;
        }

        bool isDead(const MWWorld::Ptr& ptr)
        {
            return ptr.getClass().isActor() && ptr.getClass().getCreatureStats(ptr).isDead();
        }

        bool isAlive(const MWWorld::Ptr& ptr)
        {
            return isPresent(ptr) && !isDead(ptr);
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

    void QuestMarkers::refresh()
    {
        const MWWorld::Ptr player = MWBase::Environment::get().getWorld()->getPlayerPtr();
        if (player.isEmpty() || player.getCell() == nullptr)
            return;
        mLastPlayerCell = player.getCell()->getCell()->getId();
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
        // When a game starts or loads, the quest with the newest entry is tracked. The journal shows the quests that
        // share its name as one, such as the steps of a main quest's chapter, so those are tracked too.
        mJournalSeen = true;
        if (!autoTrack)
            return;
        const auto& entries = journal->getEntries();
        for (auto it = entries.rbegin(); it != entries.rend(); ++it)
        {
            const auto quest = journal->getQuests().find(it->mTopic);
            if (quest == journal->getQuests().end() || quest->second.isFinished())
                continue;
            for (const auto& [id, other] : journal->getQuests())
                if (!other.isFinished() && other.getName() == quest->second.getName())
                    mTracked.insert(id);
            break;
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

    std::vector<MWWorld::Ptr> QuestMarkers::nearestMember(const QuestTargets::Objective& objective) const
    {
        const MWWorld::ESMStore& store = *MWBase::Environment::get().getESMStore();
        const std::string cellPrefix = objective.mCell.empty() ? std::string() : objective.mCell.getRefIdString();
        ESM::RefId best;
        std::size_t bestDoors = std::numeric_limits<std::size_t>::max();
        for (const ESM::RefId& member : mIndex->membersOf(objective.mFaction, objective.mRank))
        {
            if (store.getRefCount(member) != 1)
                continue;
            const ESM::Cell* cell = store.getRefCells(member).front();
            // A line can be limited to the cells whose names start with some text, such as "Mournhold".
            if (!cellPrefix.empty() && !Misc::StringUtils::ciStartsWith(cell->mName, cellPrefix))
                continue;
            const auto route = MWWorld::findDoorRoute(store.getTeleportDoors(), mLastPlayerCell, cell->mId);
            if (route && route->size() < bestDoors)
            {
                best = member;
                bestDoors = route->size();
            }
        }
        if (best.empty())
            return {};
        return findPlaced(best);
    }

    std::optional<QuestMarkers::Target> QuestMarkers::placeOfCell(
        const ESM::RefId& cellId, QuestTargets::Task task) const
    {
        const MWWorld::ESMStore& store = *MWBase::Environment::get().getESMStore();
        const std::string name = cellId.getRefIdString();
        if (const ESM::Cell* cell = store.get<ESM::Cell>().search(name); cell != nullptr && !cell->isExterior())
        {
            // The place is where the player comes in, not where a door from one of its own rooms comes out.
            if (const MWWorld::DoorLink* door
                = MWWorld::findDoorInto(store.getTeleportDoors(), mLastPlayerCell, cell->mId))
                return Target{ task, ESM::RefId(), cell->mName, door->mDestPos, cell->mId };
            return std::nullopt;
        }
        // An exterior place, such as "Gnaar Mok", is the middle of its first cell, at the player's height.
        const MWWorld::Store<ESM::Cell>& cells = store.get<ESM::Cell>();
        for (auto it = cells.extBegin(); it != cells.extEnd(); ++it)
        {
            if (!Misc::StringUtils::ciEqual(it->mName, name))
                continue;
            const osg::Vec2f centre = ESM::indexToPosition(
                ESM::ExteriorCellLocation(it->getGridX(), it->getGridY(), ESM::Cell::sDefaultWorldspaceId), true);
            const float height
                = MWBase::Environment::get().getWorld()->getPlayerPtr().getRefData().getPosition().asVec3().z();
            return Target{ task, ESM::RefId(), it->mName, osg::Vec3f(centre.x(), centre.y(), height), it->mId };
        }
        return std::nullopt;
    }

    bool QuestMarkers::addObjectiveTargets(const QuestTargets::Objective& objective, std::vector<Target>& targets)
    {
        using QuestTargets::Task;
        const MWWorld::Ptr player = MWBase::Environment::get().getWorld()->getPlayerPtr();
        if (objective.mTask == Task::Fetch && !objective.mObject.empty()
            && player.getClass().getContainerStore(player).count(objective.mObject) >= objective.mCount)
            return false;
        const auto add = [&](const MWWorld::Ptr& ptr, Task task, std::string name) {
            if (!isPresent(ptr) || (isDead(ptr) && !QuestTargets::marksTheDead(task)) || ptr.getCell() == nullptr)
                return false;
            targets.push_back(Target{ task, ptr.getCellRef().getRefId(), std::move(name),
                ptr.getRefData().getPosition().asVec3(), ptr.getCell()->getCell()->getId() });
            return true;
        };

        bool added = false;
        if (!objective.mObject.empty())
        {
            for (const MWWorld::Ptr& ptr : findPlaced(objective.mObject))
                added |= add(ptr, objective.mTask, std::string(ptr.getClass().getName(ptr)));
            // An item that isn't lying anywhere can be on someone, or in a container.
            if (!added && objective.mTask == Task::Fetch)
            {
                const MWWorld::ESMStore& store = *MWBase::Environment::get().getESMStore();
                if (store.find(objective.mObject) == 0)
                    return false;
                const MWWorld::ManualRef item(store, objective.mObject);
                const std::string itemName(item.getPtr().getClass().getName(item.getPtr()));
                // A holder can be a corpse, such as the one that carries a pipe someone wants back.
                for (const ESM::RefId& holder : mIndex->holdersOf(objective.mObject))
                    for (const MWWorld::Ptr& ptr : findPlaced(holder))
                    {
                        if (!ptr.getRefData().isEnabled() || ptr.getCell() == nullptr
                            || ptr.getClass().getContainerStore(ptr).count(objective.mObject) <= 0)
                            continue;
                        targets.push_back(Target{ Task::Fetch, objective.mObject, itemName,
                            ptr.getRefData().getPosition().asVec3(), ptr.getCell()->getCell()->getId() });
                        added = true;
                    }
            }
        }
        else if (!objective.mFaction.empty())
        {
            for (const MWWorld::Ptr& ptr : nearestMember(objective))
                added |= add(ptr, objective.mTask, std::string(ptr.getClass().getName(ptr)));
        }
        else if (!objective.mCell.empty())
        {
            if (const std::optional<Target> place = placeOfCell(objective.mCell, objective.mTask))
            {
                targets.push_back(*place);
                added = true;
            }
        }
        return added;
    }

    bool QuestMarkers::isOutOfReach(const QuestTargets::Way& way)
    {
        const MWWorld::Ptr player = MWBase::Environment::get().getWorld()->getPlayerPtr();
        std::vector<Target> found;
        for (const QuestTargets::Objective& objective : way.mObjectives)
        {
            if (objective.mObject.empty())
                continue;
            // Someone to talk to who is dead, or gone, such as a person a script disabled.
            if (objective.mTask == QuestTargets::Task::Talk)
            {
                const std::vector<MWWorld::Ptr> placed = findPlaced(objective.mObject);
                if (!placed.empty() && std::none_of(placed.begin(), placed.end(), isAlive))
                    return true;
                continue;
            }
            if (objective.mTask != QuestTargets::Task::Fetch)
                continue;
            if (player.getClass().getContainerStore(player).count(objective.mObject) >= objective.mCount)
                continue;
            if (!addObjectiveTargets(objective, found))
                return true;
        }
        return false;
    }

    QuestCorrections::Progress QuestMarkers::questProgress()
    {
        MWBase::Journal* journal = MWBase::Environment::get().getJournal();
        QuestCorrections::Progress progress;
        progress.mStage = [journal](const ESM::RefId& quest) { return journal->getJournalIndex(quest); };
        progress.mFinished = [journal](const ESM::RefId& quest) {
            const auto it = journal->getQuests().find(quest);
            return it != journal->getQuests().end() && it->second.isFinished();
        };
        progress.mDead = [](const ESM::RefId& id) {
            return MWBase::Environment::get().getMechanicsManager()->countDeaths(id) > 0;
        };
        progress.mGlobal = [](std::string_view name) {
            MWBase::World* world = MWBase::Environment::get().getWorld();
            const std::string variable(name);
            return world->getGlobalVariableType(variable) == ' ' ? 0.f : world->getGlobalFloat(variable);
        };
        progress.mCarried = [](const ESM::RefId& id) {
            const MWWorld::Ptr player = MWBase::Environment::get().getWorld()->getPlayerPtr();
            return player.getClass().getContainerStore(player).count(id);
        };
        return progress;
    }

    void QuestMarkers::addTargets(const ESM::RefId& quest, int stage, std::vector<Target>& targets)
    {
        using QuestTargets::Task;
        MWBase::World* world = MWBase::Environment::get().getWorld();
        MWBase::Journal* journal = MWBase::Environment::get().getJournal();
        QuestTargets::Rule rule = QuestTargets::Rule::Lowest;
        if (Settings::gui().mQuestMarkerRule == 1)
            rule = QuestTargets::Rule::All;
        else if (Settings::gui().mQuestMarkerRule == 2)
            rule = QuestTargets::Rule::LowestAndHighest;
        // A way whose line needs a global variable, another quest's stage, the speaker's local variable or gold that
        // doesn't hold yet isn't one yet, such as a branch for when the player killed someone. So is a line for
        // players of a faction rank the player doesn't have, such as a councilor's line for the house's own members.
        // The rule leaves those out, so the next way along takes their place.
        std::vector<QuestTargets::Way> ways = mIndex->waysForward(quest, stage);
        const MWWorld::Ptr player = world->getPlayerPtr();
        const std::map<ESM::RefId, int>& ranks = player.getClass().getNpcStats(player).getFactionRanks();
        for (QuestTargets::Way& way : ways)
            way.mWaiting = !QuestTargets::playerRankAllows(way, ranks)
                || !std::all_of(way.mChecks.begin(), way.mChecks.end(), [&](const auto& condition) {
                       if (condition.mFunction == ESM::DialogueCondition::Function_Global)
                       {
                           if (world->getGlobalVariableType(condition.mVariable) == ' ')
                               return true;
                           return QuestTargets::compare(condition, world->getGlobalFloat(condition.mVariable));
                       }
                       if (condition.mFunction == ESM::DialogueCondition::Function_Item)
                           return QuestTargets::compare(condition,
                               static_cast<float>(player.getClass().getContainerStore(player).count(
                                   ESM::RefId::stringRefId(condition.mVariable))));
                       if (condition.mFunction == ESM::DialogueCondition::Function_Local)
                       {
                           const std::vector<MWWorld::Ptr> speaker
                               = way.mSpeaker.empty() ? std::vector<MWWorld::Ptr>() : findPlaced(way.mSpeaker);
                           if (speaker.empty())
                               return true;
                           const ESM::RefId script = speaker.front().getClass().getScript(speaker.front());
                           MWScript::Locals& locals = speaker.front().getRefData().getLocals();
                           if (script.empty() || !locals.hasVar(script, condition.mVariable))
                               return true;
                           return QuestTargets::compare(condition, locals.getFloatVar(script, condition.mVariable));
                       }
                       return QuestTargets::compare(condition,
                           static_cast<float>(journal->getJournalIndex(ESM::RefId::stringRefId(condition.mVariable))));
                   });
        for (QuestTargets::Way& way : ways)
            way.mOutOfReach = isOutOfReach(way);
        const QuestCorrections::Entry* entry = mCorrections.find(quest, stage);
        std::vector<QuestTargets::Way> chosen = QuestCorrections::chooseWays(std::move(ways), entry, rule);
        if (entry != nullptr)
        {
            const QuestCorrections::Progress progress = questProgress();
            std::vector<QuestTargets::Way> extra = QuestCorrections::targetWays(*entry, &progress);
            chosen.insert(chosen.end(), std::make_move_iterator(extra.begin()), std::make_move_iterator(extra.end()));
        }
        for (const QuestTargets::Way& way : chosen)
        {
            // Kills, fetches and visits come first. The talk they lead to is marked once they're done.
            const std::size_t first = targets.size();
            bool before = false;
            for (const QuestTargets::Objective& objective : way.mObjectives)
            {
                if (objective.mTask == Task::Talk && before)
                    continue;
                if (addObjectiveTargets(objective, targets) && objective.mTask != Task::Talk)
                    before = true;
            }
            for (std::size_t i = first; i < targets.size(); ++i)
                targets[i].mOptional = way.mOptional;
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
                mCorrections
                    = QuestCorrections::load(*MWBase::Environment::get().getResourceSystem()->getVFS(), store, *mIndex);
                Log(Debug::Info)
                    << "Worked out the quests' ways forward in "
                    << std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start).count()
                    << " ms";
            }
            MWBase::Journal* journal = MWBase::Environment::get().getJournal();
            const std::span<const MWWorld::DoorLink> doors = store.getTeleportDoors();
            // A tracked quest's correction can follow quests that help with it, which are marked as if tracked.
            std::vector<ESM::RefId> quests(mTracked.begin(), mTracked.end());
            const QuestCorrections::Progress progress = questProgress();
            for (const ESM::RefId& id : mTracked)
                if (const QuestCorrections::Entry* entry = mCorrections.find(id, journal->getJournalIndex(id)))
                    for (const ESM::RefId& followed : QuestCorrections::followedQuests(*entry, progress))
                        if (std::find(quests.begin(), quests.end(), followed) == quests.end())
                            quests.push_back(followed);
            for (const ESM::RefId& id : quests)
            {
                const auto quest = journal->getQuests().find(id);
                if (quest == journal->getQuests().end())
                    continue;
                std::vector<Target> targets;
                addTargets(id, quest->second.getIndex(), targets);
                const std::string questName(quest->second.getName());
                for (const Target& target : targets)
                {
                    Marker marker{ id, questName, objectiveText(target.mTask, target.mName), target.mTask, target.mId,
                        target.mCell, target.mPos, target.mCell, false, target.mOptional };
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
        // Several targets behind one door share its marker, which is optional only if they all are. An optional marker
        // in the same place as another marker of the same quest isn't needed.
        const auto dedupe = [](std::vector<Marker>& markers) {
            const auto samePlace = [](const Marker& l, const Marker& r) {
                return l.mQuestId == r.mQuestId && (l.mPos - r.mPos).length2() < 1.f;
            };
            std::vector<Marker> unique;
            for (Marker& marker : markers)
            {
                const auto known = std::find_if(unique.begin(), unique.end(),
                    [&](const Marker& other) { return samePlace(other, marker) && other.mDoor && marker.mDoor; });
                if (known == unique.end())
                    unique.push_back(std::move(marker));
                else if (!marker.mOptional)
                    known->mOptional = false;
            }
            std::erase_if(unique, [&](const Marker& marker) {
                return marker.mOptional && std::any_of(unique.begin(), unique.end(), [&](const Marker& other) {
                    return !other.mOptional && samePlace(other, marker);
                });
            });
            markers = std::move(unique);
        };
        dedupe(local);
        dedupe(outdoors);
        const auto same = [](const std::vector<Marker>& l, const std::vector<Marker>& r) {
            return std::equal(l.begin(), l.end(), r.begin(), r.end(), [](const Marker& a, const Marker& b) {
                return a.mQuest == b.mQuest && a.mObjective == b.mObjective && a.mOptional == b.mOptional
                    && (a.mPos - b.mPos).length2() < 1.f;
            });
        };
        if (same(local, mLocalMarkers) && same(outdoors, mWorldMarkers))
            return;
        mLocalMarkers = std::move(local);
        mWorldMarkers = std::move(outdoors);
        ++mGeneration;
    }
}
