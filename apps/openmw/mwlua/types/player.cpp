#include "types.hpp"

#include <algorithm>

#include <components/esm3/loadbsgn.hpp>
#include <components/esm3/loadfact.hpp>
#include <components/esm3/loadskil.hpp>
#include <components/lua/util.hpp>

#include "../birthsignbindings.hpp"
#include "../luamanagerimp.hpp"
#include "../magicbindings.hpp"

#include "apps/openmw/mwbase/dialoguemanager.hpp"
#include "apps/openmw/mwbase/inputmanager.hpp"
#include "apps/openmw/mwbase/journal.hpp"
#include "apps/openmw/mwbase/mechanicsmanager.hpp"
#include "apps/openmw/mwbase/world.hpp"
#include "apps/openmw/mwmechanics/npcstats.hpp"
#include "apps/openmw/mwmechanics/services.hpp"
#include "apps/openmw/mwworld/class.hpp"
#include "apps/openmw/mwworld/containerstore.hpp"
#include "apps/openmw/mwworld/esmstore.hpp"
#include "apps/openmw/mwworld/globals.hpp"
#include "apps/openmw/mwworld/player.hpp"

namespace MWLua
{
    struct Quests
    {
        bool mMutable = false;
    };
    struct Quest
    {
        ESM::RefId mQuestId;
        bool mMutable = false;
    };
    struct Topics
    {
    };
    struct JournalEntries
    {
    };
    struct TopicEntries
    {
        ESM::RefId mTopicId;
    };
}

namespace sol
{
    template <>
    struct is_automagical<MWLua::Quests> : std::false_type
    {
    };
    template <>
    struct is_automagical<MWLua::Quest> : std::false_type
    {
    };
    template <>
    struct is_automagical<MWBase::Journal> : std::false_type
    {
    };
    template <>
    struct is_automagical<MWLua::Topics> : std::false_type
    {
    };
    template <>
    struct is_automagical<MWLua::JournalEntries> : std::false_type
    {
    };
    template <>
    struct is_automagical<MWDialogue::StampedJournalEntry> : std::false_type
    {
    };
    template <>
    struct is_automagical<MWDialogue::Topic> : std::false_type
    {
    };
    template <>
    struct is_automagical<MWLua::TopicEntries> : std::false_type
    {
    };
    template <>
    struct is_automagical<MWDialogue::Entry> : std::false_type
    {
    };
}

namespace
{
    ESM::RefId toBirthSignId(const sol::object& recordOrId)
    {
        if (recordOrId.is<ESM::BirthSign>())
            return recordOrId.as<const ESM::BirthSign*>()->mId;
        std::string_view textId = LuaUtil::cast<std::string_view>(recordOrId);
        ESM::RefId id = ESM::RefId::deserializeText(textId);
        if (!MWBase::Environment::get().getESMStore()->get<ESM::BirthSign>().search(id))
            throw std::runtime_error("Failed to find birth sign: " + std::string(textId));
        return id;
    }

    ESM::RefId parseFactionId(std::string_view faction)
    {
        ESM::RefId id = ESM::RefId::deserializeText(faction);
        if (!MWBase::Environment::get().getESMStore()->get<ESM::Faction>().search(id))
            return ESM::RefId();
        return id;
    }

    const MWDialogue::Topic& getTopicDataOrThrow(const ESM::RefId& topicId, const MWBase::Journal* journal)
    {
        const auto it = journal->getTopics().find(topicId);
        if (it == journal->getTopics().end())
            throw std::runtime_error("Topic " + topicId.toDebugString() + " could not be found in the journal");
        return it->second;
    }
}

namespace MWLua
{
    static void verifyPlayer(const Object& player)
    {
        if (player.ptr() != MWBase::Environment::get().getWorld()->getPlayerPtr())
            throw std::runtime_error("The argument must be a player!");
    }

    static void verifyNpc(const MWWorld::Class& cls)
    {
        if (!cls.isNpc())
            throw std::runtime_error("The argument must be a NPC!");
    }

    static void verifyActor(const MWWorld::Class& cls)
    {
        if (!cls.isActor())
            throw std::runtime_error("Actor expected");
    }

    // Buying a service changes the merchant as well as the player, so only global scripts can do it.
    static void verifyBuyer(const Object& player)
    {
        verifyPlayer(player);
        if (!player.isGObject())
            throw std::runtime_error("Only global scripts can buy services");
    }

