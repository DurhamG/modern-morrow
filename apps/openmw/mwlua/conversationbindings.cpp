#include "conversationbindings.hpp"

#include <stdexcept>

#include <components/esm3/loaddial.hpp>
#include <components/esm3/loadinfo.hpp>

#include "../mwbase/dialoguemanager.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwworld/worldmodel.hpp"

#include "context.hpp"
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
    }
}
