#ifndef GAME_MWMECHANICS_RUNFATIGUE_H
#define GAME_MWMECHANICS_RUNFATIGUE_H

namespace MWMechanics
{
    /// The fatigue the player loses to running in a frame, with the "run fatigue" setting applied. The setting scales
    /// only what running takes beyond the fatigue that comes back in the same frame, so at 0 running keeps fatigue
    /// level, and at 1 it costs what it does in Morrowind.
    inline float scaleRunFatigueLoss(float loss, float recovered, float scale)
    {
        if (loss > recovered)
            return recovered + (loss - recovered) * scale;
        return loss;
    }
}

#endif
