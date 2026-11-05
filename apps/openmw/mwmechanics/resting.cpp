#include "resting.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/ptr.hpp"

namespace MWMechanics
{
    RestConditions getRestConditions(const MWWorld::Ptr& bed)
    {
        const int restFlags = MWBase::Environment::get().getWorld()->canRest();

        const bool underwater = (restFlags & MWBase::World::Rest_PlayerIsUnderwater) != 0;
        // Resting in air is allowed if you're using a bed
        const bool inAir = bed.isEmpty() && (restFlags & MWBase::World::Rest_PlayerIsInAir) != 0;

        RestConditions conditions;
        conditions.mOnSolidGround = !underwater && !inAir;
        conditions.mEnemiesNearby = (restFlags & MWBase::World::Rest_EnemiesAreNearby) != 0;
        conditions.mCanSleep = !bed.isEmpty() || (restFlags & MWBase::World::Rest_CanSleep) != 0;
        return conditions;
    }
}
