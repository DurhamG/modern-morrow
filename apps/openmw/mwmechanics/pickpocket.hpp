#ifndef OPENMW_MECHANICS_PICKPOCKET_H
#define OPENMW_MECHANICS_PICKPOCKET_H

#include "../mwworld/ptr.hpp"

namespace MWMechanics
{

    class Pickpocket
    {
    public:
        Pickpocket(const MWWorld::Ptr& thief, const MWWorld::Ptr& victim);

        /// Whether @a item, which the victim carries, can be stolen. Equipped items and bound items can't.
        bool canSteal(const MWWorld::Ptr& item) const;
        /// Roll whether the thief notices an item in the victim's pockets. The items the thief doesn't notice stay
        /// hidden from them while they pick the pockets.
        bool noticesItem() const;
        /// Steal some items
        /// @return Was the thief detected?
        bool pick(const MWWorld::Ptr& item, int count);
        /// End the pickpocketing process
        /// @return Was the thief detected?
        bool finish();
        /// Report the crime after the victim detected the thief.
        void commitCrime() const;
        /// Give the thief Sneak experience after they stole @a count of @a item unnoticed, and record the item as
        /// stolen from the victim.
        void recordTheft(const MWWorld::Ptr& item, int count) const;

    private:
        bool getDetected(float valueTerm);
        float getChanceModifier(const MWWorld::Ptr& ptr, float add = 0);
        MWWorld::Ptr mThief;
        MWWorld::Ptr mVictim;
    };

}

#endif
