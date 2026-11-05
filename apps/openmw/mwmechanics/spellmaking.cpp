#include "spellmaking.hpp"

#include <algorithm>

#include <components/esm/attr.hpp>
#include <components/esm3/loadgmst.hpp>
#include <components/esm3/loadmgef.hpp>
#include <components/esm3/loadskil.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/containerstore.hpp"
#include "../mwworld/esmstore.hpp"

#include "actorutil.hpp"
#include "creaturestats.hpp"
#include "spells.hpp"
#include "spellutil.hpp"

namespace
{
    float getExactCustomSpellCost(const ESM::EffectList& effects)
    {
        float y = 0;

        for (const ESM::IndexedENAMstruct& effect : effects.mList)
        {
            y += std::max(
                1.f, MWMechanics::calcEffectCost(effect.mData, nullptr, MWMechanics::EffectCostMethod::PlayerSpell));

            if (effect.mData.mRange == ESM::RT_Target)
                y *= 1.5;
        }

        return y;
    }
}

namespace MWMechanics
{
    std::vector<const ESM::MagicEffect*> getCustomMagicEffects(const MWWorld::Ptr& actor, CustomMagic type)
    {
        const MWWorld::Store<ESM::MagicEffect>& store
            = MWBase::Environment::get().getESMStore()->get<ESM::MagicEffect>();
        std::vector<const ESM::MagicEffect*> knownEffects;

        for (const ESM::Spell* spell : actor.getClass().getCreatureStats(actor).getSpells())
        {
            // only normal spells count
            if (spell->mData.mType != ESM::Spell::ST_Spell)
                continue;

            for (const ESM::IndexedENAMstruct& effectInfo : spell->mEffects.mList)
            {
                const ESM::MagicEffect* effect = store.find(effectInfo.mData.mEffectID);

                // skip effects that do not allow spellmaking/enchanting
                int requiredFlags = (type == CustomMagic::Spell) ? ESM::MagicEffect::AllowSpellmaking
                                                                 : ESM::MagicEffect::AllowEnchanting;
                if (!(effect->mData.mFlags & requiredFlags))
                    continue;

                if (std::find(knownEffects.begin(), knownEffects.end(), effect) == knownEffects.end())
                    knownEffects.push_back(effect);
            }
        }

        return knownEffects;
    }

    bool isCustomEffectRangeAllowed(const ESM::MagicEffect& effect, int range, bool isConstant)
    {
        switch (range)
        {
            case ESM::RT_Self:
                return (effect.mData.mFlags & ESM::MagicEffect::CastSelf) != 0 || isConstant;
            case ESM::RT_Touch:
                return (effect.mData.mFlags & ESM::MagicEffect::CastTouch) != 0 && !isConstant;
            case ESM::RT_Target:
                return (effect.mData.mFlags & ESM::MagicEffect::CastTarget) != 0 && !isConstant;
            default:
                return false;
        }
    }