    static bool canPay(int price)
    {
        const MWWorld::Ptr ptr = MWBase::Environment::get().getWorld()->getPlayerPtr();
        return price <= ptr.getClass().getContainerStore(ptr).count(MWWorld::ContainerStore::sGoldId);
    }

    void addJournalClassBindings(sol::state_view& lua, const MWBase::Journal* journal)
    {
        auto journalBindingsClass = lua.new_usertype<MWBase::Journal>("MWDialogue_Journal");
        journalBindingsClass[sol::meta_function::to_string] = [](const MWBase::Journal& store) {
            const size_t numberOfTopics = store.getTopics().size();
            const size_t numberOfJournalEntries = store.getEntries().size();
            return "{MWDialogue_Journal: " + std::to_string(numberOfTopics) + " topic entries, "
                + std::to_string(numberOfJournalEntries) + " journal entries}";
        };
        journalBindingsClass["topics"]
            = sol::readonly_property([](const MWBase::Journal& store) { return MWLua::Topics{}; });
        journalBindingsClass["journalTextEntries"]
            = sol::readonly_property([](const MWBase::Journal& store) { return MWLua::JournalEntries{}; });
    }

    void addJournalClassTopicsListBindings(sol::state_view& lua, const MWBase::Journal* journal)
    {
        auto topicsBindingsClass = lua.new_usertype<MWLua::Topics>("MWDialogue_Journal_Topics");
        topicsBindingsClass[sol::meta_function::to_string] = [journal](const MWLua::Topics& topicEntriesStore) {
            const size_t numberOfTopics = journal->getTopics().size();
            return "{MWDialogue_Journal_Topics: " + std::to_string(numberOfTopics) + " topics}";
        };
        topicsBindingsClass[sol::meta_function::index]
            = [journal](
                  const MWLua::Topics& topicEntriesStore, std::string_view givenTopicId) -> const MWDialogue::Topic* {
            const auto it = journal->getTopics().find(ESM::RefId::deserializeText(givenTopicId));
            if (it == journal->getTopics().end())
                return nullptr;
            return &it->second;
        };
        topicsBindingsClass[sol::meta_function::length]
            = [journal](const MWLua::Topics&) -> size_t { return journal->getTopics().size(); };
        topicsBindingsClass[sol::meta_function::pairs] = [journal](const MWLua::Topics&) {
            auto iterator = journal->getTopics().begin();
            return sol::as_function(
                [iterator, journal]() mutable -> std::pair<sol::optional<std::string>, const MWDialogue::Topic*> {
                    if (iterator != journal->getTopics().end())
                    {
                        return { iterator->first.serializeText(), &((iterator++)->second) };
                    }
                    return { sol::nullopt, nullptr };
                });
        };
    }

    void addJournalClassTopicBindings(sol::state_view& lua, const MWBase::Journal* journal)
    {
        auto topicBindingsClass = lua.new_usertype<MWDialogue::Topic>("MWDialogue_Journal_Topic");
        topicBindingsClass[sol::meta_function::to_string] = [](const MWDialogue::Topic& topic) {
            return "MWDialogue_Journal_Topic: \"" + std::string{ topic.getName() } + "\"";
        };
        topicBindingsClass["id"]
            = sol::readonly_property([](const MWDialogue::Topic& topic) { return topic.getTopic().serializeText(); });
        topicBindingsClass["name"]
            = sol::readonly_property([](const MWDialogue::Topic& topic) { return topic.getName(); });
        topicBindingsClass["entries"] = sol::readonly_property(
            [](const MWDialogue::Topic& topic) { return MWLua::TopicEntries{ topic.getTopic() }; });
    }

