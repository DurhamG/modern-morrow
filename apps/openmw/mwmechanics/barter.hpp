#ifndef OPENMW_MECHANICS_BARTER_H
#define OPENMW_MECHANICS_BARTER_H

namespace MWWorld
{
    class Ptr;
}

namespace MWMechanics
{
    /// The value of @a count of @a item in a trade, before the merchant's offer.
    /// Items that wear down are worth less as their condition drops.
    int getEffectiveItemValue(const MWWorld::Ptr& item, int count);

    /// The gold that changes hands when the player trades @a count of @a item with @a merchant, before haggling.
    /// @param playerBuys true when the player buys the item from the merchant, false when the player sells it.
    int getBarterPrice(const MWWorld::Ptr& merchant, const MWWorld::Ptr& item, int count, bool playerBuys);
}

#endif
