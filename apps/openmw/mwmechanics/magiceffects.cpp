#include "magiceffects.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include <components/esm/attr.hpp>
#include <components/esm3/effectlist.hpp>
#include <components/esm3/loadmgef.hpp>
#include <components/esm3/loadskil.hpp>
#include <components/esm3/magiceffects.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwworld/esmstore.hpp"

namespace
{
    // Round value to prevent precision issues
    void truncate(float& value)
    {
        value = static_cast<int>(value * 1024.f) / 1024.f;
    }
}

namespace MWMechanics
{
    EffectKey::EffectKey()
        : mId(ESM::MagicEffect::WaterWalking)
    {
    }

    EffectKey::EffectKey(const ESM::ENAMstruct& effect)
    {
        mId = effect.mEffectID;
        mArg = effect.mSkill;

        if (!effect.mAttribute.empty())
        {
            if (!mArg.empty())
                throw std::runtime_error("magic effect can't have both a skill and an attribute argument");

            mArg = effect.mAttribute;
        }
    }

    std::string EffectKey::toString() const
    {
        const auto& store = MWBase::Environment::get().getESMStore();
        const ESM::MagicEffect* magicEffect = store->get<ESM::MagicEffect>().find(mId);
        return getMagicEffectString(
            *magicEffect, store->get<ESM::Attribute>().search(mArg), store->get<ESM::Skill>().search(mArg));
    }

    bool operator<(const EffectKey& left, const EffectKey& right)
    {
        return std::tie(left.mId, left.mArg) < std::tie(right.mId, right.mArg);
    }

    bool operator==(const EffectKey& left, const EffectKey& right)
    {
        return left.mId == right.mId && left.mArg == right.mArg;
    }

    float EffectParam::getMagnitude() const
    {
        return mBase + mModifier;
    }

    void EffectParam::modifyBase(int diff)
    {
        mBase += diff;
    }

    int EffectParam::getBase() const
    {
        return mBase;
    }

    void EffectParam::setBase(int base)
    {
        mBase = base;
    }

    void EffectParam::setModifier(float mod)
    {
        mModifier = mod;
    }

    float EffectParam::getModifier() const
    {
        return mModifier;
    }

    EffectParam::EffectParam()
        : mModifier(0)
        , mBase(0)
    {
    }

    EffectParam& EffectParam::operator+=(const EffectParam& param)
    {
        mModifier += param.mModifier;
        mBase += param.mBase;
        truncate(mModifier);
        return *this;
    }

    EffectParam& EffectParam::operator-=(const EffectParam& param)
    {
        mModifier -= param.mModifier;
        mBase -= param.mBase;
        truncate(mModifier);
        return *this;
    }

    void MagicEffects::add(const EffectKey& key, const EffectParam& param)
    {
        Collection::iterator iter = mCollection.find(key);

        if (iter == mCollection.end())
        {
            mCollection.insert(std::make_pair(key, param));
        }
        else
        {
            iter->second += param;
        }
    }

    void MagicEffects::modifyBase(const EffectKey& key, int diff)
    {
        mCollection[key].modifyBase(diff);
    }

    EffectParam MagicEffects::getOrDefault(const EffectKey& key) const
    {
        return get(key).value_or(EffectParam());
    }

    EffectParam MagicEffects::getOrDefault(ESM::RefId effectId) const
    {
        return getOrDefault(EffectKey(effectId));
    }

    std::optional<EffectParam> MagicEffects::get(const EffectKey& key) const
    {
        Collection::const_iterator iter = mCollection.find(key);

        if (iter != mCollection.end())
        {
            return iter->second;
        }
        return std::nullopt;
    }

    void MagicEffects::writeState(ESM::MagicEffects& state) const
    {
        for (const auto& [key, params] : mCollection)
        {
            if (params.getBase() != 0 || params.getModifier() != 0.f)
            {
                // Don't worry about mArg, never used by magic effect script instructions
                state.mEffects[key.mId] = { params.getBase(), params.getModifier() };
            }
        }
    }

    void MagicEffects::readState(const ESM::MagicEffects& state)
    {
        for (const auto& [key, params] : state.mEffects)
        {
            mCollection[EffectKey(key)].setBase(params.first);
            mCollection[EffectKey(key)].setModifier(params.second);
        }
    }

    std::string getMagicEffectString(
        const ESM::MagicEffect& effect, const ESM::Attribute* attribute, const ESM::Skill* skill)
    {
        const bool targetsSkill = effect.mData.mFlags & ESM::MagicEffect::TargetSkill && skill;
        const bool targetsAttribute = effect.mData.mFlags & ESM::MagicEffect::TargetAttribute && attribute;

        std::string spellLine;

        auto windowManager = MWBase::Environment::get().getWindowManager();

        if (targetsSkill || targetsAttribute)
        {
            if (effect.mId == ESM::MagicEffect::AbsorbAttribute || effect.mId == ESM::MagicEffect::AbsorbSkill)
                spellLine = windowManager->getGameSettingString("sAbsorb", {});
            else if (effect.mId == ESM::MagicEffect::DamageAttribute || effect.mId == ESM::MagicEffect::DamageSkill)
                spellLine = windowManager->getGameSettingString("sDamage", {});
            else if (effect.mId == ESM::MagicEffect::DrainAttribute || effect.mId == ESM::MagicEffect::DrainSkill)
                spellLine = windowManager->getGameSettingString("sDrain", {});
            else if (effect.mId == ESM::MagicEffect::FortifyAttribute || effect.mId == ESM::MagicEffect::FortifySkill)
                spellLine = windowManager->getGameSettingString("sFortify", {});
            else if (effect.mId == ESM::MagicEffect::RestoreAttribute || effect.mId == ESM::MagicEffect::RestoreSkill)
                spellLine = windowManager->getGameSettingString("sRestore", {});
        }

        if (spellLine.empty())
            spellLine = effect.mName;

        if (targetsSkill)
        {
            spellLine += ' ';
            spellLine += skill->mName;
        }
        else if (targetsAttribute)
        {
            spellLine += ' ';
            spellLine += attribute->mName;
        }
        return spellLine;
    }

