#ifndef GAME_MWMECHANICS_RUNSPEED_H
#define GAME_MWMECHANICS_RUNSPEED_H

#include <algorithm>

namespace MWMechanics
{
    /// The player's run speed with the "minimum run speed" setting. The slowest anyone runs in Morrowind is
    /// minWalkSpeed times baseRunMultiplier, at Speed 0 with no Athletics and nothing carried. That is 175 with
    /// Morrowind's fMinWalkSpeed of 100 and fBaseRunMultiplier of 1.75. The setting makes the player run at least half
    /// again as fast as that, 262.5, when they carry nothing. Faster runners are unchanged.
    ///
    /// unloadedRunSpeed is the run speed from Speed and Athletics alone. loadFactor is the share of it that a load
    /// leaves, which is 1 with nothing carried. A load slows the player by the same share as in Morrowind, from the
    /// raised speed, so the minimum doesn't make up for what a load takes away.
    inline float runSpeedWithMinimum(
        float unloadedRunSpeed, float loadFactor, float minWalkSpeed, float baseRunMultiplier)
    {
        return std::max(unloadedRunSpeed, 1.5f * minWalkSpeed * baseRunMultiplier) * loadFactor;
    }
}

#endif
