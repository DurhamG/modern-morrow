#ifndef MWGUI_QUESTTARGETS_H
#define MWGUI_QUESTTARGETS_H

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <components/esm/refid.hpp>
#include <components/esm3/dialoguecondition.hpp>
#include <components/esm3/loadinfo.hpp>

namespace MWWorld
{
    class ESMStore;
}

namespace MWGui
{
    class TopicLearning;

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

        /// Something the player does to move a quest on. The target is an object, or else anyone in a faction, or a
        /// place. A faction can be limited to the members of a rank or higher, and to the cells whose names start
        /// with mCell.
        struct Objective
        {
            Task mTask = Task::Talk;
            ESM::RefId mObject;
            ESM::RefId mFaction;
            ESM::RefId mCell;
            // How many of the object must be dead or carried, for Kill and Fetch.
            int mCount = 1;
            // The lowest rank in the faction that can be talked to, or -1 for any.
            int mRank = -1;

            bool operator==(const Objective& other) const = default;
        };

        /// One way forward from a stage: the objectives it needs, in order. Kills and fetches come before the talk
        /// that the dialogue needs them for. The way leads to mToStage.
        struct Way
        {
            int mToStage = 0;
            std::vector<Objective> mObjectives;
            // Whether the way is an answer to a question in a conversation. The marker is the same, on the person who
            // asks.
            bool mChoice = false;
            // Whether the way needs someone dead who talks in the quest, or who follows the player somewhere. That
            // is usually how the quest fails, such as the quest giver or an escorted pilgrim dying.
            bool mFailure = false;
            // Whether the way needs an item that the player doesn't carry and that isn't anywhere in the world yet,
            // such as a blade that a later stage makes, or someone to talk to who is dead or gone. Only the game
            // while it runs can tell.
            bool mOutOfReach = false;
            // Whether the way is a side step the player can take or leave, such as asking someone for directions. Only
            // a correction marks a way as optional. See questcorrections.hpp.
            bool mOptional = false;
            // The line's conditions on global variables, other quests' stages and the speaker's local variables,
            // which only the game while it runs can check.
            std::vector<ESM::DialogueCondition> mChecks;
            // Who says the line, whose local variables the checks can name.
            ESM::RefId mSpeaker;
        };

        /// Which ways forward to mark.
        enum class Rule
        {
            /// Only the ways that lead to the lowest stage above the current one. This follows the journal's hints,
            /// one at a time.
            Lowest,
            /// Every way forward.
            All,
            /// The ways to the lowest stage above the current one, and to the highest. This keeps the journal's next
            /// hint and adds where the quest is going.
            LowestAndHighest,
        };

        /// What a script waits for before it moves a quest on.
        enum class ScriptEvent
        {
            Death,
            PickUp,
            Other,
        };

        /// A Journal or SetJournalIndex command in a script, and the conditions of the if blocks it is inside, each
        /// as the words of its line, lowercase.
        struct ScriptJournalCall
        {
            ESM::RefId mQuest;
            int mStage = 0;
            std::vector<std::vector<std::string>> mConditions;
            // The conditions of the blocks before the command that return when they hold, such as
            // `if ( GetJournalIndex MG_EscortScholar2 < 50 ) return endif`. The command only runs when they don't.
            std::vector<std::vector<std::string>> mGuards;
        };

        /// The Journal and SetJournalIndex commands in a script. A command can be run on another object, the quest
        /// can be in quotes, and anything after a ";" is a comment.
        std::vector<ScriptJournalCall> scriptJournalCalls(std::string_view script);

        /// The quest and stage of every Journal or SetJournalIndex command in a script.
        std::vector<std::pair<ESM::RefId, int>> journalChanges(std::string_view script);

        /// The numbers of the answers a Choice command in a script offers, such as 1 and 2 for
        /// `Choice "Yes" 1 "No" 2`.
        std::vector<int> choicesOffered(std::string_view script);

        /// The lines of a topic, in order, with what each answer in a conversation inherits from the lines that offer
        /// its choice. The game only gives the answer after one of those lines, so the answer needs what they all
        /// need: their shared conditions, other than choices and the stages of quests those lines move on, and their
        /// speaker, faction and cell where the answer names none. For example, an answer that hands over an item needs
        /// the item when the line offering it does. A line that isn't an answer, or whose choice no line offers, stays
        /// as it is.
        std::vector<ESM::DialInfo> withOfferingConditions(const std::vector<ESM::DialInfo>& lines);

        /// The items a script takes from the player, and how many, such as `Player->RemoveItem "misc_box" 1`.
        std::vector<std::pair<ESM::RefId, int>> itemsTaken(std::string_view script);

        /// What the conditions around a script's Journal command wait for.
        struct CallTargets
        {
            // What happens to the object the script is on, such as `OnDeath` or `OnPCAdd`.
            ScriptEvent mEvent = ScriptEvent::Other;
            // Objects the player has to be near, such as `Player->GetDistance "sattir the bold" < 200`.
            std::vector<ESM::RefId> mNear;
            // Cells the player has to be in, such as `GetPCCell "Gnaar Mok"`.
            std::vector<ESM::RefId> mCells;
            // Objects the script counts the dead of, such as `GetDeadCount "rat_cave_fgrh"`.
            std::vector<ESM::RefId> mKills;
            // Items the player has to carry, such as `Player->GetItemCount "misc_box" > 0`.
            std::vector<ESM::RefId> mFetches;
        };

