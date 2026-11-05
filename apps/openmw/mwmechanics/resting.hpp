#ifndef OPENMW_MECHANICS_RESTING_H
#define OPENMW_MECHANICS_RESTING_H

namespace MWWorld
{
    class Ptr;
}

namespace MWMechanics
{
    /// What decides whether the player can rest or wait where they are, as the rest menu checks it when it opens.
    struct RestConditions
    {
        /// The player is neither underwater nor in the air. In the air, a bed counts as solid ground.
        bool mOnSolidGround = false;

        /// An enemy is nearby.
        bool mEnemiesNearby = false;

        /// The player sleeps, and so heals, when they rest. A bed always allows sleeping. Otherwise the player only
        /// waits in a cell that doesn't allow sleeping, or when they are a werewolf.
        bool mCanSleep = false;

        /// Whether the player can rest or wait at all.
        bool canWait() const { return mOnSolidGround && !mEnemiesNearby; }
    };

    /// The conditions for resting where the player is.
    /// @param bed The bed that the player uses, or an empty Ptr.
    RestConditions getRestConditions(const MWWorld::Ptr& bed);
}

#endif
