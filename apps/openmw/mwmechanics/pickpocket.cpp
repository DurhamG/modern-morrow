#include "pickpocket.hpp"

#include <algorithm>

#include <components/misc/rng.hpp>

#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/inventorystore.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/world.hpp"

#include "npcstats.hpp"

namespace MWMechanics
{

    Pickpocket::Pickpocket(const MWWorld::Ptr& thief, const MWWorld::Ptr& victim)
        : mThief(thief)
        , mVictim(victim)
    {
    }

    float Pickpocket::getChanceModifier(const MWWorld::Ptr& ptr, float add) const
    {
        NpcStats& stats = ptr.getClass().getNpcStats(ptr);
        float agility = stats.getAttribute(ESM::Attribute::Agility).getModified();
        float luck = stats.getAttribute(ESM::Attribute::Luck).getModified();
        float sneak = static_cast<float>(ptr.getClass().getSkill(ptr, ESM::Skill::Sneak));
        return (add + 0.2f * agility + 0.1f * luck + sneak) * stats.getFatigueTerm();
    }

    float Pickpocket::getValueTerm(const MWWorld::Ptr& item, int count) const
    {
        float stackValue = static_cast<float>(item.getClass().getValue(item) * count);
        float fPickPocketMod = MWBase::Environment::get()
                                   .getESMStore()
                                   ->get<ESM::GameSetting>()
                                   .find("fPickPocketMod")
                                   ->mValue.getFloat();
        return 10 * fPickPocketMod * stackValue;
    }

    int Pickpocket::getHighestUnnoticedRoll(float valueTerm) const
    {
        float x = getChanceModifier(mThief);
        float y = getChanceModifier(mVictim, valueTerm);

        float t = 2 * x - y;

        float pcSneak = static_cast<float>(mThief.getClass().getSkill(mThief, ESM::Skill::Sneak));
        int iPickMinChance = MWBase::Environment::get()
                                 .getESMStore()
                                 ->get<ESM::GameSetting>()
                                 .find("iPickMinChance")
                                 ->mValue.getInteger();
        int iPickMaxChance = MWBase::Environment::get()
                                 .getESMStore()
                                 ->get<ESM::GameSetting>()
                                 .find("iPickMaxChance")
                                 ->mValue.getInteger();

        if (t < pcSneak / iPickMinChance)
            return int(pcSneak / iPickMinChance);
        return int(std::min(float(iPickMaxChance), t));
    }

    bool Pickpocket::getDetected(float valueTerm) const
    {
        const int highestUnnoticedRoll = getHighestUnnoticedRoll(valueTerm);
        auto& prng = MWBase::Environment::get().getWorld()->getPrng();
        return Misc::Rng::roll0to99(prng) > highestUnnoticedRoll;
    }

    int Pickpocket::getChance(const MWWorld::Ptr& item, int count) const
    {
        return std::clamp(getHighestUnnoticedRoll(getValueTerm(item, count)) + 1, 0, 100);
    }

    int Pickpocket::getFinishChance() const
    {
        return std::clamp(getHighestUnnoticedRoll(0.f) + 1, 0, 100);
    }

    bool Pickpocket::canSteal(const MWWorld::Ptr& item) const
    {
        if (MWBase::Environment::get().getMechanicsManager()->isBoundItem(item))
            return false;
        return !mVictim.getClass().getInventoryStore(mVictim).isEquipped(item);
    }

    bool Pickpocket::noticesItem() const
    {
        const float sneak = mThief.getClass().getSkill(mThief, ESM::Skill::Sneak);
        return Misc::Rng::roll0to99(MWBase::Environment::get().getWorld()->getPrng()) <= sneak;
    }

    bool Pickpocket::pick(const MWWorld::Ptr& item, int count) const
    {
        return getDetected(getValueTerm(item, count));
    }

    bool Pickpocket::finish() const
    {
        return getDetected(0.f);
    }

    void Pickpocket::commitCrime() const
    {
        MWBase::Environment::get().getMechanicsManager()->commitCrime(
            mThief, mVictim, MWBase::MechanicsManager::OT_Pickpocket, ESM::RefId(), 0, true);
    }

    void Pickpocket::recordTheft(const MWWorld::Ptr& item, int count) const
    {
        mThief.getClass().skillUsageSucceeded(mThief, ESM::Skill::Sneak, ESM::Skill::Sneak_PickPocket);
        MWBase::Environment::get().getMechanicsManager()->itemTaken(mThief, item, mVictim, count, false);
    }
}
