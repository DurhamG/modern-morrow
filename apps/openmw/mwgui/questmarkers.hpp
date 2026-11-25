#ifndef MWGUI_QUESTMARKERS_H
#define MWGUI_QUESTMARKERS_H

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <osg/Vec3f>

#include <components/esm/refid.hpp>

#include "questcorrections.hpp"
#include "questtargets.hpp"

namespace MWWorld
{
    class Ptr;
}

namespace MWGui
{
    /// The quests the player tracks, and where their markers go. A quest that starts or moves on is tracked, as in
    /// Skyrim, and one that finishes stops being tracked. The markers are worked out from the content by QuestTargets,
    /// and placed where the targets are now. See ui-rework/design/quest-markers.md.
    class QuestMarkers
    {
    public:
        struct Marker
        {
            // The quest's name, and what to do there, such as "Talk to Caius Cosades".
            ESM::RefId mQuestId;
            std::string mQuest;
            std::string mObjective;
            QuestTargets::Task mTask = QuestTargets::Task::Talk;
            // What the marker is for: the object, or nothing for a place. And the cell the target is in.
            ESM::RefId mTarget;
            ESM::RefId mTargetCell;
            // Where the marker goes, and the cell that is in. For a target in another place, the marker goes on the
            // first door of the way there.
            osg::Vec3f mPos;
            ESM::RefId mCell;
            bool mDoor = false;
            // Whether the marker is for a side step the player can take or leave. It is drawn hollow and smaller, and
            // the compass leaves it out.
            bool mOptional = false;
        };

        QuestMarkers();
        ~QuestMarkers();

        /// Looks at the journal for quests that moved on, and works out the markers again, a few times a second.
        void update(float dt);

        /// Looks at the journal and works out the markers now, without waiting for the next update.
        void refresh();

        /// Forgets the tracked quests, such as when a game starts or loads. The newest quest is tracked again.
        void clear();

        bool isTracked(const ESM::RefId& quest) const;
        const std::set<ESM::RefId>& getTracked() const { return mTracked; }
        void setTracked(const std::vector<ESM::RefId>& quests, bool tracked);

        /// The markers in the place the player is in: the targets there, and the doors on the way to the others.
        const std::vector<Marker>& getLocalMarkers() const { return mLocalMarkers; }

        /// The markers outdoors, for the world map: the targets outdoors, and the doors into the others.
        const std::vector<Marker>& getWorldMarkers() const { return mWorldMarkers; }

        /// The number the markers have been worked out, so a map knows when to draw them again.
        unsigned int getGeneration() const { return mGeneration; }

    private:
        // A target of an objective, where it is now.
        struct Target
        {
            QuestTargets::Task mTask;
            // The object, or nothing for a place.
            ESM::RefId mId;
            std::string mName;
            osg::Vec3f mPos;
            ESM::RefId mCell;
            bool mOptional = false;
        };

        void pollJournal();
        void computeMarkers();
        void addTargets(const ESM::RefId& quest, int stage, std::vector<Target>& targets);
        // How far the player's quests have got, for the corrections' targets.
        static QuestCorrections::Progress questProgress();
        // Adds where an objective's targets are now. Returns whether there were any.
        bool addObjectiveTargets(
            const QuestTargets::Way& way, const QuestTargets::Objective& objective, std::vector<Target>& targets);
        // Whether a way needs someone to talk to who is dead or gone, or an item that the player doesn't carry and that
        // can't be found anywhere.
        bool isOutOfReach(const QuestTargets::Way& way);
        std::vector<MWWorld::Ptr> findPlaced(const ESM::RefId& id) const;
        // The faction member, of the rank the objective needs, whom the player can reach through the fewest doors.
        std::vector<MWWorld::Ptr> nearestMember(const QuestTargets::Objective& objective) const;
        // Where a cell is: inside the door the player comes in through, or the middle of an exterior cell of that name.
        std::optional<Target> placeOfCell(const ESM::RefId& cell, QuestTargets::Task task) const;

        std::optional<QuestTargets::Index> mIndex;
        QuestCorrections::Corrections mCorrections;
        std::set<ESM::RefId> mTracked;
        // Each quest's stage when the journal was last looked at, to see which quests moved on.
        std::map<ESM::RefId, int> mStages;
        bool mJournalSeen = false;
        float mTimer = 0.f;
        ESM::RefId mLastPlayerCell;
        std::vector<Marker> mLocalMarkers;
        std::vector<Marker> mWorldMarkers;
        unsigned int mGeneration = 0;
    };
}

#endif