    void addJournalClassTopicEntriesListBindings(sol::state_view& lua, const MWBase::Journal* journal)
    {
        auto topicEntriesBindingsClass
            = lua.new_usertype<MWLua::TopicEntries>("MWDialogue_Journal_Topic_WrittenEntries");
        topicEntriesBindingsClass[sol::meta_function::to_string] = [journal](const MWLua::TopicEntries& topicEntries) {
            const MWDialogue::Topic& topic = getTopicDataOrThrow(topicEntries.mTopicId, journal);
            return "MWDialogue_Journal_Topic_WrittenEntries for \"" + std::string{ topic.getName() }
            + "\": " + std::to_string(topic.size()) + " elements";
        };
        topicEntriesBindingsClass[sol::meta_function::length] = [journal](const MWLua::TopicEntries& topicEntries) {
            const MWDialogue::Topic& topic = getTopicDataOrThrow(topicEntries.mTopicId, journal);
            return topic.size();
        };
        topicEntriesBindingsClass[sol::meta_function::index]
            = [journal](const MWLua::TopicEntries& topicEntries, size_t index) -> const MWDialogue::Entry* {
            const MWDialogue::Topic& topic = getTopicDataOrThrow(topicEntries.mTopicId, journal);

            if (index == 0 || index > topic.size())
                return nullptr;
            index = LuaUtil::fromLuaIndex(index);
            return &topic[index];
        };
        topicEntriesBindingsClass[sol::meta_function::ipairs] = lua["ipairsForArray"].template get<sol::function>();
        topicEntriesBindingsClass[sol::meta_function::pairs] = lua["ipairsForArray"].template get<sol::function>();
    }

    void addJournalClassTopicEntryBindings(sol::state_view& lua, const MWBase::Journal* journal)
    {
        auto topicEntryBindingsClass = lua.new_usertype<MWDialogue::Entry>("MWDialogue_Journal_Topic_WrittenEntry");
        topicEntryBindingsClass[sol::meta_function::to_string] = [](const MWDialogue::Entry& topicEntry) {
            return "MWDialogue_Journal_Topic_WrittenEntry: " + topicEntry.mInfoId.toDebugString();
        };
        topicEntryBindingsClass["id"] = sol::readonly_property(
            [](const MWDialogue::Entry& topicEntry) { return topicEntry.mInfoId.serializeText(); });
        topicEntryBindingsClass["text"] = sol::readonly_property(
            [](const MWDialogue::Entry& topicEntry) -> std::string_view { return topicEntry.mText; });
        topicEntryBindingsClass["actor"] = sol::readonly_property(
            [](const MWDialogue::Entry& topicEntry) -> std::string_view { return topicEntry.mActorName; });
    }

    void addJournalClassJournalEntriesListBindings(sol::state_view& lua, const MWBase::Journal* journal)
    {
        auto journalEntriesBindingsClass = lua.new_usertype<MWLua::JournalEntries>("MWDialogue_Journal_WrittenEntries");
        journalEntriesBindingsClass[sol::meta_function::to_string] = [journal](const MWLua::JournalEntries&) {
            const size_t numberOfEntries = journal->getEntries().size();
            return "{MWDialogue_Journal_WrittenEntries: " + std::to_string(numberOfEntries) + " journal text entries}";
        };
        journalEntriesBindingsClass[sol::meta_function::length]
            = [journal](const MWLua::JournalEntries&) { return journal->getEntries().size(); };
        journalEntriesBindingsClass[sol::meta_function::index]
            = [journal](const MWLua::JournalEntries&, size_t index) -> const MWDialogue::StampedJournalEntry* {
            if (index == 0 || index > journal->getEntries().size())
                return nullptr;
            index = LuaUtil::fromLuaIndex(index);
            return &journal->getEntries()[index];
        };
        journalEntriesBindingsClass[sol::meta_function::ipairs] = lua["ipairsForArray"].template get<sol::function>();
        journalEntriesBindingsClass[sol::meta_function::pairs] = lua["ipairsForArray"].template get<sol::function>();
    }

    void addJournalClassJournalEntryBindings(sol::state_view& lua, const MWBase::Journal* journal)
    {
        auto journalEntryBindingsClass
            = lua.new_usertype<MWDialogue::StampedJournalEntry>("MWDialogue_Journal_WrittenEntry");
        journalEntryBindingsClass[sol::meta_function::to_string]
            = [](const MWDialogue::StampedJournalEntry& journalEntry) {
                  return "MWDialogue_Journal_WrittenEntry: " + journalEntry.mTopic.toDebugString();
              };
        journalEntryBindingsClass["id"] = sol::readonly_property(
            [](const MWDialogue::StampedJournalEntry& journalEntry) { return journalEntry.mInfoId.serializeText(); });
        journalEntryBindingsClass["text"] = sol::readonly_property(
            [](const MWDialogue::StampedJournalEntry& journalEntry) -> std::string_view { return journalEntry.mText; });
        journalEntryBindingsClass["questId"] = sol::readonly_property(
            [](const MWDialogue::StampedJournalEntry& journalEntry) { return journalEntry.mTopic.serializeText(); });
        journalEntryBindingsClass["day"] = sol::readonly_property(
            [](const MWDialogue::StampedJournalEntry& journalEntry) { return journalEntry.mDay; });
        journalEntryBindingsClass["month"] = sol::readonly_property(
            [](const MWDialogue::StampedJournalEntry& journalEntry) { return journalEntry.mMonth + 1; });
        journalEntryBindingsClass["dayOfMonth"] = sol::readonly_property(
            [](const MWDialogue::StampedJournalEntry& journalEntry) { return journalEntry.mDayOfMonth; });
    }

