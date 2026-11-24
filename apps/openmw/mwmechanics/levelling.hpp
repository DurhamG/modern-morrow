#ifndef GAME_MWMECHANICS_LEVELLING_H
#define GAME_MWMECHANICS_LEVELLING_H

#include <algorithm>
#include <cmath>

#include <components/esm/refid.hpp>
#include <components/esm3/loadclas.hpp>
#include <components/esm3/loadrace.hpp>

namespace MWMechanics
{
    /// The base health of a character at this level, as if every level up had counted the Endurance they have now.
    /// That is the "retroactive health" setting. The health at level 1 is half of the starting Strength plus the
    /// starting Endurance, as character creation gives it. Each level after that adds healthPerEndurance times the
    /// current Endurance, where Morrowind adds it times the Endurance at that level up. fLevelUpHealthEndMult sets
    /// healthPerEndurance, to 0.1 in Morrowind.
    inline float retroactiveHealth(
        float startingStrength, float startingEndurance, float endurance, int level, float healthPerEndurance)
    {
        return std::floor(0.5f * (startingStrength + startingEndurance))
            + healthPerEndurance * endurance * (level - 1);
    }

    /// A player's base attribute as character creation set it: their race's value for their sex, plus 10 if their
    /// class favours the attribute. It is never more than the attribute is now, in case the player skipped character
    /// creation or something lowered the attribute since. Without a race, the attribute as it is now stands in for
    /// the race's value.
    inline float startingAttribute(
        const ESM::Race* race, bool male, const ESM::Class* playerClass, ESM::RefId attribute, float now)
    {
        float value = race != nullptr ? static_cast<float>(race->mData.getAttribute(attribute, male)) : now;
        if (playerClass != nullptr)
            for (const ESM::RefId& favoured : playerClass->mData.mAttribute)
                if (favoured == attribute)
                    value += 10;
        return std::min(value, now);
    }
}

#endif
