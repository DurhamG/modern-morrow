#ifndef OPENMW_MWMECHANICS_ACTORUTIL_H
#define OPENMW_MWMECHANICS_ACTORUTIL_H

namespace MWWorld
{
    class Ptr;
}

namespace MWMechanics
{
    MWWorld::Ptr getPlayer();
    bool isPlayerInCombat();
    bool canActorMoveByZAxis(const MWWorld::Ptr& actor);
    bool hasWaterWalking(const MWWorld::Ptr& actor);
    bool isTargetMagicallyHidden(const MWWorld::Ptr& actor);

    /// Removes the corpse of @a actor from the world, as the container window's Dispose of Corpse button does. If the
    /// death animation is still playing, the death is counted, the actor's script runs and the actor's summons
    /// vanish, as they would when the animation ends.
    /// @return false, and does nothing, if the actor is persistent. The corpses of such actors stay.
    bool disposeCorpse(const MWWorld::Ptr& actor);
}

#endif