    void addJournalEntryBindings(sol::table& playerBindings, sol::state_view lua, const MWBase::Journal* journal)
    {
        playerBindings["journal"] = [journal](const Object& player) -> const MWBase::Journal* {
            verifyPlayer(player);
            return journal;
        };

        addJournalClassBindings(lua, journal);
        addJournalClassTopicsListBindings(lua, journal);
        addJournalClassTopicBindings(lua, journal);
        addJournalClassTopicEntriesListBindings(lua, journal);
        addJournalClassTopicEntryBindings(lua, journal);
        addJournalClassJournalEntriesListBindings(lua, journal);
        addJournalClassJournalEntryBindings(lua, journal);
    }

    // A purchase checks these when the script asks for it, and again when the queued action runs, because an
    // earlier action may have spent the gold or already made the same purchase.

    // Why the player can't buy training in @a skill from @a trainer now, or nothing if they can.
    static std::optional<std::string> getTrainingProblem(const MWWorld::Ptr& trainer, ESM::RefId skill)
    {
        const std::vector<ESM::RefId> skills = MWMechanics::getTrainingSkills(trainer);
        if (std::find(skills.begin(), skills.end(), skill) == skills.end())
            return "The trainer doesn't teach " + skill.toDebugString();
        if (MWMechanics::getTrainingRefusal(trainer, skill) != MWMechanics::TrainingRefusal::None)
            return "The trainer can't raise " + skill.toDebugString();
        if (!canPay(MWMechanics::getTrainingPrice(trainer, skill)))
            return "The player doesn't have enough gold";
        return std::nullopt;
    }

    // Why the player can't buy @a spell from @a merchant now, or nothing if they can.
    static std::optional<std::string> getSpellPurchaseProblem(const MWWorld::Ptr& merchant, const ESM::Spell& spell)
    {
        if (!MWMechanics::isSpellForSale(merchant, spell))
            return "The merchant doesn't sell " + spell.mId.toDebugString();
        if (!canPay(MWMechanics::getSpellPrice(merchant, spell)))
            return "The player doesn't have enough gold";
        return std::nullopt;
    }

    // Why the player can't pay @a merchant to repair @a item now, or nothing if they can.
    static std::optional<std::string> getRepairPurchaseProblem(const MWWorld::Ptr& merchant, const MWWorld::Ptr& item)
    {
        const MWWorld::Ptr player = MWBase::Environment::get().getWorld()->getPlayerPtr();
        if (item.getContainerStore() != &player.getClass().getContainerStore(player))
            return "The item must be in the player's inventory";
        if (!MWMechanics::canMerchantRepair(item))
            return "Merchants can't repair this item";
        if (!canPay(MWMechanics::getRepairPrice(merchant, item)))
            return "The player doesn't have enough gold";
        return std::nullopt;
    }

