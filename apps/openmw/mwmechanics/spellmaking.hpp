#ifndef OPENMW_MECHANICS_SPELLMAKING_H
#define OPENMW_MECHANICS_SPELLMAKING_H

#include <cstddef>
#include <string>
#include <vector>

#include <components/esm3/effectlist.hpp>
#include <components/esm3/loadspel.hpp>

namespace ESM
{
    struct MagicEffect;
}

namespace MWWorld
{
    class Ptr;
}

namespace MWMechanics
{
    /// The most effects that a spell or an enchantment that the player makes can have.
    static constexpr std::size_t sMaxCustomEffects = 8;

    /// The highest magnitude that the player can give an effect of their own spell or enchantment.
    static constexpr int sMaxCustomMagnitude = 100;

    /// The longest duration, in seconds, that the player can give an effect of their own spell or enchantment.
    static constexpr int sMaxCustomDuration = 1440;

    /// The largest area, in feet, that the player can give an effect of their own spell or enchantment.
    static constexpr int sMaxCustomArea = 50;

    /// What the player makes from magic effects of their own choosing.
    enum class CustomMagic
    {
        Spell,
        Enchantment,
    };

    /// The magic effects that @a actor can put into a spell or an enchantment of their own. These are the effects of
    /// the spells that they know, if the effect allows spellmaking or enchanting. Abilities, powers and other kinds
    /// of spell don't count. The list is in no particular order.
    std::vector<const ESM::MagicEffect*> getCustomMagicEffects(const MWWorld::Ptr& actor, CustomMagic type);

    /// Whether an effect of the player's own spell or enchantment can have @a range, an ESM::RangeType.
    /// The effect must allow the range, and a constant effect works on self only.
    bool isCustomEffectRangeAllowed(const ESM::MagicEffect& effect, int range, bool isConstant);

    /// A rule for effects that the spellmaking and enchanting windows don't let the player break.
    enum class CustomEffectProblem
    {
        None,
        /// There are more than sMaxCustomEffects effects.
        TooManyEffects,
        /// An effect isn't one of getCustomMagicEffects.
        UnknownEffect,
        /// An effect that doesn't change a skill or an attribute is there twice.
        RepeatedEffect,
        /// An effect has a range that isCustomEffectRangeAllowed refuses.
        RangeNotAllowed,
        /// An effect lacks the skill or the attribute that it changes, or its magnitude, duration or area is out of
        /// bounds. An effect on self has no area.
        InvalidParameters,
    };

    /// The first rule that @a effects break, for a spell or an enchantment that @a actor makes.
    /// @param isConstant Whether the effects are for a constant effect enchantment.
    CustomEffectProblem getCustomEffectProblem(
        const MWWorld::Ptr& actor, const ESM::EffectList& effects, CustomMagic type, bool isConstant);

    /// The magicka cost of a spell that the player makes with @a effects.
    int getCustomSpellCost(const ESM::EffectList& effects);

    /// The gold @a merchant asks to make a spell with @a effects for the player.
    int getCustomSpellPrice(const MWWorld::Ptr& merchant, const ESM::EffectList& effects);

    /// The spell that the spellmaking window makes with @a name and @a effects.
    ESM::Spell makeCustomSpell(const std::string& name, const ESM::EffectList& effects);

    /// The first problem that stops the player from buying a spell of their own.
    enum class SpellmakingProblem
    {
        None,
        NoEffects,
        NoName,
        NoCost,
        NotEnoughGold,
    };

    /// What stops the player from buying @a spell from @a merchant, checked in the order that the spellmaking window
    /// checks.
    SpellmakingProblem getSpellmakingProblem(const MWWorld::Ptr& merchant, const ESM::Spell& spell);

    /// Makes the player pay @a merchant, adds @a spell to the game and teaches it to the player.
    /// @return The new spell record.
    const ESM::Spell* buyCustomSpell(const MWWorld::Ptr& merchant, const ESM::Spell& spell);
}

#endif
