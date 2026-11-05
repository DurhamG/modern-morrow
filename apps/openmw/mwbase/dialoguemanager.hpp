#ifndef GAME_MWBASE_DIALOGUEMANAGER_H
#define GAME_MWBASE_DIALOGUEMANAGER_H

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <cstdint>

#include <components/esm/refid.hpp>

namespace Loading
{
    class Listener;
}

namespace ESM
{
    class ESMReader;
    class ESMWriter;
    struct DialInfo;
    struct Dialogue;
}

namespace MWDialogue
{
    class KeywordSearch;
}

namespace MWWorld
{
    class Ptr;
}

namespace MWBase
{
    /// \brief Interface for dialogue manager (implemented in MWDialogue)
    class DialogueManager
    {
        DialogueManager(const DialogueManager&);
        ///< not implemented

        DialogueManager& operator=(const DialogueManager&);
        ///< not implemented

    public:
        /// A line of the current conversation, in the order the dialogue window shows them
        struct TranscriptEntry
        {
            /// The header above a response, such as the name of the topic. Empty for greetings and answers.
            std::string mTitle;
            /// The text with its variables, such as %PCName, filled in. Topic link markers (@...#) are kept.
            std::string mText;
            /// The dialogue record and the info that a response came from, if any
            const ESM::Dialogue* mDialogue = nullptr;
            const ESM::DialInfo* mInfo = nullptr;
            /// A message that the game showed during the conversation, such as a journal update
            bool mIsMessage = false;
        };

        /// A topic that the player can ask the actor about
        struct AvailableTopic
        {
            ESM::RefId mId;
            /// The name of the topic as the dialogue window lists it
            std::string mName;
            /// TopicType flags
            int mFlags;
        };

        DialogueManager() {}

        virtual void clear() = 0;

        virtual ~DialogueManager() {}

        virtual bool isInChoice() const = 0;

        virtual bool startDialogue(const MWWorld::Ptr& actor) = 0;

        /// Forgets the transcript once the conversation is over
        virtual void endDialogue() = 0;

        /// The actor the player is talking to. Empty outside a conversation.
        virtual MWWorld::Ptr getActor() const = 0;

        virtual const std::vector<TranscriptEntry>& getTranscript() const = 0;

        /// Adds a message that the game shows during the conversation to the transcript
        virtual void addMessage(std::string_view text) = 0;

        /// Adds the actor's reply to a finished trade to the transcript
        virtual void tradeCompleted() = 0;

        virtual void executeScript(const std::string& script, const MWWorld::Ptr& actor) = 0;

        virtual bool inJournal(const ESM::RefId& topicId, const ESM::RefId& infoId) const = 0;

        virtual void addTopic(const ESM::RefId& topic) = 0;

        virtual void addChoice(std::string_view text, int choice) = 0;
        virtual const std::vector<std::pair<std::string, int>>& getChoices() const = 0;

        virtual bool isGoodbye() const = 0;

        virtual void goodbye() = 0;

        virtual bool say(const MWWorld::Ptr& actor, const ESM::RefId& topic) = 0;

        /// Asks the actor about one of the available topics. Does nothing and returns false if the topic isn't
        /// available, or while the player must answer a choice or say goodbye.
        virtual bool keywordSelected(std::string_view keyword) = 0;
        /// Ends the conversation from the dialogue manager's side. Does nothing and returns false while the player must
        /// answer a choice, unless a script has ended the conversation with Goodbye.
        virtual bool goodbyeSelected() = 0;
        /// Answers the current choice. Does nothing and returns false if the player isn't in a choice or the answer
        /// isn't one of the choices.
        virtual bool questionAnswered(int answer) = 0;

        enum TopicType
        {
            Specific = 1,
            Exhausted = 2
        };

        enum ServiceType
        {
            Any = -1,
            Barter = 1,
            Repair = 2,
            Spells = 3,
            Training = 4,
            Travel = 5,
            Spellmaking = 6,
            Enchanting = 7
        };

        /// The topics that the player can ask about, sorted by name. They are worked out again after each step of the
        /// conversation.
        virtual const std::vector<AvailableTopic>& getAvailableTopics() const = 0;

        /// Finds the available topics in a text, to show them as links
        virtual const MWDialogue::KeywordSearch& getAvailableTopicSearch() const = 0;

        virtual bool checkServiceRefused(ServiceType service = ServiceType::Any) = 0;

        virtual void persuade(int type) = 0;

        /// Changes the disposition of @a merchant after the player haggled with them. Does nothing unless the player
        /// is talking to @a merchant.
        /// @note Controlled by an option, gets discarded when dialogue ends by default
        virtual void applyBarterDispositionChange(const MWWorld::Ptr& merchant, int delta) = 0;

        virtual size_t countSavedGameRecords() const = 0;

        virtual void write(ESM::ESMWriter& writer, Loading::Listener& progress) const = 0;

        virtual void readRecord(ESM::ESMReader& reader, uint32_t type) = 0;

        /// Changes faction1's opinion of faction2 by \a diff.
        virtual void modFactionReaction(const ESM::RefId& faction1, const ESM::RefId& faction2, int diff) = 0;

        /// Set faction1's opinion of faction2.
        virtual void setFactionReaction(const ESM::RefId& faction1, const ESM::RefId& faction2, int absolute) = 0;

        /// @return faction1's opinion of faction2
        virtual int getFactionReaction(const ESM::RefId& faction1, const ESM::RefId& faction2) const = 0;

        /// @return all faction's opinion overrides
        virtual const std::map<ESM::RefId, int>* getFactionReactionOverrides(const ESM::RefId& faction) const = 0;

        /// Removes the last added topic response for the given actor from the journal
        virtual void clearInfoActor(const MWWorld::Ptr& actor) const = 0;
    };
}

#endif
