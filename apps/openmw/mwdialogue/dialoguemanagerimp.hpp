#ifndef GAME_MWDIALOG_DIALOGUEMANAGERIMP_H
#define GAME_MWDIALOG_DIALOGUEMANAGERIMP_H

#include "../mwbase/dialoguemanager.hpp"

#include <map>
#include <optional>
#include <set>
#include <unordered_map>

#include <components/compiler/streamerrorhandler.hpp>
#include <components/esm3/loadinfo.hpp>
#include <components/interpreter/program.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/translation/translation.hpp>

#include "../mwscript/compilercontext.hpp"
#include "../mwworld/ptr.hpp"

#include "keywordsearch.hpp"
#include "lockouts.hpp"

namespace ESM
{
    struct Dialogue;
}

namespace MWDialogue
{
    class DialogueManager : public MWBase::DialogueManager
    {
        struct ActorKnownTopicInfo
        {
            int mFlags;
            const ESM::DialInfo* mInfo;
        };

        std::set<ESM::RefId> mKnownTopics; // Those are the topics the player knows.
        // Topic answers the player heard since the game was loaded, as topic and answer IDs. Some answers remove
        // themselves from the journal, such as most answers to "Background", so the journal can't tell whether
        // the player heard them.
        std::set<std::pair<ESM::RefId, ESM::RefId>> mHeardAnswers;

        // Modified faction reactions. <Faction1, <Faction2, Difference> >
        typedef std::map<ESM::RefId, std::map<ESM::RefId, int>> ModFactionReactionMap;
        ModFactionReactionMap mChangedFactionReaction;

        std::map<ESM::RefId, ActorKnownTopicInfo> mActorKnownTopics;

        Translation::Storage& mTranslationDataStorage;
        mutable bool mKeywordSearchInitialized{ false };
        mutable MWDialogue::KeywordSearch mKeywordSearch;

        MWScript::CompilerContext mCompilerContext;
        Compiler::StreamErrorHandler mErrorHandler;

        MWWorld::Ptr mActor;
        bool mTalkedTo;

        int mChoice;
        ESM::RefId mLastTopic; // last topic ID, lowercase
        bool mIsInChoice;
        bool mGoodbye;

        std::vector<std::pair<std::string, int>> mChoices;

        // A topic or answer the player picked whose line closes off other content, while the player is asked whether
        // to go ahead.
        struct PendingLockout
        {
            ESM::RefId mTopic;
            std::optional<int> mAnswer;
            ResponseCallback* mCallback = nullptr;
            // The key of the explanation in the Interface localisation context, and whether the player asked for it.
            std::string mWarning;
            bool mExplained = false;
        };
        std::optional<PendingLockout> mPendingLockout;
        // Set while a line the player agreed to is said, so they aren't asked again.
        bool mLockoutConfirmed = false;
        std::optional<Lockouts::Data> mLockouts;

        int mOriginalDisposition;
        int mCurrentDisposition;
        int mPermanentDispositionChange;

        const MWDialogue::KeywordSearch& getKeywordSearch() const;
        std::vector<ESM::RefId> parseTopicIdsFromText(const std::string& text) const;
        void addTopicsFromText(const std::string& text);

        void updateActorKnownTopics();
        void updateGlobals();

        std::optional<Interpreter::Program> compile(const std::string& cmd, const MWWorld::Ptr& actor);

        void executeTopic(const ESM::RefId& topic, ResponseCallback* callback);

        const ESM::Dialogue* searchDialogue(const ESM::RefId& id);

        void updateOriginalDisposition();

        /// The explanation for a line that closes off other content, if the setting is on and the player should be
        /// asked before it.
        std::optional<std::string> lockoutWarning(const ESM::DialInfo& info);
        /// Asks the player whether to go ahead, and keeps the line waiting for the answer. At first the question only
        /// says that the choice is a major one, so as not to spoil anything.
        void askBeforeLockout(PendingLockout pending);

    public:
        DialogueManager(const Compiler::Extensions& extensions, Translation::Storage& translationDataStorage);

        void clear() override;

        bool isInChoice() const override;

        bool isAskingBeforeLockout() const override;
        void answerLockout(int button) override;

        bool startDialogue(const MWWorld::Ptr& actor, ResponseCallback* callback) override;

        void executeScript(const std::string& script, const MWWorld::Ptr& actor) override;

        std::list<std::string> getAvailableTopics() override;
        int getTopicFlag(const ESM::RefId& topicId) const override;

        bool inJournal(const ESM::RefId& topicId, const ESM::RefId& infoId) const override;

        void addTopic(const ESM::RefId& topic) override;

        void addChoice(std::string_view text, int choice) override;
        const std::vector<std::pair<std::string, int>>& getChoices() const override;

        bool isGoodbye() const override;

        void goodbye() override;

        bool checkServiceRefused(ResponseCallback* callback, ServiceType service = ServiceType::Any) override;

        bool say(const MWWorld::Ptr& actor, const ESM::RefId& topic) override;

        // calbacks for the GUI
        void keywordSelected(std::string_view keyword, ResponseCallback* callback) override;
        void goodbyeSelected() override;
        void questionAnswered(int answer, ResponseCallback* callback) override;

        void persuade(int type, ResponseCallback* callback) override;

        /// @note Controlled by an option, gets discarded when dialogue ends by default
        void applyBarterDispositionChange(int delta) override;

        size_t countSavedGameRecords() const override;

        void write(ESM::ESMWriter& writer, Loading::Listener& progress) const override;

        void readRecord(ESM::ESMReader& reader, uint32_t type) override;

        /// Changes faction1's opinion of faction2 by \a diff.
        void modFactionReaction(const ESM::RefId& faction1, const ESM::RefId& faction2, int diff) override;

        void setFactionReaction(const ESM::RefId& faction1, const ESM::RefId& faction2, int absolute) override;

        /// @return faction1's opinion of faction2
        int getFactionReaction(const ESM::RefId& faction1, const ESM::RefId& faction2) const override;

        const std::map<ESM::RefId, int>* getFactionReactionOverrides(const ESM::RefId& faction) const override;

        /// Removes the last added topic response for the given actor from the journal
        void clearInfoActor(const MWWorld::Ptr& actor) const override;
    };
}

#endif
