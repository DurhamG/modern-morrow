#include "conversationbindings.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>

#include <components/esm3/loaddial.hpp>
#include <components/esm3/loadinfo.hpp>
#include <components/lua/luastate.hpp>

#include "../mwbase/dialoguemanager.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwdialogue/keywordsearch.hpp"
#include "../mwworld/worldmodel.hpp"

#include "context.hpp"
#include "luamanagerimp.hpp"
#include "object.hpp"

namespace MWLua
{
    namespace
    {
        using DialogueManager = MWBase::DialogueManager;

        const DialogueManager& getDialogueManager()
        {
            return *MWBase::Environment::get().getDialogueManager();
        }

        bool isInDialogue()
        {
            return !getDialogueManager().getActor().isEmpty();
        }

        void verifyPlayer(const Object& player)
        {
            if (player.ptr() != MWBase::Environment::get().getWorld()->getPlayerPtr())
                throw std::runtime_error("The argument must be a player!");
        }

        std::string_view getRecordType(const ESM::Dialogue& dialogue)
        {
            switch (dialogue.mType)
            {
                case ESM::Dialogue::Topic:
                    return "topic";
                case ESM::Dialogue::Voice:
                    return "voice";
                case ESM::Dialogue::Greeting:
                    return "greeting";
                case ESM::Dialogue::Persuasion:
                    return "persuasion";
                case ESM::Dialogue::Journal:
                    return "journal";
                default:
                    return {};
            }
        }

        sol::table makeTranscriptEntry(sol::state_view lua, const DialogueManager::TranscriptEntry& entry)
        {
            sol::table result(lua, sol::create);
            result["text"] = entry.mText;
            if (!entry.mTitle.empty())
                result["title"] = entry.mTitle;
            result["isMessage"] = entry.mIsMessage;
            if (entry.mDialogue != nullptr)
            {
                result["type"] = getRecordType(*entry.mDialogue);
                result["recordId"] = entry.mDialogue->mId.serializeText();
            }
            if (entry.mInfo != nullptr)
                result["infoId"] = entry.mInfo->mId.serializeText();
            return result;
        }

        sol::table makeTopics(sol::state_view lua)
        {
            sol::table result(lua, sol::create);
            if (!isInDialogue())
                return result;
            for (const DialogueManager::AvailableTopic& topic : getDialogueManager().getAvailableTopics())
            {
                sol::table entry(lua, sol::create);
                entry["id"] = topic.mId.serializeText();
                entry["name"] = topic.mName;
                entry["specific"] = (topic.mFlags & DialogueManager::TopicType::Specific) != 0;
                entry["exhausted"] = (topic.mFlags & DialogueManager::TopicType::Exhausted) != 0;
                result.add(entry);
            }
            return result;
        }

        sol::table makeChoices(sol::state_view lua)
        {
            sol::table result(lua, sol::create);
            if (!isInDialogue() || !getDialogueManager().isInChoice())
                return result;
            for (const auto& [text, answer] : getDialogueManager().getChoices())
            {
                sol::table choice(lua, sol::create);
                choice["text"] = text;
                choice["answer"] = answer;
                result.add(choice);
            }
            return result;
        }

        bool mustSayGoodbye()
        {
            return isInDialogue() && getDialogueManager().isGoodbye();
        }

        // Talking runs dialogue scripts, which can change anything, so local scripts may only talk for their own player
        void verifySpeaker(const Object& player)
        {
            verifyPlayer(player);
            if (player.isLObject() && !player.isSelfObject())
                throw std::runtime_error("Only player and global scripts can talk for the player");
            if (!isInDialogue())
                throw std::runtime_error("The player isn't talking to anyone");
        }

        void verifyNoQuestion()
        {
            if (getDialogueManager().isGoodbye())
                throw std::runtime_error("The conversation is over, the player can only say goodbye");
            if (getDialogueManager().isInChoice())
                throw std::runtime_error("The player must answer the question first");
        }

        std::optional<LuaUtil::Callback> toCallback(const sol::optional<sol::table>& callback)
        {
            if (!callback)
                return std::nullopt;
            return LuaUtil::Callback::fromLua(*callback);
        }

        // Gives the lines that an action added, and the state of the conversation after it, to the script's callback
        void reportOutcome(const Context& context, const std::optional<LuaUtil::Callback>& callback, size_t firstLine)
        {
            if (!callback)
                return;
            sol::state_view lua = context.mLua->unsafeState();
            sol::table lines(lua, sol::create);
            const std::vector<DialogueManager::TranscriptEntry>& transcript = getDialogueManager().getTranscript();
            for (size_t i = firstLine; i < transcript.size(); ++i)
                lines.add(makeTranscriptEntry(lua, transcript[i]));
            sol::table outcome(lua, sol::create);
            outcome["lines"] = lines;
            outcome["topics"] = makeTopics(lua);
            outcome["choices"] = makeChoices(lua);
            outcome["mustSayGoodbye"] = mustSayGoodbye();
            context.mLuaManager->queueCallback(*callback, sol::main_object(lua, sol::in_place, outcome));
        }

