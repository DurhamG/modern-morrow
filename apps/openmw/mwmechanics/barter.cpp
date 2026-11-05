#include "barter.hpp"

#include <algorithm>

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/ptr.hpp"

namespace MWMechanics
{
    int getEffectiveItemValue(const MWWorld::Ptr& item, int count)
    {
        float price = static_cast<float>(item.getClass().getValue(item));
        if (item.getClass().hasItemHealth(item))
        {
            price *= item.getClass().getItemNormalizedHealth(item);
        }
        return static_cast<int>(price * count);
    }

    int getBarterPrice(const MWWorld::Ptr& merchant, const MWWorld::Ptr& item, int count, bool playerBuys)
    {
        // The offered price must be capped at 75% of the base price to avoid exploits
        // connected to buying and selling the same item.
        // This value has been determined by researching the limitations of the vanilla formula
        // and may not be sufficient if getBarterOffer behavior has been changed.
        const int basePrice = getEffectiveItemValue(item, count);
        const int cap = static_cast<int>(std::max(1.f, 0.75f * basePrice));
        const int offer
            = MWBase::Environment::get().getMechanicsManager()->getBarterOffer(merchant, basePrice, playerBuys);
        if (playerBuys)
            return std::max(cap, offer); // Minimum buying price -- 75% of the base
        return merchant.getClass().isNpc() ? std::min(cap, offer) : offer; // Maximum selling price -- 75% of the base
    }
}
