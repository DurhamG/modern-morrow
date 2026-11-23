#ifndef MWGUI_QUESTTARGETS_H
#define MWGUI_QUESTTARGETS_H

#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <components/esm/refid.hpp>
#include <components/esm3/dialoguecondition.hpp>

namespace ESM
{
    struct DialInfo;
}

namespace MWWorld
{
    class ESMStore;
}

namespace MWGui
{
    /// Works out where a quest goes next from the game's content. The journal only keeps a stage for each quest, so the
    /// places come from what in the content moves a quest from one stage to the next: a line of dialogue whose result
    /// script runs Journal, or a script that runs it. See ui-rework/design/quest-markers.md.
    namespace QuestTargets
    {
        /// What the player does at a target.
        enum class Task
        {
            Talk,
            Kill,
            Fetch,
            Visit,
        };

        /// Something the player does to move a quest on. Talking can be to a person, or to anyone in a faction or a
        /// cell, when the line of dialogue doesn't name a person.
        struct Objective
        {
            Task mTask = Task::Talk;
            ESM::RefId mObject;
            ESM::RefId mFaction;
            ESM::RefId mCell;
            // How many of the object must be dead or carried, for Kill and Fetch.
            int mCount = 1;

            bool operator==(const Objective& other) const = default;
        };

        /// One way forward from a stage: the objectives it needs, in order. Kills and fetches come before the talk
        /// that the dialogue needs them for. The way leads to mToStage.
        struct Way
        {
            int mToStage = 0;
            std::vector<Objective> mObjectives;
        };

        /// Which ways forward to mark.
        enum class Rule
        {
            /// Only the ways that lead to the lowest stage above the current one. This follows the journal's hints,
            /// one at a time.
            Lowest,
            /// Every way forward.
            All,
        };

        /// What happens to an object that makes its script move a quest on.
        enum class ScriptEvent
        {
            Death,
            PickUp,
            Other,
        };

        /// The quest and stage of every Journal or SetJournalIndex command in a script. The command can be run on
        /// another object, the quest can be in quotes, and anything after a ";" is a comment.
        std::vector<std::pair<ESM::RefId, int>> journalChanges(std::string_view script);

        /// What a script waits for before it moves a quest on, from the commands it uses.
        ScriptEvent scriptEvent(std::string_view script);

        /// The objects a script checks the dead count of, such as `GetDeadCount "rat_cave_fgrh"`.
        std::vector<ESM::RefId> deadCountTargets(std::string_view script);

        /// Whether a line's conditions on a quest's stage allow the stage. A line with no condition on the quest
        /// allows every stage.
        bool allowsStage(const std::vector<ESM::DialogueCondition>& conditions, const ESM::RefId& quest, int stage);

        /// The ways forward of every quest in the content.
        class Index
        {
        public:
            /// An object that a script is on, and whether it is an actor.
            struct Carrier
            {
                ESM::RefId mId;
                bool mActor = false;
            };

            /// Adds the ways forward from a line of dialogue that isn't a journal entry.
            void addDialogue(const ESM::DialInfo& info);

            /// Adds the ways forward from a script, and the objects it's on. A script on no object is a global
            /// script.
            void addScript(std::string_view text, const std::vector<Carrier>& carriers);

            /// The ways forward from a stage of a quest, by the rule.
            std::vector<Way> waysForward(const ESM::RefId& quest, int stage, Rule rule) const;

            /// Builds the index from every dialogue and script in the content.
            static Index build(const MWWorld::ESMStore& store);

        private:
            struct Source
            {
                int mToStage = 0;
                // The line's conditions, for a way that comes from dialogue.
                std::vector<ESM::DialogueCondition> mConditions;
                bool mDialogue = false;
                ESM::RefId mSpeaker;
                ESM::RefId mFaction;
                ESM::RefId mCell;
                // The objects a script is on, and what it waits for.
                std::vector<Carrier> mCarriers;
                ScriptEvent mEvent = ScriptEvent::Other;
                std::vector<ESM::RefId> mDeadCountTargets;
            };

            std::vector<Objective> objectivesOf(const ESM::RefId& quest, const Source& source) const;

            std::unordered_map<ESM::RefId, std::vector<Source>> mSources;
            // The people who move each quest on by talking. A line that needs one of them dead is a failure branch,
            // such as the quest giver dying, and not an objective.
            std::unordered_map<ESM::RefId, std::unordered_set<ESM::RefId>> mSpeakers;
        };
    }
}

#endif