        // Runs a step of the conversation at the end of the frame, if the player is still talking by then
        template <class Step>
        void queueStep(
            const Context& context, const sol::optional<sol::table>& callback, Step step, std::string_view name)
        {
            context.mLuaManager->addAction(
                [context, callback = toCallback(callback), step = std::move(step)] {
                    MWBase::DialogueManager& dialogueManager = *MWBase::Environment::get().getDialogueManager();
                    const size_t firstLine = dialogueManager.getTranscript().size();
                    if (isInDialogue())
                        step(dialogueManager);
                    reportOutcome(context, callback, firstLine);
                },
                name);
        }
    }

    void addConversationBindings(sol::table& player, const Context& context)
    {
        player["getDialogueActor"] = [](sol::this_state thisState, const Object& object) -> sol::object {
            verifyPlayer(object);
            const MWWorld::Ptr actor = getDialogueManager().getActor();
            if (actor.isEmpty())
                return sol::nil;
            MWBase::Environment::get().getWorldModel()->registerPtr(actor);
            if (object.isGObject())
                return sol::make_object(thisState, GObject(actor));
            return sol::make_object(thisState, LObject(actor));
        };
        player["getDialogueTopics"] = [](sol::this_state thisState, const Object& object) {
            verifyPlayer(object);
            return makeTopics(thisState);
        };
        player["getDialogueTranscript"] = [](sol::this_state thisState, const Object& object) {
            verifyPlayer(object);
            sol::table result(thisState, sol::create);
            if (!isInDialogue())
                return result;
            for (const DialogueManager::TranscriptEntry& entry : getDialogueManager().getTranscript())
                result.add(makeTranscriptEntry(thisState, entry));
            return result;
        };
        player["getDialogueChoices"] = [](sol::this_state thisState, const Object& object) {
            verifyPlayer(object);
            return makeChoices(thisState);
        };
        player["mustSayGoodbye"] = [](const Object& object) {
            verifyPlayer(object);
            return mustSayGoodbye();
        };
        player["findTopicLinks"] = [](sol::this_state thisState, const Object& object, std::string_view text) {
            verifyPlayer(object);
            const DialogueManager& dialogueManager = getDialogueManager();
            const MWDialogue::KeywordSearch::DisplayText displayText
                = dialogueManager.getAvailableTopicSearch().getDisplayText(
                    std::string(text), MWBase::Environment::get().getWindowManager()->getTranslationDataStorage());
            const std::vector<DialogueManager::AvailableTopic>& topics = dialogueManager.getAvailableTopics();
            sol::table links(thisState, sol::create);
            for (const MWDialogue::KeywordSearch::Link& link : displayText.mLinks)
            {
                // Like the Dialogue window, only link to the topics that the player can ask about
                const ESM::RefId id = ESM::RefId::stringRefId(link.mTopicId);
                const auto topic = std::find_if(topics.begin(), topics.end(),
                    [&](const DialogueManager::AvailableTopic& available) { return available.mId == id; });
                if (topic == topics.end() || link.mBegin == link.mEnd)
                    continue;
                sol::table entry(thisState, sol::create);
                entry["first"] = link.mBegin + 1;
                entry["last"] = link.mEnd;
                entry["topicId"] = topic->mId.serializeText();
                links.add(entry);
            }
            sol::table result(thisState, sol::create);
            result["text"] = displayText.mText;
            result["links"] = links;
            return result;
        };

        player["askDialogueTopic"] = [context](const Object& object, std::string_view topicId,
                                         const sol::optional<sol::table>& callback) {
            verifySpeaker(object);
            verifyNoQuestion();
            const ESM::RefId id = ESM::RefId::deserializeText(topicId);
            const std::vector<DialogueManager::AvailableTopic>& topics = getDialogueManager().getAvailableTopics();
            const auto topic = std::find_if(topics.begin(), topics.end(),
                [&](const DialogueManager::AvailableTopic& available) { return available.mId == id; });
            if (topic == topics.end())
                throw std::runtime_error("The actor doesn't offer the topic " + id.toDebugString());
            queueStep(
                context, callback,
                [name = topic->mName](
                    MWBase::DialogueManager& dialogueManager) { dialogueManager.keywordSelected(name); },
                "AskDialogueTopicAction");
        };
        player["answerDialogueChoice"] = [context](const Object& object, int answer,
                                             const sol::optional<sol::table>& callback) {
            verifySpeaker(object);
            const std::vector<std::pair<std::string, int>>& choices = getDialogueManager().getChoices();
            const auto isAnswer = [&](const std::pair<std::string, int>& choice) { return choice.second == answer; };
            if (!getDialogueManager().isInChoice() || std::none_of(choices.begin(), choices.end(), isAnswer))
                throw std::runtime_error("The player can't give the answer " + std::to_string(answer));
            queueStep(
                context, callback,
                [answer](MWBase::DialogueManager& dialogueManager) { dialogueManager.questionAnswered(answer); },
                "AnswerDialogueChoiceAction");
        };
        player["sayGoodbye"] = [context](const Object& object) {
            verifySpeaker(object);
            if (getDialogueManager().isInChoice() && !getDialogueManager().isGoodbye())
                throw std::runtime_error("The player must answer the question first");
            queueStep(
                context, sol::nullopt,
                [](MWBase::DialogueManager& dialogueManager) {
                    if (dialogueManager.goodbyeSelected())
                        MWBase::Environment::get().getWindowManager()->removeGuiMode(MWGui::GM_Dialogue);
                },
                "SayGoodbyeAction");
        };
    }
}
