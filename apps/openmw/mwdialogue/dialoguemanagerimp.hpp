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

        std::vector<TranscriptEntry> mTranscript;

        int mOriginalDisposition;
        int mCurrentDisposition;
        int mPermanentDispositionChange;

        const MWDialogue::KeywordSearch& getKeywordSearch() const;
        std::vector<ESM::RefId> parseTopicIdsFromText(const std::string& text) const;
        void addTopicsFromText(const std::string& text);

        void updateActorKnownTopics();
        void updateGlobals();

        std::optional<Interpreter::Program> compile(const std::string& cmd, const MWWorld::Ptr& actor);

        void addResponse(std::string_view title, const ESM::DialInfo& info, const ESM::Dialogue& dialogue);

        void executeTopic(const ESM::RefId& topic);

        const ESM::Dialogue* searchDialogue(const ESM::RefId& id);

        void updateOriginalDisposition();

    public:
        DialogueManager(const Compiler::Extensions& extensions, Translation::Storage& translationDataStorage);

        void clear() override;

        bool isInChoice() const override;

        bool startDialogue(const MWWorld::Ptr& actor) override;

        void endDialogue() override;

        const std::vector<TranscriptEntry>& getTranscript() const override;

        void addMessage(std::string_view text) override;

        void tradeCompleted() override;

        void executeScript(const std::string& script, const MWWorld::Ptr& actor) override;

        std::list<std::string> getAvailableTopics() override;
        int getTopicFlag(const ESM::RefId& topicId) const override;

        bool inJournal(const ESM::RefId& topicId, const ESM::RefId& infoId) const override;

        void addTopic(const ESM::RefId& topic) override;

        void addChoice(std::string_view text, int choice) override;
        const std::vector<std::pair<std::string, int>>& getChoices() const override;

        bool isGoodbye() const override;

        void goodbye() override;

        bool checkServiceRefused(ServiceType service = ServiceType::Any) override;

        bool say(const MWWorld::Ptr& actor, const ESM::RefId& topic) override;

        // calbacks for the GUI
        void keywordSelected(std::string_view keyword) override;
        void goodbyeSelected() override;
        void questionAnswered(int answer) override;

        void persuade(int type) override;

        /// @note Controlled by an option, gets discarded when dialogue ends by default
        void applyBarterDispositionChange(const MWWorld::Ptr& merchant, int delta) override;

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
