#include "barter.hpp"

#include <algorithm>
#include <cmath>

#include <components/esm3/loadgmst.hpp>
#include <components/esm3/loadnpc.hpp>
#include <components/misc/rng.hpp>

#include "../mwbase/dialoguemanager.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/containerstore.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/inventorystore.hpp"
#include "../mwworld/ptr.hpp"

#include "actorutil.hpp"
#include "creaturestats.hpp"

namespace
{
    bool rollHaggle(const MWWorld::Ptr& player, const MWWorld::Ptr& merchant, int playerOffer, int merchantOffer)
    {
        // accept if merchant offer is better than player offer
        if (playerOffer <= merchantOffer)
        {
            return true;
        }

        // reject if npc is a creature
        if (merchant.getType() != ESM::NPC::sRecordId)
        {
            return false;
        }

        const MWWorld::Store<ESM::GameSetting>& gmst
            = MWBase::Environment::get().getESMStore()->get<ESM::GameSetting>();

        // Is the player buying?
        bool buying = (merchantOffer < 0);
        int a = std::abs(merchantOffer);
        int b = std::abs(playerOffer);
        int d = (buying) ? int(100 * (a - b) / a) : int(100 * (b - a) / b);

        int clampedDisposition = MWBase::Environment::get().getMechanicsManager()->getDerivedDisposition(merchant);

        const MWMechanics::CreatureStats& merchantStats = merchant.getClass().getCreatureStats(merchant);
        const MWMechanics::CreatureStats& playerStats = player.getClass().getCreatureStats(player);

        float a1 = static_cast<float>(player.getClass().getSkill(player, ESM::Skill::Mercantile));
        float b1 = 0.1f * playerStats.getAttribute(ESM::Attribute::Luck).getModified();
        float c1 = 0.2f * playerStats.getAttribute(ESM::Attribute::Personality).getModified();
        float d1 = static_cast<float>(merchant.getClass().getSkill(merchant, ESM::Skill::Mercantile));
        float e1 = 0.1f * merchantStats.getAttribute(ESM::Attribute::Luck).getModified();
        float f1 = 0.2f * merchantStats.getAttribute(ESM::Attribute::Personality).getModified();

        float dispositionTerm = gmst.find("fDispositionMod")->mValue.getFloat() * (clampedDisposition - 50);
        float pcTerm = (dispositionTerm + a1 + b1 + c1) * playerStats.getFatigueTerm();
        float npcTerm = (d1 + e1 + f1) * merchantStats.getFatigueTerm();
        float x = gmst.find("fBargainOfferMulti")->mValue.getFloat() * d
            + gmst.find("fBargainOfferBase")->mValue.getFloat() + int(pcTerm - npcTerm);

        auto& prng = MWBase::Environment::get().getWorld()->getPrng();
        int roll = Misc::Rng::rollDice(100, prng) + 1;

        // reject if roll fails
        // (or if player tries to buy things and get money)
        if (roll > x || (merchantOffer < 0 && 0 < playerOffer))
        {
            return false;
        }

        // apply skill gain on successful barter
        float skillGain = 0.f;
        int finalPrice = std::abs(playerOffer);
        int initialMerchantOffer = std::abs(merchantOffer);

        if (!buying && (finalPrice > initialMerchantOffer))
        {
            skillGain = std::floor(100.f * (finalPrice - initialMerchantOffer) / finalPrice);
        }
        else if (buying && (finalPrice < initialMerchantOffer))
        {
            skillGain = std::floor(100.f * (initialMerchantOffer - finalPrice) / initialMerchantOffer);
        }
        player.getClass().skillUsageSucceeded(
            player, ESM::Skill::Mercantile, ESM::Skill::Mercantile_Success, skillGain);

        return true;
    }
}

namespace MWMechanics
{
    int getEffectiveItemValue(const MWWorld::Ptr& item, int count)
    {
        float price = static_cast<float>(item.getClass().getValue(item));
        if (item.getClass().hasItemHealth(item))
        {
            price *= item.getClass().getItemNormalizedHealth(item);
        }
        return static_cast<int>(price * count);
    }

    int getBarterPrice(const MWWorld::Ptr& merchant, const MWWorld::Ptr& item, int count, bool playerBuys)
    {
        // The offered price must be capped at 75% of the base price to avoid exploits
        // connected to buying and selling the same item.
        // This value has been determined by researching the limitations of the vanilla formula
        // and may not be sufficient if getBarterOffer behavior has been changed.
        const int basePrice = getEffectiveItemValue(item, count);
        const int cap = static_cast<int>(std::max(1.f, 0.75f * basePrice));
        const int offer
            = MWBase::Environment::get().getMechanicsManager()->getBarterOffer(merchant, basePrice, playerBuys);
        if (playerBuys)
            return std::max(cap, offer); // Minimum buying price -- 75% of the base
        return merchant.getClass().isNpc() ? std::min(cap, offer) : offer; // Maximum selling price -- 75% of the base
    }

    bool canBarterItem(const MWWorld::Ptr& merchant, const MWWorld::Ptr& item)
    {
        if (item.getCellRef().getRefId() == MWWorld::ContainerStore::sGoldId)
            return false;

        if (!item.getClass().showsInInventory(item))
            return false;

        if (!item.getClass().canSell(item, merchant.getClass().getServices(merchant)))
            return false;

        // Bound items may not be bought
        if (MWBase::Environment::get().getMechanicsManager()->isBoundItem(item))
            return false;

        // Merchants don't sell what they wear
        if (merchant.getClass().hasInventoryStore(merchant))
        {
            MWWorld::InventoryStore& store = merchant.getClass().getInventoryStore(merchant);
            if (store.isEquipped(item))
                return false;
        }
        return true;
    }

    BarterProblem getBarterProblem(const MWWorld::Ptr& merchant, int balance)
    {
        const MWWorld::Ptr player = getPlayer();
        const int playerGold = player.getClass().getContainerStore(player).count(MWWorld::ContainerStore::sGoldId);
        if (balance < 0 && playerGold < std::abs(balance))
            return BarterProblem::PlayerLacksGold;

        if (balance > 0 && merchant.getClass().getCreatureStats(merchant).getGoldPool() < balance)
            return BarterProblem::MerchantLacksGold;

        return BarterProblem::None;
    }

    bool haggle(const MWWorld::Ptr& merchant, int playerOffer, int merchantOffer)
    {
        const bool accepted = rollHaggle(getPlayer(), merchant, playerOffer, merchantOffer);

        // apply disposition change if merchant is NPC
        if (merchant.getClass().isNpc())
        {
            const MWWorld::Store<ESM::GameSetting>& gmst
                = MWBase::Environment::get().getESMStore()->get<ESM::GameSetting>();
            int dispositionDelta = accepted ? gmst.find("iBarterSuccessDisposition")->mValue.getInteger()
                                            : gmst.find("iBarterFailDisposition")->mValue.getInteger();

            MWBase::Environment::get().getDialogueManager()->applyBarterDispositionChange(merchant, dispositionDelta);
        }
        return accepted;
    }

    void payBarterBalance(const MWWorld::Ptr& merchant, int balance)
    {
        if (balance == 0)
            return;

        const MWWorld::Ptr player = getPlayer();
        MWWorld::ContainerStore& store = player.getClass().getContainerStore(player);
        if (balance > 0)
            store.add(MWWorld::ContainerStore::sGoldId, balance);
        else
            store.remove(MWWorld::ContainerStore::sGoldId, -balance);

        CreatureStats& merchantStats = merchant.getClass().getCreatureStats(merchant);
        merchantStats.setGoldPool(merchantStats.getGoldPool() - balance);
    }
}