    CustomEffectProblem getCustomEffectProblem(
        const MWWorld::Ptr& actor, const ESM::EffectList& effects, CustomMagic type, bool isConstant)
    {
        if (effects.mList.size() > sMaxCustomEffects)
            return CustomEffectProblem::TooManyEffects;

        const MWWorld::ESMStore& store = *MWBase::Environment::get().getESMStore();
        const std::vector<const ESM::MagicEffect*> knownEffects = getCustomMagicEffects(actor, type);
        for (auto it = effects.mList.begin(); it != effects.mList.end(); ++it)
        {
            const ESM::ENAMstruct& params = it->mData;
            const ESM::MagicEffect* effect = store.get<ESM::MagicEffect>().search(params.mEffectID);
            if (effect == nullptr || std::find(knownEffects.begin(), knownEffects.end(), effect) == knownEffects.end())
                return CustomEffectProblem::UnknownEffect;

            const int flags = effect->mData.mFlags;
            if (flags & ESM::MagicEffect::TargetSkill)
            {
                if (!store.get<ESM::Skill>().search(params.mSkill))
                    return CustomEffectProblem::InvalidParameters;
            }
            else if (flags & ESM::MagicEffect::TargetAttribute)
            {
                if (!store.get<ESM::Attribute>().search(params.mAttribute))
                    return CustomEffectProblem::InvalidParameters;
            }
            else
            {
                const auto isSameEffect
                    = [&](const ESM::IndexedENAMstruct& other) { return other.mData.mEffectID == params.mEffectID; };
                if (std::any_of(effects.mList.begin(), it, isSameEffect))
                    return CustomEffectProblem::RepeatedEffect;
            }

            if (!isCustomEffectRangeAllowed(*effect, params.mRange, isConstant))
                return CustomEffectProblem::RangeNotAllowed;

            if (!(flags & ESM::MagicEffect::NoMagnitude)
                && (params.mMagnMin < sMinCustomMagnitude || params.mMagnMax < params.mMagnMin
                    || params.mMagnMax > sMaxCustomMagnitude))
                return CustomEffectProblem::InvalidParameters;
            if (!(flags & ESM::MagicEffect::NoDuration) && !isConstant
                && (params.mDuration < sMinCustomDuration || params.mDuration > sMaxCustomDuration))
                return CustomEffectProblem::InvalidParameters;
            if (params.mArea < sMinCustomArea || params.mArea > sMaxCustomArea)
                return CustomEffectProblem::InvalidParameters;
            // The effect editor hides the area of an effect on self, and sets it to the smallest area.
            if (params.mRange == ESM::RT_Self && params.mArea != sMinCustomArea)
                return CustomEffectProblem::InvalidParameters;
        }

        return CustomEffectProblem::None;
    }

    int getCustomSpellCost(const ESM::EffectList& effects)
    {
        return static_cast<int>(getExactCustomSpellCost(effects));
    }

    int getCustomSpellPrice(const MWWorld::Ptr& merchant, const ESM::EffectList& effects)
    {
        const MWWorld::ESMStore& store = *MWBase::Environment::get().getESMStore();
        float fSpellMakingValueMult = store.get<ESM::GameSetting>().find("fSpellMakingValueMult")->mValue.getFloat();

        int price = std::max(1, static_cast<int>(getExactCustomSpellCost(effects) * fSpellMakingValueMult));
        return MWBase::Environment::get().getMechanicsManager()->getBarterOffer(merchant, price, true);
    }

    ESM::Spell makeCustomSpell(const std::string& name, const ESM::EffectList& effects)
    {
        ESM::Spell spell;
        spell.mName = name;
        spell.mEffects = effects;
        spell.mData.mCost = getCustomSpellCost(effects);
        spell.mData.mType = ESM::Spell::ST_Spell;
        spell.mData.mFlags = 0;
        return spell;
    }

    SpellmakingProblem getSpellmakingProblem(const MWWorld::Ptr& merchant, const ESM::Spell& spell)
    {
        if (spell.mEffects.mList.empty())
            return SpellmakingProblem::NoEffects;

        if (spell.mName.empty())
            return SpellmakingProblem::NoName;

        if (spell.mData.mCost == 0)
            return SpellmakingProblem::NoCost;

        const MWWorld::Ptr player = getPlayer();
        int playerGold = player.getClass().getContainerStore(player).count(MWWorld::ContainerStore::sGoldId);
        if (getCustomSpellPrice(merchant, spell.mEffects) > playerGold)
            return SpellmakingProblem::NotEnoughGold;

        return SpellmakingProblem::None;
    }

    const ESM::Spell* buyCustomSpell(const MWWorld::Ptr& merchant, const ESM::Spell& spell)
    {
        MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Mysticism Hit"));

        const int price = getCustomSpellPrice(merchant, spell.mEffects);
        const MWWorld::Ptr player = getPlayer();

        player.getClass().getContainerStore(player).remove(MWWorld::ContainerStore::sGoldId, price);

        // add gold to NPC trading gold pool
        CreatureStats& npcStats = merchant.getClass().getCreatureStats(merchant);
        npcStats.setGoldPool(npcStats.getGoldPool() + price);

        const ESM::Spell* record = MWBase::Environment::get().getESMStore()->insert(spell);

        player.getClass().getCreatureStats(player).getSpells().add(record->mId);
        return record;
    }
}
