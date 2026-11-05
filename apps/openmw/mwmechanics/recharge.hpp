#ifndef MWMECHANICS_RECHARGE_H
#define MWMECHANICS_RECHARGE_H

#include "../mwworld/ptr.hpp"

namespace MWMechanics
{

    bool rechargeItem(const MWWorld::Ptr& item, const float maxCharge, const float duration);

    /// The chance, from 0 to 100, that a soul gem restores some of an item's charge.
    /// It depends on the player, not on the soul gem.
    int getRechargeChance();

    /// Use @a gem, which the player carries, to restore some of the charge of @a item.
    /// @return Whether the item regained some charge. This is false when the gem is used up already.
    bool rechargeItem(const MWWorld::Ptr& item, const MWWorld::Ptr& gem);

    /// Whether a soul gem can restore charge to @a item: an enchanted item that has lost some of its charge.
    bool canRecharge(const MWWorld::ConstPtr& item);
}

#endif
