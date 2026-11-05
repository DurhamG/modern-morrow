#include "types.hpp"

#include <algorithm>
#include <optional>

#include <components/esm/attr.hpp>
#include <components/esm3/loadappa.hpp>
#include <components/esm3/loadbsgn.hpp>
#include <components/esm3/loadcrea.hpp>
#include <components/esm3/loadfact.hpp>
#include <components/esm3/loadingr.hpp>
#include <components/esm3/loadmgef.hpp>
#include <components/esm3/loadmisc.hpp>
#include <components/esm3/loadrepa.hpp>
#include <components/esm3/loadskil.hpp>
#include <components/lua/util.hpp>
#include <components/settings/values.hpp>

#include "../birthsignbindings.hpp"
#include "../luamanagerimp.hpp"
#include "../magicbindings.hpp"
#include "../magictypebindings.hpp"

#include "apps/openmw/mwbase/dialoguemanager.hpp"
#include "apps/openmw/mwbase/inputmanager.hpp"
#include "apps/openmw/mwbase/journal.hpp"
#include "apps/openmw/mwbase/mechanicsmanager.hpp"
#include "apps/openmw/mwbase/world.hpp"
#include "apps/openmw/mwmechanics/actorutil.hpp"
#include "apps/openmw/mwmechanics/alchemy.hpp"
#include "apps/openmw/mwmechanics/barter.hpp"
#include "apps/openmw/mwmechanics/enchanting.hpp"
#include "apps/openmw/mwmechanics/magiceffects.hpp"
#include "apps/openmw/mwmechanics/npcstats.hpp"
#include "apps/openmw/mwmechanics/pickpocket.hpp"
#include "apps/openmw/mwmechanics/recharge.hpp"
#include "apps/openmw/mwmechanics/repair.hpp"
#include "apps/openmw/mwmechanics/services.hpp"
#include "apps/openmw/mwmechanics/spellmaking.hpp"
#include "apps/openmw/mwmechanics/spellutil.hpp"
#include "apps/openmw/mwworld/class.hpp"
#include "apps/openmw/mwworld/containerstore.hpp"
#include "apps/openmw/mwworld/esmstore.hpp"
#include "apps/openmw/mwworld/globals.hpp"
#include "apps/openmw/mwworld/manualref.hpp"
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

    // Taking an item changes its container as well as the player, so only global scripts can do it.
    static void verifyTaker(const Object& player)
    {
        verifyPlayer(player);
        if (!player.isGObject())
            throw std::runtime_error("Only global scripts can take items");
    }

    static void verifyVictim(const MWWorld::Ptr& victim)
    {
        if (!victim.getClass().isNpc() || victim.getClass().getCreatureStats(victim).isDead())
            throw std::runtime_error("The victim must be a living NPC");
    }

    static void verifyIsIn(const MWWorld::Ptr& item, const MWWorld::Ptr& container)
    {
        if (item.getContainerStore() != &container.getClass().getContainerStore(container))
            throw std::runtime_error("The item must be in " + container.toString());
    }

    // The number of items to take from a stack: all of them, unless the script asks for fewer.
    static int getCountToTake(const MWWorld::Ptr& item, const sol::optional<int>& count)
    {
        const int available = item.getCellRef().getCount();
        const int result = count.value_or(available);
        if (result <= 0 || result > available)
            throw std::runtime_error(
                "Can't take " + std::to_string(result) + " of " + std::to_string(available) + " items");
        return result;
    }

    // The number of items that a queued action can still take, after the actions before it ran.
    static int getCountLeft(const MWWorld::Ptr& item, const MWWorld::Ptr& container, int count)
    {
        if (item.getContainerStore() != &container.getClass().getContainerStore(container))
            return 0;
        return std::min(count, item.getCellRef().getCount());
    }

    // Moves items from their container into the player's inventory, the same way as moveInto.
    static void moveToPlayer(const MWWorld::Ptr& item, int count)
    {
        const MWWorld::Ptr player = MWMechanics::getPlayer();
        MWWorld::ContainerStore* container = item.getContainerStore();
        player.getClass().getContainerStore(player).add(item, count, false);
        container->remove(item, count, false);
    }

    static std::optional<LuaUtil::Callback> toCallback(const sol::optional<sol::table>& callback)
    {
        if (!callback)
            return std::nullopt;
        return LuaUtil::Callback::fromLua(*callback);
    }

    // Gives the outcome of a queued action to the script's callback, if it passed one.
    template <class T>
    static void reportOutcome(const Context& context, const std::optional<LuaUtil::Callback>& callback, T outcome)
    {
        if (callback)
            context.mLuaManager->queueCallback(
                *callback, sol::main_object(context.mLua->unsafeState(), sol::in_place, outcome));
    }

    static void addTakingBindings(sol::table& playerBindings, const Context& context)
    {
        playerBindings["isAllowedToUse"] = [](const Object& player, const Object& object) -> bool {
            verifyPlayer(player);
            MWWorld::Ptr victim;
            return MWBase::Environment::get().getMechanicsManager()->isAllowedToUse(player.ptr(), object.ptr(), victim);
        };
        playerBindings["takeItem"]
            = [context](const Object& player, const Object& container, const Object& item, sol::optional<int> count) {
                  verifyTaker(player);
                  const MWWorld::Ptr containerPtr = container.ptr();
                  const MWWorld::Ptr itemPtr = item.ptr();
                  if (containerPtr == player.ptr())
                      throw std::runtime_error("The item must not be in the player's inventory");
                  verifyIsIn(itemPtr, containerPtr);
                  if (MWBase::Environment::get().getMechanicsManager()->isBoundItem(itemPtr))
                      throw std::runtime_error("Bound items can't be taken");
                  const int countToTake = getCountToTake(itemPtr, count);
                  context.mLuaManager->addAction(
                      [container = Object(containerPtr), item = Object(itemPtr), countToTake] {
                          const MWWorld::Ptr from = container.ptr();
                          const MWWorld::Ptr taken = item.ptr();
                          const int countLeft = getCountLeft(taken, from, countToTake);
                          if (countLeft <= 0)
                              return;
                          MWBase::Environment::get().getMechanicsManager()->itemTaken(
                              MWMechanics::getPlayer(), taken, from, countLeft);
                          moveToPlayer(taken, countLeft);
                      },
                      "TakeItemAction");
              };

        playerBindings["getPickpocketChance"] = [](const Object& player, const Object& victim,
                                                    sol::optional<Object> item, sol::optional<int> count) -> int {
            verifyPlayer(player);
            const MWWorld::Ptr victimPtr = victim.ptr();
            verifyNpc(victimPtr.getClass());
            const MWMechanics::Pickpocket pickpocket(player.ptr(), victimPtr);
            if (!item)
                return pickpocket.getFinishChance();
            const MWWorld::Ptr itemPtr = item->ptr();
            return pickpocket.getChance(itemPtr, getCountToTake(itemPtr, count));
        };
        playerBindings["getPickpocketItems"]
            = [](sol::this_state thisState, const Object& player, const Object& victim) -> sol::table {
            verifyPlayer(player);
            const MWWorld::Ptr victimPtr = victim.ptr();
            verifyVictim(victimPtr);
            const MWMechanics::Pickpocket pickpocket(player.ptr(), victimPtr);
            const bool knockedDown = victimPtr.getClass().getCreatureStats(victimPtr).getKnockedDown();
            sol::table result(thisState, sol::create);
            MWWorld::ContainerStore& store = victimPtr.getClass().getContainerStore(victimPtr);
            for (MWWorld::ContainerStoreIterator it = store.begin(); it != store.end(); ++it)
            {
                if (!it->getClass().showsInInventory(*it) || !pickpocket.canSteal(*it))
                    continue;
                if (!knockedDown && !pickpocket.noticesItem())
                    continue;
                if (victim.isGObject())
                    result.add(GObject(*it));
                else
                    result.add(LObject(*it));
            }
            return result;
        };
        playerBindings["pickpocket"] = [context](const Object& player, const Object& victim, const Object& item,
                                           sol::optional<int> count, sol::optional<sol::table> callback) {
            verifyTaker(player);
            const MWWorld::Ptr victimPtr = victim.ptr();
            verifyVictim(victimPtr);
            const MWWorld::Ptr itemPtr = item.ptr();
            verifyIsIn(itemPtr, victimPtr);
            if (!MWMechanics::Pickpocket(player.ptr(), victimPtr).canSteal(itemPtr))
                throw std::runtime_error("Equipped items and bound items can't be stolen");
            const int countToTake = getCountToTake(itemPtr, count);
            context.mLuaManager->addAction(
                [context, victim = Object(victimPtr), item = Object(itemPtr), countToTake,
                    callback = toCallback(callback)] {
                    const MWWorld::Ptr from = victim.ptr();
                    const MWWorld::Ptr stolen = item.ptr();
                    const int countLeft = getCountLeft(stolen, from, countToTake);
                    // An earlier action may have taken the items. Then nothing is stolen, and nobody can notice.
                    if (countLeft <= 0)
                    {
                        reportOutcome(context, callback, sol::lua_nil);
                        return;
                    }
                    const MWWorld::Ptr thief = MWMechanics::getPlayer();
                    bool detected = false;
                    if (from.getClass().getCreatureStats(from).getKnockedDown())
                        MWBase::Environment::get().getMechanicsManager()->itemTaken(thief, stolen, from, countLeft);
                    else
                    {
                        const MWMechanics::Pickpocket pickpocket(thief, from);
                        detected = pickpocket.pick(stolen, countLeft);
                        if (detected)
                            pickpocket.commitCrime();
                        else
                            pickpocket.recordTheft(stolen, countLeft);
                    }
                    if (!detected)
                        moveToPlayer(stolen, countLeft);
                    reportOutcome(context, callback, detected);
                },
                "PickpocketAction");
        };
        playerBindings["finishPickpocketing"]
            = [context](const Object& player, const Object& victim, sol::optional<sol::table> callback) {
                  verifyTaker(player);
                  const MWWorld::Ptr victimPtr = victim.ptr();
                  verifyVictim(victimPtr);
                  context.mLuaManager->addAction(
                      [context, victim = Object(victimPtr), callback = toCallback(callback)] {
                          const MWMechanics::Pickpocket pickpocket(MWMechanics::getPlayer(), victim.ptr());
                          const bool detected = pickpocket.finish();
                          if (detected)
                              pickpocket.commitCrime();
                          reportOutcome(context, callback, detected);
                      },
                      "FinishPickpocketingAction");
              };
    }

    // Using a tool changes the items in the player's inventory, which only global scripts can do.
    static void verifyCrafter(const Object& player)
    {
        verifyPlayer(player);
        if (!player.isGObject())
            throw std::runtime_error("Only global scripts can change the player's items");
    }

    static void verifyFilledSoulGem(const MWWorld::Ptr& gem)
    {
        const ESM::RefId& soul = gem.getCellRef().getSoul();
        if (gem.getType() != ESM::Miscellaneous::sRecordId || soul.empty()
            || !MWBase::Environment::get().getESMStore()->get<ESM::Creature>().search(soul))
            throw std::runtime_error("A soul gem with a soul expected");
    }

    static void addToolBindings(sol::table& playerBindings, const Context& context)
    {
        playerBindings["getRepairChance"] = [](const Object& player) -> int {
            verifyPlayer(player);
            return MWMechanics::getRepairChance();
        };
        playerBindings["getRechargeChance"] = [](const Object& player) -> int {
            verifyPlayer(player);
            return MWMechanics::getRechargeChance();
        };
        playerBindings["repairItem"] = [context](const Object& player, const Object& item, const Object& tool,
                                           sol::optional<sol::table> callback) {
            verifyCrafter(player);
            const MWWorld::Ptr itemPtr = item.ptr();
            const MWWorld::Ptr toolPtr = tool.ptr();
            verifyIsIn(itemPtr, player.ptr());
            verifyIsIn(toolPtr, player.ptr());
            if (toolPtr.getType() != ESM::Repair::sRecordId)
                throw std::runtime_error("Repair tool expected");
            if (!MWMechanics::canMerchantRepair(itemPtr))
                throw std::runtime_error("The item must be a weapon or a piece of armor that has lost some condition");
            context.mLuaManager->addAction(
                [context, item = Object(itemPtr), tool = Object(toolPtr), callback = toCallback(callback)] {
                    const MWWorld::Ptr repaired = item.ptr();
                    const MWWorld::Ptr used = tool.ptr();
                    // An earlier action may have used up the tool or repaired the item.
                    if (used.getCellRef().getCount() <= 0 || !MWMechanics::canMerchantRepair(repaired))
                    {
                        reportOutcome(context, callback, false);
                        return;
                    }
                    MWMechanics::Repair repair;
                    repair.setTool(used);
                    reportOutcome(context, callback, repair.repair(repaired));
                },
                "RepairItemAction");
        };
        playerBindings["rechargeItem"] = [context](const Object& player, const Object& item, const Object& soulGem,
                                             sol::optional<sol::table> callback) {
            verifyCrafter(player);
            const MWWorld::Ptr itemPtr = item.ptr();
            const MWWorld::Ptr gemPtr = soulGem.ptr();
            verifyIsIn(itemPtr, player.ptr());
            verifyIsIn(gemPtr, player.ptr());
            verifyFilledSoulGem(gemPtr);
            if (!MWMechanics::canRecharge(itemPtr))
                throw std::runtime_error("The item must be an enchanted item that has lost some charge");
            context.mLuaManager->addAction(
                [context, item = Object(itemPtr), soulGem = Object(gemPtr), callback = toCallback(callback)] {
                    const MWWorld::Ptr recharged = item.ptr();
                    const MWWorld::Ptr used = soulGem.ptr();
                    // An earlier action may have used up the soul gem or recharged the item.
                    if (used.getCellRef().getCount() <= 0 || !MWMechanics::canRecharge(recharged))
                    {
                        reportOutcome(context, callback, false);
                        return;
                    }
                    reportOutcome(context, callback, MWMechanics::rechargeItem(recharged, used));
                },
                "RechargeItemAction");
        };
    }

    // Throws an error for effects that the spellmaking or enchanting window wouldn't let the player choose.
    static void verifyCustomEffects(const ESM::EffectList& effects, MWMechanics::CustomMagic type, bool isConstant)
    {
        switch (MWMechanics::getCustomEffectProblem(MWMechanics::getPlayer(), effects, type, isConstant))
        {
            case MWMechanics::CustomEffectProblem::None:
                return;
            case MWMechanics::CustomEffectProblem::TooManyEffects:
                throw std::runtime_error(
                    "There can be at most " + std::to_string(MWMechanics::sMaxCustomEffects) + " effects");
            case MWMechanics::CustomEffectProblem::UnknownEffect:
                throw std::runtime_error(type == MWMechanics::CustomMagic::Spell
                        ? "The player doesn't know an effect, or it doesn't allow spellmaking"
                        : "The player doesn't know an effect, or it doesn't allow enchanting");
            case MWMechanics::CustomEffectProblem::RepeatedEffect:
                throw std::runtime_error("An effect is there twice");
            case MWMechanics::CustomEffectProblem::RangeNotAllowed:
                throw std::runtime_error("An effect has a range that it doesn't allow");
            case MWMechanics::CustomEffectProblem::InvalidParameters:
                throw std::runtime_error(
                    "An effect lacks its skill or attribute, its magnitude, duration or area is out of bounds, or an "
                    "effect on self has an area");
        }
    }

    static sol::table toMagicEffectList(sol::this_state thisState, MWMechanics::CustomMagic type)
    {
        sol::table result(thisState, sol::create);
        for (const ESM::MagicEffect* effect : MWMechanics::getCustomMagicEffects(MWMechanics::getPlayer(), type))
            result.add(effect);
        return result;
    }

    // What a script picked for enchanting, as it describes it in a Lua table. The items are kept as Lua objects,
    // so that a queued action can find them again.
    struct EnchantingSetup
    {
        std::optional<Object> mEnchanter;
        std::optional<Object> mItem;
        std::optional<Object> mSoulGem;
        ESM::EffectList mEffects;
        std::optional<std::string> mName;
        std::optional<int> mType;
    };

    template <class T>
    static std::optional<T> getField(const sol::table& table, std::string_view key)
    {
        if (sol::optional<T> value = table.get<sol::optional<T>>(key))
            return *value;
        return std::nullopt;
    }

    static EnchantingSetup toEnchantingSetup(const sol::table& table)
    {
        EnchantingSetup setup;
        setup.mEnchanter = getField<Object>(table, "enchanter");
        setup.mItem = getField<Object>(table, "item");
        setup.mSoulGem = getField<Object>(table, "soulGem");
        if (const std::optional<sol::table> effects = getField<sol::table>(table, "effects"))
            setup.mEffects = tableToEffectList(*effects);
        setup.mName = getField<std::string>(table, "name");
        setup.mType = getField<int>(table, "type");
        return setup;
    }

    // Sets up the engine's enchanting the way the enchanting window would for the same choices.
    static MWMechanics::Enchanting makeEnchanting(const EnchantingSetup& setup)
    {
        const MWWorld::Ptr player = MWMechanics::getPlayer();
        MWMechanics::Enchanting enchanting;
        if (setup.mEnchanter)
        {
            const MWWorld::Ptr enchanter = setup.mEnchanter->ptr();
            verifyActor(enchanter.getClass());
            enchanting.setSelfEnchanting(false);
            enchanting.setEnchanter(enchanter);
        }
        else
        {
            enchanting.setSelfEnchanting(true);
            enchanting.setEnchanter(player);
        }

        if (setup.mItem)
        {
            const MWWorld::Ptr item = setup.mItem->ptr();
            verifyIsIn(item, player);
            if (!MWMechanics::Enchanting::canEnchant(item))
                throw std::runtime_error("The item can't be enchanted");
            enchanting.setOldItem(item);
            enchanting.setNewItemName(setup.mName.value_or(std::string(item.getClass().getName(item))));
        }
        else if (setup.mName)
            enchanting.setNewItemName(*setup.mName);

        if (setup.mSoulGem)
        {
            const MWWorld::Ptr gem = setup.mSoulGem->ptr();
            verifyIsIn(gem, player);
            verifyFilledSoulGem(gem);
            enchanting.setSoulGem(gem);
        }

        enchanting.setEffect(setup.mEffects);

        const std::vector<int> castStyles = enchanting.getCastStyles();
        if (setup.mType)
        {
            if (std::find(castStyles.begin(), castStyles.end(), *setup.mType) == castStyles.end())
                throw std::runtime_error("The item can't be enchanted with cast type " + std::to_string(*setup.mType));
            enchanting.setCastStyle(*setup.mType);
        }
        else if (!castStyles.empty())
            enchanting.setCastStyle(castStyles.front());
        return enchanting;
    }

    // Throws an error for effects that the enchanting window wouldn't let the player choose for the cast type.
    static void verifyEnchantingEffects(const EnchantingSetup& setup, const MWMechanics::Enchanting& enchanting)
    {
        verifyCustomEffects(setup.mEffects, MWMechanics::CustomMagic::Enchantment,
            enchanting.getCastStyle() == ESM::Enchantment::ConstantEffect);
    }

    static void verifyCanEnchant(const MWMechanics::Enchanting& enchanting)
    {
        switch (enchanting.getProblem())
        {
            case MWMechanics::Enchanting::Problem::None:
                return;
            case MWMechanics::Enchanting::Problem::NoEffects:
                throw std::runtime_error("The enchantment needs at least one effect");
            case MWMechanics::Enchanting::Problem::NoName:
                throw std::runtime_error("The enchanted item needs a name");
            case MWMechanics::Enchanting::Problem::NoSoulGem:
                throw std::runtime_error("Enchanting needs a soul gem");
            case MWMechanics::Enchanting::Problem::NoItem:
                throw std::runtime_error("Enchanting needs an item");
            case MWMechanics::Enchanting::Problem::TooManyPoints:
                throw std::runtime_error("The effects need more enchantment points than the item has");
            case MWMechanics::Enchanting::Problem::NotEnoughGold:
                throw std::runtime_error("The player doesn't have enough gold");
        }
    }

    // Sets up an engine object, such as MWMechanics::Enchanting, again in a queued action. Setting it up throws an
    // error when an earlier action used up an item that it needs. The queued action can't give that error to the
    // script, so this returns nothing instead.
    template <class Function>
    static auto setUpAgain(const Function& setUp) -> std::optional<decltype(setUp())>
    {
        try
        {
            return setUp();
        }
        catch (const std::runtime_error&)
        {
            return std::nullopt;
        }
    }

    static void addEnchantingBindings(sol::table& playerBindings, const Context& context)
    {
        playerBindings["getEnchantingEffects"] = [](sol::this_state thisState, const Object& player) -> sol::table {
            verifyPlayer(player);
            return toMagicEffectList(thisState, MWMechanics::CustomMagic::Enchantment);
        };
        playerBindings["getEnchantingInfo"]
            = [](sol::this_state thisState, const Object& player, const sol::table& table) -> sol::table {
            verifyPlayer(player);
            const EnchantingSetup setup = toEnchantingSetup(table);
            const MWMechanics::Enchanting enchanting = makeEnchanting(setup);
            verifyEnchantingEffects(setup, enchanting);
            const bool selfEnchanting = !setup.mEnchanter.has_value();
            sol::table result(thisState, sol::create);
            result["points"] = static_cast<int>(enchanting.getEnchantPoints(false));
            result["maxPoints"] = enchanting.getMaxEnchantValue();
            result["charge"] = enchanting.getGemCharge();
            result["castCost"] = enchanting.getEffectiveCastCost();
            // An enchanter never fails. Only the player's own attempts roll against the chance.
            result["chance"] = selfEnchanting ? std::clamp(enchanting.getEnchantChance(), 0, 100) : 100;
            result["price"] = selfEnchanting ? 0 : enchanting.getEnchantPrice();
            result["type"] = enchanting.getCastStyle();
            sol::table types(thisState, sol::create);
            for (int castStyle : enchanting.getCastStyles())
                types.add(castStyle);
            result["types"] = types;
            return result;
        };
        playerBindings["enchantItem"]
            = [context](const Object& player, const sol::table& table, sol::optional<sol::table> callback) {
                  verifyCrafter(player);
                  const EnchantingSetup setup = toEnchantingSetup(table);
                  const MWMechanics::Enchanting checked = makeEnchanting(setup);
                  verifyEnchantingEffects(setup, checked);
                  verifyCanEnchant(checked);
                  context.mLuaManager->addAction(
                      [context, setup, callback = toCallback(callback)] {
                          // An earlier action may have spent the gold, or used up the item or the soul gem.
                          std::optional<MWMechanics::Enchanting> enchanting
                              = setUpAgain([&] { return makeEnchanting(setup); });
                          if (!enchanting || enchanting->getProblem() != MWMechanics::Enchanting::Problem::None)
                          {
                              reportOutcome(context, callback, false);
                              return;
                          }
                          const MWWorld::Ptr stolen = enchanting->getStolenItem();
                          if (!stolen.isEmpty())
                          {
                              MWBase::Environment::get().getMechanicsManager()->confiscateStolenItemToOwner(
                                  MWMechanics::getPlayer(), stolen, setup.mEnchanter->ptr(), 1);
                              reportOutcome(context, callback, false);
                              return;
                          }
                          reportOutcome(context, callback, enchanting->create());
                      },
                      "EnchantItemAction");
              };
    }

    // What a script picked for spellmaking, as it describes it in a Lua table. The merchant is kept as a Lua object,
    // so that a queued action can find them again.
    struct SpellmakingSetup
    {
        std::optional<Object> mMerchant;
        ESM::Spell mSpell;
    };

    static SpellmakingSetup toSpellmakingSetup(const sol::table& table)
    {
        SpellmakingSetup setup;
        setup.mMerchant = getField<Object>(table, "merchant");
        if (!setup.mMerchant)
            throw std::runtime_error("Spellmaking needs a merchant");
        verifyActor(setup.mMerchant->ptr().getClass());
        ESM::EffectList effects;
        if (const std::optional<sol::table> list = getField<sol::table>(table, "effects"))
            effects = tableToEffectList(*list);
        verifyCustomEffects(effects, MWMechanics::CustomMagic::Spell, false);
        setup.mSpell = MWMechanics::makeCustomSpell(getField<std::string>(table, "name").value_or(""), effects);
        return setup;
    }

    static void addSpellmakingBindings(sol::table& playerBindings, const Context& context)
    {
        playerBindings["getSpellmakingEffects"] = [](sol::this_state thisState, const Object& player) -> sol::table {
            verifyPlayer(player);
            return toMagicEffectList(thisState, MWMechanics::CustomMagic::Spell);
        };
        playerBindings["getSpellmakingInfo"]
            = [](sol::this_state thisState, const Object& player, const sol::table& table) -> sol::table {
            verifyPlayer(player);
            const SpellmakingSetup setup = toSpellmakingSetup(table);
            sol::table result(thisState, sol::create);
            result["cost"] = setup.mSpell.mData.mCost;
            result["price"] = 0;
            result["chance"] = 0;
            // The spellmaking window shows no price or chance without effects.
            if (!setup.mSpell.mEffects.mList.empty())
            {
                const float chance = MWMechanics::calcSpellBaseSuccessChance(&setup.mSpell, player.ptr(), nullptr);
                result["price"] = MWMechanics::getCustomSpellPrice(setup.mMerchant->ptr(), setup.mSpell.mEffects);
                result["chance"] = std::clamp(static_cast<int>(chance), 0, 100);
            }
            return result;
        };
        playerBindings["makeSpell"]
            = [context](const Object& player, const sol::table& table, sol::optional<sol::table> callback) {
                  verifyBuyer(player);
                  const SpellmakingSetup setup = toSpellmakingSetup(table);
                  switch (MWMechanics::getSpellmakingProblem(setup.mMerchant->ptr(), setup.mSpell))
                  {
                      case MWMechanics::SpellmakingProblem::None:
                          break;
                      case MWMechanics::SpellmakingProblem::NoEffects:
                      case MWMechanics::SpellmakingProblem::NoCost:
                          throw std::runtime_error("The spell needs at least one effect");
                      case MWMechanics::SpellmakingProblem::NoName:
                          throw std::runtime_error("The spell needs a name");
                      case MWMechanics::SpellmakingProblem::NotEnoughGold:
                          throw std::runtime_error("The player doesn't have enough gold");
                  }
                  context.mLuaManager->addAction(
                      [context, setup, callback = toCallback(callback)] {
                          const MWWorld::Ptr merchant = setup.mMerchant->ptr();
                          // An earlier action may have spent the gold.
                          if (MWMechanics::getSpellmakingProblem(merchant, setup.mSpell)
                              != MWMechanics::SpellmakingProblem::None)
                          {
                              reportOutcome(context, callback, sol::lua_nil);
                              return;
                          }
                          const ESM::Spell* spell = MWMechanics::buyCustomSpell(merchant, setup.mSpell);
                          reportOutcome(context, callback, spell->mId.serializeText());
                      },
                      "MakeSpellAction");
              };
    }

    // What a script picked for brewing, as it describes it in a Lua table. The items are kept as Lua objects, so
    // that a queued action can find them again.
    struct AlchemySetup
    {
        std::vector<Object> mIngredients;
        std::optional<std::vector<Object>> mApparatus;
        std::optional<std::string> mName;
        int mCount = 1;
    };

    static std::vector<Object> toObjectList(const sol::table& table)
    {
        std::vector<Object> result;
        for (size_t i = 1; i <= table.size(); ++i)
            result.push_back(LuaUtil::cast<Object>(table[i]));
        return result;
    }

    static AlchemySetup toAlchemySetup(const sol::table& table)
    {
        AlchemySetup setup;
        if (const std::optional<sol::table> ingredients = getField<sol::table>(table, "ingredients"))
            setup.mIngredients = toObjectList(*ingredients);
        if (const std::optional<sol::table> apparatus = getField<sol::table>(table, "apparatus"))
            setup.mApparatus = toObjectList(*apparatus);
        setup.mName = getField<std::string>(table, "name");
        setup.mCount = getField<int>(table, "count").value_or(1);
        if (setup.mCount < 1)
            throw std::runtime_error("The number of potions to brew must be at least 1");
        return setup;
    }

    // Sets up the engine's alchemy the way the alchemy window would for the same choices.
    static MWMechanics::Alchemy makeAlchemy(const AlchemySetup& setup)
    {
        const MWWorld::Ptr player = MWMechanics::getPlayer();
        MWMechanics::Alchemy alchemy;
        alchemy.setAlchemist(player);
        if (setup.mApparatus)
        {
            const auto slots = std::distance(alchemy.beginTools(), alchemy.endTools());
            for (std::ptrdiff_t slot = 0; slot < slots; ++slot)
                alchemy.removeApparatus(static_cast<size_t>(slot));
            for (const Object& object : *setup.mApparatus)
            {
                const MWWorld::Ptr apparatus = object.ptr();
                verifyIsIn(apparatus, player);
                if (apparatus.getType() != ESM::Apparatus::sRecordId)
                    throw std::runtime_error("Apparatus expected");
                alchemy.addApparatus(apparatus);
            }
        }
        for (const Object& object : setup.mIngredients)
        {
            const MWWorld::Ptr ingredient = object.ptr();
            verifyIsIn(ingredient, player);
            if (ingredient.getType() != ESM::Ingredient::sRecordId)
                throw std::runtime_error("Ingredient expected");
            if (alchemy.addIngredient(ingredient) < 0)
                throw std::runtime_error("Brewing takes at most four ingredients, each of a different kind");
        }
        alchemy.setPotionName(setup.mName.value_or(alchemy.suggestPotionName()));
        return alchemy;
    }

    static void addAlchemyBindings(sol::table& playerBindings, const Context& context)
    {
        playerBindings["getKnownIngredientEffects"]
            = [](sol::this_state thisState, const Object& player, const Object& ingredient) -> sol::table {
            verifyPlayer(player);
            const MWWorld::Ptr ingredientPtr = ingredient.ptr();
            if (ingredientPtr.getType() != ESM::Ingredient::sRecordId)
                throw std::runtime_error("Ingredient expected");
            const MWWorld::Ptr playerPtr = player.ptr();
            const float alchemySkill = playerPtr.getClass().getSkill(playerPtr, ESM::Skill::Alchemy);
            const ESM::Ingredient* record = ingredientPtr.get<ESM::Ingredient>()->mBase;
            sol::table result(thisState, sol::create);
            for (uint32_t i = 0; i < 4 && MWMechanics::Alchemy::knowsIngredientEffect(i, alchemySkill); ++i)
            {
                if (const std::optional<ESM::IndexedENAMstruct> effect = MWMechanics::getIngredientEffect(record, i))
                    result.add(*effect);
            }
            return result;
        };
        playerBindings["getAlchemyInfo"]
            = [](sol::this_state thisState, const Object& player, const sol::table& table) -> sol::table {
            verifyPlayer(player);
            MWMechanics::Alchemy alchemy = makeAlchemy(toAlchemySetup(table));
            const MWWorld::Ptr playerPtr = player.ptr();
            const MWWorld::ESMStore& store = *MWBase::Environment::get().getESMStore();
            sol::table effects(thisState, sol::create);
            const std::vector<MWMechanics::EffectKey> keys = alchemy.listEffects();
            for (size_t i = 0; i < keys.size(); ++i)
            {
                sol::table effect(thisState, sol::create);
                effect["id"] = keys[i].mId;
                const ESM::MagicEffect* magicEffect = store.get<ESM::MagicEffect>().find(keys[i].mId);
                if (magicEffect->mData.mFlags & ESM::MagicEffect::TargetSkill)
                    effect["affectedSkill"] = keys[i].mArg;
                else if (magicEffect->mData.mFlags & ESM::MagicEffect::TargetAttribute)
                    effect["affectedAttribute"] = keys[i].mArg;
                effect["isKnown"] = MWMechanics::Alchemy::knownEffect(i, playerPtr);
                effects.add(effect);
            }
            sol::table apparatus(thisState, sol::create);
            for (auto it = alchemy.beginTools(); it != alchemy.endTools(); ++it)
            {
                if (!it->isEmpty())
                    apparatus.add(player.isGObject() ? sol::make_object(thisState, GObject(*it))
                                                     : sol::make_object(thisState, LObject(*it)));
            }
            sol::table result(thisState, sol::create);
            result["effects"] = effects;
            result["apparatus"] = apparatus;
            result["maxCount"] = alchemy.countPotionsToBrew();
            result["suggestedName"] = alchemy.suggestPotionName();
            return result;
        };
        playerBindings["brewPotion"]
            = [context](const Object& player, const sol::table& table, sol::optional<sol::table> callback) {
                  verifyCrafter(player);
                  const AlchemySetup setup = toAlchemySetup(table);
                  switch (makeAlchemy(setup).getReadyStatus())
                  {
                      case MWMechanics::Alchemy::Result_NoMortarAndPestle:
                          throw std::runtime_error("Brewing needs a mortar and pestle");
                      case MWMechanics::Alchemy::Result_LessThanTwoIngredients:
                          throw std::runtime_error("Brewing needs at least two ingredients");
                      case MWMechanics::Alchemy::Result_NoName:
                          throw std::runtime_error("The potion needs a name");
                      default:
                          break;
                  }
                  context.mLuaManager->addAction(
                      [context, setup, callback = toCallback(callback)] {
                          // An earlier action may have used up the ingredients or the apparatus.
                          std::optional<MWMechanics::Alchemy> alchemy = setUpAgain([&] { return makeAlchemy(setup); });
                          if (!alchemy
                              || (alchemy->getReadyStatus() != MWMechanics::Alchemy::Result_Success
                                  && alchemy->getReadyStatus() != MWMechanics::Alchemy::Result_NoEffects))
                          {
                              reportOutcome(context, callback, 0);
                              return;
                          }
                          int count = std::min(setup.mCount, alchemy->countPotionsToBrew());
                          const std::string name = setup.mName.value_or(alchemy->suggestPotionName());
                          if (alchemy->create(name, count) == MWMechanics::Alchemy::Result_NoEffects)
                              count = 0;
                          reportOutcome(context, callback, count);
                      },
                      "BrewPotionAction");
              };
    }

    // Items that the player buys or sells, and how many of each. The items are kept as Lua objects, so that a queued
    // action can find them again.
    using BarterItems = std::vector<std::pair<Object, int>>;

    // What a script offers a merchant, as it describes it in a Lua table.
    struct BarterSetup
    {
        BarterItems mBought;
        BarterItems mSold;
        std::optional<int> mGold;
    };

    static BarterItems toBarterItems(const sol::table& table, const MWWorld::Ptr& owner, const MWWorld::Ptr& merchant)
    {
        BarterItems result;
        for (size_t i = 1; i <= table.size(); ++i)
        {
            const sol::table entry = LuaUtil::cast<sol::table>(table[i]);
            const Object item = LuaUtil::cast<Object>(entry.get<sol::object>("item"));
            const MWWorld::Ptr itemPtr = item.ptr();
            verifyIsIn(itemPtr, owner);
            if (!MWMechanics::canBarterItem(merchant, itemPtr))
                throw std::runtime_error("The merchant doesn't trade " + itemPtr.toString());
            for (const auto& [listed, count] : result)
            {
                if (listed.ptr() == itemPtr)
                    throw std::runtime_error(itemPtr.toString() + " is listed twice");
            }
            result.emplace_back(item, getCountToTake(itemPtr, entry.get<sol::optional<int>>("count")));
        }
        return result;
    }

    static BarterSetup toBarterSetup(const sol::table& table, const MWWorld::Ptr& merchant)
    {
        BarterSetup setup;
        if (const std::optional<sol::table> bought = getField<sol::table>(table, "buy"))
            setup.mBought = toBarterItems(*bought, merchant, merchant);
        if (const std::optional<sol::table> sold = getField<sol::table>(table, "sell"))
            setup.mSold = toBarterItems(*sold, MWMechanics::getPlayer(), merchant);
        setup.mGold = getField<int>(table, "gold");
        return setup;
    }

    // The gold that the merchant offers the player, as the trade window adds it up. It is negative when the player
    // pays.
    static int getMerchantOffer(const MWWorld::Ptr& merchant, const BarterSetup& setup)
    {
        int offer = 0;
        for (const auto& [item, count] : setup.mBought)
            offer -= MWMechanics::getBarterPrice(merchant, item.ptr(), count, true);
        for (const auto& [item, count] : setup.mSold)
            offer += MWMechanics::getBarterPrice(merchant, item.ptr(), count, false);
        return offer;
    }

    // Whether the items are still where they were when the script made the offer, after the actions before it ran.
    static bool areBarterItemsLeft(const BarterItems& items, const MWWorld::Ptr& owner)
    {
        for (const auto& [item, count] : items)
        {
            if (getCountLeft(item.ptr(), owner, count) < count)
                return false;
        }
        return true;
    }

    // Moves items to another inventory, the way the trade window moves them. A merchant never runs out of items
    // that restock, so the merchant keeps those.
    static void moveBarterItem(const MWWorld::Ptr& item, int count, const MWWorld::Ptr& to, bool equipReplacement)
    {
        const bool allowAutoEquip = !Settings::game().mPreventMerchantEquipping;
        MWWorld::ContainerStore& store = to.getClass().getContainerStore(to);
        const bool restocks = item.getCellRef().getCount(false) < 0;
        if (!restocks)
            item.getContainerStore()->remove(item, count, equipReplacement);
        // When the whole stack moves, it can keep its RefNum.
        if (!restocks && item.getCellRef().getCount() == 0)
        {
            store.add(item, count, allowAutoEquip);
            return;
        }
        MWWorld::ManualRef copy(*MWBase::Environment::get().getESMStore(), item, count);
        store.add(copy.getPtr(), count, allowAutoEquip);
    }

    static void addBarterBindings(sol::table& playerBindings, const Context& context)
    {
        playerBindings["getBarterOffer"]
            = [](const Object& player, const Object& merchant, const sol::table& table) -> int {
            verifyPlayer(player);
            const MWWorld::Ptr merchantPtr = merchant.ptr();
            verifyActor(merchantPtr.getClass());
            return getMerchantOffer(merchantPtr, toBarterSetup(table, merchantPtr));
        };
        playerBindings["barter"] = [context](const Object& player, const Object& merchant, const sol::table& table,
                                       sol::optional<sol::table> callback) {
            verifyPlayer(player);
            if (!player.isGObject())
                throw std::runtime_error("Only global scripts can barter");
            const MWWorld::Ptr merchantPtr = merchant.ptr();
            verifyActor(merchantPtr.getClass());
            const BarterSetup setup = toBarterSetup(table, merchantPtr);
            if (setup.mBought.empty() && setup.mSold.empty())
                throw std::runtime_error("The offer must buy or sell at least one item");
            const int gold = setup.mGold.value_or(getMerchantOffer(merchantPtr, setup));
            switch (MWMechanics::getBarterProblem(merchantPtr, gold))
            {
                case MWMechanics::BarterProblem::None:
                    break;
                case MWMechanics::BarterProblem::PlayerLacksGold:
                    throw std::runtime_error("The player doesn't have enough gold");
                case MWMechanics::BarterProblem::MerchantLacksGold:
                    throw std::runtime_error("The merchant doesn't have enough gold");
            }
            context.mLuaManager->addAction(
                [context, merchant = Object(merchantPtr), setup, gold, callback = toCallback(callback)] {
                    const MWWorld::Ptr trader = merchant.ptr();
                    const MWWorld::Ptr buyer = MWMechanics::getPlayer();
                    // An earlier action may have moved the items or spent the gold.
                    if (!areBarterItemsLeft(setup.mBought, trader) || !areBarterItemsLeft(setup.mSold, buyer)
                        || MWMechanics::getBarterProblem(trader, gold) != MWMechanics::BarterProblem::None)
                    {
                        reportOutcome(context, callback, false);
                        return;
                    }
                    MWBase::MechanicsManager* mechanics = MWBase::Environment::get().getMechanicsManager();
                    for (const auto& [item, count] : setup.mSold)
                    {
                        const MWWorld::Ptr sold = item.ptr();
                        if (mechanics->isItemStolenFrom(sold.getCellRef().getRefId(), trader))
                        {
                            mechanics->confiscateStolenItemToOwner(buyer, sold, trader, count);
                            reportOutcome(context, callback, false);
                            return;
                        }
                    }
                    if (!MWMechanics::haggle(trader, gold, getMerchantOffer(trader, setup)))
                    {
                        reportOutcome(context, callback, false);
                        return;
                    }
                    for (const auto& [item, count] : setup.mSold)
                        moveBarterItem(item.ptr(), count, trader, true);
                    for (const auto& [item, count] : setup.mBought)
                        moveBarterItem(item.ptr(), count, buyer, false);
                    MWMechanics::payBarterBalance(trader, gold);
                    reportOutcome(context, callback, true);
                },
                "BarterAction");
        };
    }

    static std::vector<ESM::RefId> toLevelUpAttributes(const MWMechanics::NpcStats& stats, const sol::table& table)
    {
        std::vector<ESM::RefId> result;
        const MWWorld::Store<ESM::Attribute>& store = MWBase::Environment::get().getESMStore()->get<ESM::Attribute>();
        for (size_t i = 1; i <= table.size(); ++i)
        {
            const std::string_view text = LuaUtil::cast<std::string_view>(table[i]);
            const ESM::RefId id = ESM::RefId::deserializeText(text);
            if (!store.search(id))
                throw std::runtime_error("Attribute '" + std::string(text) + "' does not exist");
            if (std::find(result.begin(), result.end(), id) != result.end())
                throw std::runtime_error("Attribute '" + std::string(text) + "' is chosen twice");
            if (stats.getLevelUpAttributeIncrease(id) <= 0)
                throw std::runtime_error("Attribute '" + std::string(text) + "' can't be raised any further");
            result.push_back(id);
        }
        if (static_cast<int>(result.size()) != stats.getLevelUpAttributeCount())
            throw std::runtime_error(
                "A level-up raises " + std::to_string(stats.getLevelUpAttributeCount()) + " attributes");
        return result;
    }

    // Whether a level-up can still raise the attributes that toLevelUpAttributes accepted.
    static bool canLevelUpWith(const MWMechanics::NpcStats& stats, const std::vector<ESM::RefId>& chosen)
    {
        if (static_cast<int>(chosen.size()) != stats.getLevelUpAttributeCount())
            return false;
        return std::all_of(chosen.begin(), chosen.end(),
            [&](const ESM::RefId& id) { return stats.getLevelUpAttributeIncrease(id) > 0; });
    }

    static void addLevelUpBindings(sol::table& playerBindings, const Context& context)
    {
        playerBindings["getLevelUpInfo"] = [](sol::this_state thisState, const Object& player) -> sol::table {
            verifyPlayer(player);
            const MWWorld::Ptr ptr = player.ptr();
            const MWMechanics::NpcStats& stats = ptr.getClass().getNpcStats(ptr);
            sol::table increases(thisState, sol::create);
            for (const ESM::Attribute& attribute : MWBase::Environment::get().getESMStore()->get<ESM::Attribute>())
                increases[attribute.mId.serializeText()] = stats.getLevelUpAttributeIncrease(attribute.mId);
            sol::table result(thisState, sol::create);
            result["isReady"] = stats.isLevelUpReady();
            result["level"] = stats.getLevel() + 1;
            result["description"] = stats.getLevelUpDescription();
            result["classImage"] = stats.getLevelUpClassImage();
            result["attributeCount"] = stats.getLevelUpAttributeCount();
            result["attributeIncreases"] = increases;
            return result;
        };
        playerBindings["levelUp"] = [context](const Object& player, const sol::table& attributes) {
            verifyPlayer(player);
            if (!player.isGObject())
                throw std::runtime_error("Only global scripts can level up the player");
            const MWWorld::Ptr ptr = player.ptr();
            std::vector<ESM::RefId> chosen = toLevelUpAttributes(ptr.getClass().getNpcStats(ptr), attributes);
            context.mLuaManager->addAction(
                [chosen = std::move(chosen)] {
                    const MWWorld::Ptr levelled = MWMechanics::getPlayer();
                    MWMechanics::NpcStats& stats = levelled.getClass().getNpcStats(levelled);
                    // An earlier level-up in the frame may have taken one of the attributes to 100.
                    if (!canLevelUpWith(stats, chosen))
                    {
                        Log(Debug::Warning) << "The player didn't level up: an attribute can't be raised any further";
                        return;
                    }
                    stats.levelUp(chosen);
                },
                "LevelUpAction");
        };
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
        player["buyTravel"] = [context](const Object& buyer, const Object& merchant, int64_t destination) {
            verifyBuyer(buyer);
            const MWWorld::Ptr merchantPtr = merchant.ptr();
            verifyActor(merchantPtr.getClass());
            const ESM::Transport::Dest& dest = getTravelDestination(merchantPtr, destination);
            if (!canPay(MWMechanics::getTravelPrice(merchantPtr, dest)))
                throw std::runtime_error("The player doesn't have enough gold");
            // The trip moves the player, so it must wait until the other queued actions are done.
            context.mLuaManager->addTeleportPlayerAction([merchant = Object(merchantPtr), dest] {
                const MWWorld::Ptr ptr = merchant.ptr();
                const int price = MWMechanics::getTravelPrice(ptr, dest);
                // An earlier action may have spent the gold.
                if (!canPay(price))
                {
                    Log(Debug::Warning) << "The player didn't travel: The player doesn't have enough gold";
                    return;
                }
                MWMechanics::travel(ptr, dest, price);
            });
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
        addTakingBindings(player, context);
        addToolBindings(player, context);
        addEnchantingBindings(player, context);
        addAlchemyBindings(player, context);
        addSpellmakingBindings(player, context);
        addBarterBindings(player, context);
        addLevelUpBindings(player, context);

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