    std::string describeSpellEffect(const ESM::ENAMstruct& effect, bool isConstant, bool noTarget, bool noMagnitude)
    {
        const MWWorld::ESMStore& store = *MWBase::Environment::get().getESMStore();

        const ESM::MagicEffect* magicEffect = store.get<ESM::MagicEffect>().find(effect.mEffectID);
        const ESM::Attribute* attribute = store.get<ESM::Attribute>().search(effect.mAttribute);
        const ESM::Skill* skill = store.get<ESM::Skill>().search(effect.mSkill);

        auto windowManager = MWBase::Environment::get().getWindowManager();

        std::string_view pt = windowManager->getGameSettingString("spoint", {});
        std::string_view pts = windowManager->getGameSettingString("spoints", {});
        std::string_view pct = windowManager->getGameSettingString("spercent", {});
        std::string_view ft = windowManager->getGameSettingString("sfeet", {});
        std::string_view lvl = windowManager->getGameSettingString("sLevel", {});
        std::string_view lvls = windowManager->getGameSettingString("sLevels", {});
        std::string to = " " + std::string{ windowManager->getGameSettingString("sTo", {}) } + " ";
        std::string sec = " " + std::string{ windowManager->getGameSettingString("ssecond", {}) };
        std::string secs = " " + std::string{ windowManager->getGameSettingString("sseconds", {}) };

        std::string spellLine = getMagicEffectString(*magicEffect, attribute, skill);

        if ((effect.mMagnMin || effect.mMagnMax) && !noMagnitude)
        {
            ESM::MagicEffect::MagnitudeDisplayType displayType = magicEffect->getMagnitudeDisplayType();
            if (displayType == ESM::MagicEffect::MDT_TimesInt)
            {
                std::string_view timesInt = windowManager->getGameSettingString("sXTimesINT", {});
                std::stringstream formatter;

                formatter << std::fixed << std::setprecision(1) << " " << (effect.mMagnMin / 10.0f);
                if (effect.mMagnMin != effect.mMagnMax)
                    formatter << to << (effect.mMagnMax / 10.0f);
                formatter << timesInt;

                spellLine += formatter.str();
            }
            else if (displayType != ESM::MagicEffect::MDT_None)
            {
                spellLine += " " + std::to_string(effect.mMagnMin);
                if (effect.mMagnMin != effect.mMagnMax)
                    spellLine += to + std::to_string(effect.mMagnMax);

                if (displayType == ESM::MagicEffect::MDT_Percentage)
                    spellLine += pct;
                else if (displayType == ESM::MagicEffect::MDT_Feet)
                {
                    spellLine += ' ';
                    spellLine += ft;
                }
                else if (displayType == ESM::MagicEffect::MDT_Level)
                {
                    spellLine += ' ';
                    if (effect.mMagnMin == effect.mMagnMax && std::abs(effect.mMagnMin) == 1)
                        spellLine += lvl;
                    else
                        spellLine += lvls;
                }
                else // ESM::MagicEffect::MDT_Points
                {
                    spellLine += ' ';
                    if (effect.mMagnMin == effect.mMagnMax && std::abs(effect.mMagnMin) == 1)
                        spellLine += pt;
                    else
                        spellLine += pts;
                }
            }
        }

        // constant effects have no duration and no target
        if (!isConstant)
        {
            int duration = effect.mDuration;
            if (!(magicEffect->mData.mFlags & ESM::MagicEffect::AppliedOnce))
                duration = std::max(1, duration);

            if (duration > 0 && !(magicEffect->mData.mFlags & ESM::MagicEffect::NoDuration))
            {
                spellLine += ' ';
                spellLine += windowManager->getGameSettingString("sfor", {});
                spellLine += ' ' + std::to_string(duration) + ((duration == 1) ? sec : secs);
            }

            if (effect.mArea > 0)
            {
                spellLine += ' ';
                spellLine += windowManager->getGameSettingString("sin", {});
                spellLine += ' ' + std::to_string(effect.mArea) + ' ';
                spellLine += windowManager->getGameSettingString("sfootarea", {});
            }

            // potions have no target
            if (!noTarget)
            {
                spellLine += ' ';
                spellLine += windowManager->getGameSettingString("sonword", {});
                spellLine += ' ';
                if (effect.mRange == ESM::RT_Self)
                    spellLine += windowManager->getGameSettingString("sRangeSelf", {});
                else if (effect.mRange == ESM::RT_Touch)
                    spellLine += windowManager->getGameSettingString("sRangeTouch", {});
                else if (effect.mRange == ESM::RT_Target)
                    spellLine += windowManager->getGameSettingString("sRangeTarget", {});
            }
        }

        return spellLine;
    }
}