    void addPlayerBindings(sol::table player, const Context& context)
    {
        MWBase::Journal* const journal = MWBase::Environment::get().getJournal();

        sol::state_view lua = context.sol();
        addJournalEntryBindings(player, lua, journal);

        player["quests"] = [](const Object& object) {
            verifyPlayer(object);
            bool allowChanges = object.isGObject() || object.isSelfObject();
            return Quests{ .mMutable = allowChanges };
        };
        sol::usertype<Quests> quests = lua.new_usertype<Quests>("Quests");
        quests[sol::meta_function::to_string] = [](const Quests& /*self*/) { return "Quests"; };
        quests[sol::meta_function::index] = [](const Quests& self, std::string_view questId) -> sol::optional<Quest> {
            ESM::RefId quest = ESM::RefId::deserializeText(questId);
            const ESM::Dialogue* dial = MWBase::Environment::get().getESMStore()->get<ESM::Dialogue>().search(quest);
            if (dial == nullptr || dial->mType != ESM::Dialogue::Journal)
                return sol::nullopt;
            return Quest{ .mQuestId = quest, .mMutable = self.mMutable };
        };
        quests[sol::meta_function::pairs] = [journal](const Quests& self) {
            std::vector<ESM::RefId> ids;
            for (const auto& [id, _] : journal->getQuests())
                ids.push_back(id);
            size_t i = 0;
            return [ids = std::move(ids), i,
                       allowChanges = self.mMutable]() mutable -> sol::optional<std::tuple<std::string, Quest>> {
                if (i >= ids.size())
                    return sol::nullopt;
                const ESM::RefId& id = ids[i++];
                return std::make_tuple(id.serializeText(), Quest{ .mQuestId = id, .mMutable = allowChanges });
            };
        };

        sol::usertype<Quest> quest = lua.new_usertype<Quest>("Quest");
        quest[sol::meta_function::to_string]
            = [](const Quest& self) { return "Quest[" + self.mQuestId.serializeText() + "]"; };

        auto getQuestStage = [journal](const Quest& self) -> int {
            const MWDialogue::Quest* questPtr = journal->getQuestOrNull(self.mQuestId);
            if (questPtr == nullptr)
                return 0;
            return journal->getJournalIndex(self.mQuestId);
        };
        auto setQuestStage = [context](const Quest& self, int stage) {
            if (!self.mMutable)
                throw std::runtime_error("Value can only be changed in global or player scripts!");
            context.mLuaManager->addAction(
                [self, stage] { MWBase::Environment::get().getJournal()->setJournalIndex(self.mQuestId, stage); },
                "setQuestStageAction");
        };
        quest["stage"] = sol::property(getQuestStage, setQuestStage);

        quest["id"] = sol::readonly_property([](const Quest& q) -> std::string { return q.mQuestId.serializeText(); });
        quest["started"] = sol::readonly_property(
            [journal](const Quest& q) { return journal->getQuestOrNull(q.mQuestId) != nullptr; });
        quest["finished"] = sol::property(
            [journal](const Quest& q) -> bool {
                const MWDialogue::Quest* questPtr = journal->getQuestOrNull(q.mQuestId);
                if (questPtr == nullptr)
                    return false;
                return questPtr->isFinished();
            },
            [journal, context](const Quest& q, bool finished) {
                if (!q.mMutable)
                    throw std::runtime_error("Value can only be changed in global or player scripts!");
                context.mLuaManager->addAction(
                    [q, finished, journal] { journal->getOrStartQuest(q.mQuestId).setFinished(finished); },
                    "setQuestFinishedAction");
            });
        quest["addJournalEntry"] = [context](const Quest& q, int stage, sol::optional<GObject> actor) {
            if (!q.mMutable)
                throw std::runtime_error("Can only be used in global or player scripts!");
            // The journal mwscript function has a try function here, we will make the lua function throw an
            // error. However, the addAction will cause it to error outside of this function.
            context.mLuaManager->addAction(
                [actor = std::move(actor), q, stage] {
                    MWWorld::Ptr actorPtr;
                    if (actor)
                        actorPtr = actor->ptr();
                    MWBase::Environment::get().getJournal()->addEntry(q.mQuestId, stage, actorPtr);
                },
                "addJournalEntryAction");
        };

        player["CONTROL_SWITCH"]
            = LuaUtil::makeStrictReadOnly(LuaUtil::tableFromPairs<std::string_view, std::string_view>(lua,
                {
                    { "Controls", "playercontrols" },
                    { "Fighting", "playerfighting" },
                    { "Jumping", "playerjumping" },
                    { "Looking", "playerlooking" },
                    { "Magic", "playermagic" },
                    { "ViewMode", "playerviewswitch" },
                    { "VanityMode", "vanitymode" },
                }));

        MWBase::InputManager* input = MWBase::Environment::get().getInputManager();
        player["getControlSwitch"] = [input](const Object& object, std::string_view key) {
            verifyPlayer(object);
            return input->getControlSwitch(key);
        };
        player["setControlSwitch"] = [input](const Object& object, std::string_view key, bool v) {
            verifyPlayer(object);
            if (object.isLObject() && !object.isSelfObject())
                throw std::runtime_error("Only player and global scripts can toggle control switches.");
            input->toggleControlSwitch(key, v);
        };
        player["isTeleportingEnabled"] = [](const Object& object) -> bool {
            verifyPlayer(object);
            return MWBase::Environment::get().getWorld()->isTeleportingEnabled();
        };
        player["setTeleportingEnabled"] = [](const Object& object, bool state) {
            verifyPlayer(object);
            if (object.isLObject() && !object.isSelfObject())
                throw std::runtime_error("Only player and global scripts can toggle teleportation.");
            MWBase::Environment::get().getWorld()->enableTeleporting(state);
        };
        player["addTopic"] = [](const Object& object, std::string_view topicId) {
            verifyPlayer(object);

            ESM::RefId topic = ESM::RefId::deserializeText(topicId);
            const ESM::Dialogue* dialogueRecord
                = MWBase::Environment::get().getESMStore()->get<ESM::Dialogue>().search(topic);

            if (!dialogueRecord)
                throw std::runtime_error(
                    "Failed to add topic \"" + std::string(topicId) + "\": topic record not found");

            if (dialogueRecord->mType != ESM::Dialogue::Topic)
                throw std::runtime_error("Failed to add topic \"" + std::string(topicId) + "\": record is not a topic");

            MWBase::Environment::get().getDialogueManager()->addTopic(topic);
        };
        player["sendMenuEvent"] = [context](const Object& object, std::string eventName, const sol::object& eventData) {
            verifyPlayer(object);
            context.mLuaEvents->addMenuEvent({ std::move(eventName), LuaUtil::serialize(eventData) });
        };

        player["getCrimeLevel"] = [](const Object& o) -> int {
            const MWWorld::Class& cls = o.ptr().getClass();
            return cls.getNpcStats(o.ptr()).getBounty();
        };
        player["setCrimeLevel"] = [](const Object& o, int amount) {
            verifyPlayer(o);
            if (!o.isGObject())
                throw std::runtime_error("Only global scripts can change crime level");
            const MWWorld::Class& cls = o.ptr().getClass();
            cls.getNpcStats(o.ptr()).setBounty(amount);
            if (amount == 0)
                MWBase::Environment::get().getWorld()->getPlayer().recordCrimeId();
        };
        player["buyTraining"] = [context](const Object& buyer, const Object& trainer, std::string_view skill) {
            verifyBuyer(buyer);
            const MWWorld::Ptr trainerPtr = trainer.ptr();
            verifyNpc(trainerPtr.getClass());
            const ESM::RefId skillId = parseSkillId(skill);
            if (const std::optional<std::string> problem = getTrainingProblem(trainerPtr, skillId))
                throw std::runtime_error(*problem);
            context.mLuaManager->addAction(
                [trainer = Object(trainerPtr), skillId] {
                    const MWWorld::Ptr ptr = trainer.ptr();
                    if (const std::optional<std::string> problem = getTrainingProblem(ptr, skillId))
                    {
                        Log(Debug::Warning) << "The player didn't buy training: " << *problem;
                        return;
                    }
                    MWMechanics::trainSkill(ptr, skillId);
                    MWMechanics::passTrainingTime();
                },
                "BuyTrainingAction");
        };
        player["buySpell"] = [context](const Object& buyer, const Object& merchant, const sol::object& spellOrId) {
            verifyBuyer(buyer);
            const MWWorld::Ptr merchantPtr = merchant.ptr();
            verifyActor(merchantPtr.getClass());
            const ESM::Spell* spell = toSpell(spellOrId);
            if (const std::optional<std::string> problem = getSpellPurchaseProblem(merchantPtr, *spell))
                throw std::runtime_error(*problem);
            context.mLuaManager->addAction(
                [merchant = Object(merchantPtr), spell] {
                    const MWWorld::Ptr ptr = merchant.ptr();
                    if (const std::optional<std::string> problem = getSpellPurchaseProblem(ptr, *spell))
                    {
                        Log(Debug::Warning) << "The player didn't buy a spell: " << *problem;
                        return;
                    }
                    MWMechanics::buySpell(ptr, *spell);
                },
                "BuySpellAction");
        };
        player["buyRepair"] = [context](const Object& buyer, const Object& merchant, const Object& item) {
            verifyBuyer(buyer);
            const MWWorld::Ptr merchantPtr = merchant.ptr();
            verifyActor(merchantPtr.getClass());
            const MWWorld::Ptr itemPtr = item.ptr();
            if (const std::optional<std::string> problem = getRepairPurchaseProblem(merchantPtr, itemPtr))
                throw std::runtime_error(*problem);
            context.mLuaManager->addAction(
                [merchant = Object(merchantPtr), item = Object(itemPtr)] {
                    const MWWorld::Ptr ptr = merchant.ptr();
                    const MWWorld::Ptr repaired = item.ptr();
                    if (const std::optional<std::string> problem = getRepairPurchaseProblem(ptr, repaired))
                    {
                        Log(Debug::Warning) << "The player didn't buy a repair: " << *problem;
                        return;
                    }
                    MWMechanics::repairItem(ptr, repaired);
                },
                "BuyRepairAction");
        };

        player["isCharGenFinished"] = [](const Object&) -> bool {
            return MWBase::Environment::get().getWorld()->getGlobalFloat(MWWorld::Globals::sCharGenState) == -1;
        };
        player["getRestStatus"] = [](sol::this_state thisState, const Object& object) {
            verifyPlayer(object);
            const int flags = MWBase::Environment::get().getWorld()->canRest();
            sol::table result(thisState, sol::create);
            result["isUnderwater"] = (flags & MWBase::World::Rest_PlayerIsUnderwater) != 0;
            result["isInAir"] = (flags & MWBase::World::Rest_PlayerIsInAir) != 0;
            result["isEnemyNearby"] = (flags & MWBase::World::Rest_EnemiesAreNearby) != 0;
            result["canSleep"] = (flags & MWBase::World::Rest_CanSleep) != 0;
            return result;
        };

        player["OFFENSE_TYPE"]
            = LuaUtil::makeStrictReadOnly(LuaUtil::tableFromPairs<std::string_view, int>(context.sol(),
                { { "Theft", MWBase::MechanicsManager::OffenseType::OT_Theft },
                    { "Assault", MWBase::MechanicsManager::OffenseType::OT_Assault },
                    { "Murder", MWBase::MechanicsManager::OffenseType::OT_Murder },
                    { "Trespassing", MWBase::MechanicsManager::OffenseType::OT_Trespassing },
                    { "SleepingInOwnedBed", MWBase::MechanicsManager::OffenseType::OT_SleepingInOwnedBed },
                    { "Pickpocket", MWBase::MechanicsManager::OffenseType::OT_Pickpocket } }));
        player["_runStandardCommitCrime"] = [](const Object& o, const sol::optional<Object> victim, int type,
                                                std::string_view faction, int arg = 0, bool victimAware = false) {
            verifyPlayer(o);
            if (victim.has_value() && !victim->ptrOrEmpty().isEmpty())
                verifyNpc(victim->ptrOrEmpty().getClass());
            if (!o.isGObject())
                throw std::runtime_error("Only global scripts can commit crime");
            if (type < 0 || type > MWBase::MechanicsManager::OffenseType::OT_Pickpocket)
                throw std::runtime_error("Invalid offense type");

            ESM::RefId factionId = parseFactionId(faction);
            // If the faction is provided but not found, error out
            if (faction != "" && factionId == ESM::RefId())
                throw std::runtime_error("Faction does not exist");

            MWWorld::Ptr victimObj = nullptr;
            if (victim.has_value())
                victimObj = victim->ptrOrEmpty();
            return MWBase::Environment::get().getMechanicsManager()->commitCrime(o.ptr(), victimObj,
                static_cast<MWBase::MechanicsManager::OffenseType>(type), factionId, arg, victimAware);
        };

        player["birthSigns"] = initBirthSignRecordBindings(context);
        player["getBirthSign"] = [](const Object& object) -> std::string {
            verifyPlayer(object);
            return MWBase::Environment::get().getWorld()->getPlayer().getBirthSign().serializeText();
        };
        player["setBirthSign"] = [](const Object& object, const sol::object& recordOrId) {
            verifyPlayer(object);
            if (!object.isGObject())
                throw std::runtime_error("Only global scripts can change birth signs");
            MWBase::Environment::get().getWorld()->getPlayer().setBirthSign(toBirthSignId(recordOrId));
        };
    }
}
