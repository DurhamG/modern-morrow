#ifndef GAME_MWMECHANICS_GLANCINGBLOWS_H
#define GAME_MWMECHANICS_GLANCINGBLOWS_H

namespace MWMechanics
{
    /// The share of a hit's damage that a glancing blow does, with the "glancing blows" setting.
    inline constexpr float sGlancingBlowDamageShare = 0.1f;

    /// Whether an attack that lost its roll to hit lands anyway, as a glancing blow. With the "glancing blows"
    /// setting, the player's attacks that would miss do a tenth of the damage a hit would. Other actors still miss,
    /// so enemies don't get any stronger.
    inline bool isGlancingBlow(bool rolledToHit, bool attackerIsPlayer, bool enabled)
    {
        return !rolledToHit && attackerIsPlayer && enabled;
    }
}

#endif
