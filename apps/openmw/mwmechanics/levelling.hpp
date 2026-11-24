#ifndef GAME_MWMECHANICS_LEVELLING_H
#define GAME_MWMECHANICS_LEVELLING_H

#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>

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
        return std::floor(0.5f * (startingStrength + startingEndurance)) + healthPerEndurance * endurance * (level - 1);
    }

    /// The base health the player should have now, with the "retroactive health" setting on or off.
    ///
    /// base is the player's base health now. enabled says whether the setting is on. retroactive is what
    /// retroactiveHealth gives, when the base health should change to it, and nothing to leave the base health as it
    /// is. lastSet is the base health this function returned the last time, or nothing after a game loads.
    ///
    /// morrowindHealth is the base health the player would have under Morrowind's own rules. It is set while the
    /// setting is on. When the setting is turned on, it starts as the base health the player has then. After that it
    /// gains every change to base health that this function didn't make, since the base health no longer equals
    /// lastSet. Those are each level up's tenth of Endurance, and changes such as a script's. When the setting is
    /// turned off, the player gets morrowindHealth back, and it is reset.
    inline float baseHealthToSet(std::optional<float>& morrowindHealth, std::optional<float>& lastSet, float base,
        bool enabled, std::optional<float> retroactive)
    {
        if (morrowindHealth && lastSet)
            *morrowindHealth += base - *lastSet;
        if (!enabled)
        {
            const float result = morrowindHealth.value_or(base);
            morrowindHealth.reset();
            lastSet.reset();
            return result;
        }
        if (!morrowindHealth)
            morrowindHealth = base;
        lastSet = retroactive.value_or(base);
        return *lastSet;
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

    /// How many of an attribute's skill increases a level up spends when the player raises that attribute, with the
    /// "keep level up progress" setting. It spends the fewest that give the same multiplier, so the rest count toward
    /// the next level. multiplier(n) is the level up multiplier for n increases, from 1 to 10. Morrowind gives x2 for
    /// 1 to 4, x3 for 5 to 7, x4 for 8 or 9 and x5 for 10, so 6 increases spend 5 for x3 and keep 1, and 14 spend 10
    /// for x5 and keep 4.
    inline int increasesSpent(int increases, const std::function<int(int)>& multiplier)
    {
        const int counted = std::clamp(increases, 0, 10);
        if (counted == 0)
            return 0;
        const int wanted = multiplier(counted);
        for (int spent = 1; spent < counted; ++spent)
            if (multiplier(spent) == wanted)
                return spent;
        return counted;
    }
}

#endif
