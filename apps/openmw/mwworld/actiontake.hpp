#ifndef GAME_MWWORLD_ACTIONTAKE_H
#define GAME_MWWORLD_ACTIONTAKE_H

#include "action.hpp"

namespace MWWorld
{
    /// Moves @a object, which lies in the world, into @a actor's inventory. Taking an item that someone owns is theft,
    /// which the engine records and reports as a crime. A stack of gold coins counts each coin at its value.
    /// @return The item in the inventory.
    Ptr takeFromWorld(const Ptr& actor, const Ptr& object);

    class ActionTake : public Action
    {
        void executeImp(const Ptr& actor) override;

    public:
        ActionTake(const MWWorld::Ptr& object);
    };
}

#endif
