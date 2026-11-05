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
        /// The chance, from 0 to 100, that the victim doesn't notice the thief steal @a count of @a item.
        int getChance(const MWWorld::Ptr& item, int count) const;
        /// The chance, from 0 to 100, that the victim doesn't notice the thief when they stop picking the pockets.
        int getFinishChance() const;
        /// Steal some items
        /// @return Was the thief detected?
        bool pick(const MWWorld::Ptr& item, int count) const;
        /// End the pickpocketing process
        /// @return Was the thief detected?
        bool finish() const;
        /// Report the crime after the victim detected the thief.
        void commitCrime() const;
        /// Give the thief Sneak experience after they stole @a count of @a item unnoticed, and record the item as
        /// stolen from the victim.
        void recordTheft(const MWWorld::Ptr& item, int count) const;

    private:
        float getChanceModifier(const MWWorld::Ptr& ptr, float add = 0) const;
        float getValueTerm(const MWWorld::Ptr& item, int count) const;
        /// The highest roll, from 0 to 99, at which the victim doesn't notice the thief.
        int getHighestUnnoticedRoll(float valueTerm) const;
        bool getDetected(float valueTerm) const;
        MWWorld::Ptr mThief;
        MWWorld::Ptr mVictim;
    };

}

#endif