        /// Reads what a script's conditions wait for.
        CallTargets callTargets(const std::vector<std::vector<std::string>>& conditions);

        /// The checks on quests' stages in a script's conditions, such as `GetJournalIndex MG_EscortScholar2 >= 50`, as
        /// a line of dialogue would have them. A guard's check is turned around, since the command only runs when the
        /// guard doesn't hold.
        std::vector<ESM::DialogueCondition> stageChecks(const ScriptJournalCall& call);

        /// Whether a line's conditions on a quest's stage allow the stage. A line with no condition on the quest
        /// allows every stage.
        bool allowsStage(const std::vector<ESM::DialogueCondition>& conditions, const ESM::RefId& quest, int stage);

        /// Whether a condition holds for a value, such as a global variable's.
        bool compare(const ESM::DialogueCondition& condition, float value);

        /// Chooses the ways forward to mark, by the rule. Failures and ways out of reach only count when every way
        /// forward is one, such as after the escorted pilgrim has died. The ways come out in stage order.
        std::vector<Way> chooseWays(std::vector<Way> ways, Rule rule);

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

            /// A person in a faction, and their rank.
            struct Member
            {
                ESM::RefId mId;
                int mRank = 0;
            };

            /// Adds the ways forward from a line of dialogue that isn't a journal entry. The topic is the line's, for
            /// a line in a topic the player asks about.
            void addDialogue(const ESM::DialInfo& info, std::string_view topic = {});

            /// Notes when the player can first learn the topics of the lines added so far. A line in a topic the
            /// player can only learn at a later stage of the same quest isn't a way forward before then.
            void learnTopics(const TopicLearning& learning);

            /// Adds the ways forward from a script, and the objects it's on. A script on no object is a global
            /// script.
            void addScript(std::string_view text, const std::vector<Carrier>& carriers);

            /// Notes that an actor or container starts with an item, so the item can be found on them.
            void addHolder(const ESM::RefId& item, const ESM::RefId& holder);

            /// Notes a person's faction and rank.
            void addMember(const ESM::RefId& faction, const ESM::RefId& person, int rank);

            /// Whether the content has an actor follow or escort the player, from a line they say or a script.
            bool isCompanion(const ESM::RefId& actor) const { return mCompanions.contains(actor); }

            /// Every way forward from a stage of a quest. The game leaves out the ones whose checks don't hold, then
            /// chooses among the rest with chooseWays.
            std::vector<Way> waysForward(const ESM::RefId& quest, int stage) const;

            /// The ways forward from a stage of a quest, chosen by the rule, without the checks.
            std::vector<Way> waysForward(const ESM::RefId& quest, int stage, Rule rule) const;

            /// The actors and containers that start with an item.
            const std::vector<ESM::RefId>& holdersOf(const ESM::RefId& item) const;

            /// The people in a faction of a rank or higher.
            std::vector<ESM::RefId> membersOf(const ESM::RefId& faction, int rank) const;

            /// The facts that make up how a quest moves on, without any text: for each line or script that moves
            /// it, its topic, who says it or what it's on, the stage it leads to, and its conditions on the quest's
            /// own stage. A mod that changes those changes the facts. Each fact is a line of lowercase text, in a
            /// fixed order.
            std::vector<std::string> shapeOf(const ESM::RefId& quest) const;

            /// Builds the index from every dialogue, script, inventory and faction member in the content.
            static Index build(const MWWorld::ESMStore& store);

        private:
            struct Source
            {
                int mToStage = 0;
                // The line's conditions, for a way that comes from dialogue.
                std::vector<ESM::DialogueCondition> mConditions;
                bool mDialogue = false;
                // The topic of a line the player asks about, and the lowest stage of the quest at which they can
                // learn it.
                std::string mTopic;
                int mLearnedAt = 0;
                ESM::RefId mSpeaker;
                ESM::RefId mFaction;
                ESM::RefId mCell;
                int mRank = -1;
                // The items the line's result takes from the player.
                std::vector<std::pair<ESM::RefId, int>> mTaken;
                // The objects a script is on, and what the script waits for.
                std::vector<Carrier> mCarriers;
                CallTargets mTargets;
            };

            std::vector<Objective> objectivesOf(const ESM::RefId& quest, const Source& source) const;

            std::unordered_map<ESM::RefId, std::vector<Source>> mSources;
            // The people who move each quest on by talking.
            std::unordered_map<ESM::RefId, std::unordered_set<ESM::RefId>> mSpeakers;
            // The actors who follow or escort the player.
            std::unordered_set<ESM::RefId> mCompanions;
            std::unordered_map<ESM::RefId, std::vector<ESM::RefId>> mHolders;
            std::unordered_map<ESM::RefId, std::vector<Member>> mMembers;
        };
    }
}

#endif
