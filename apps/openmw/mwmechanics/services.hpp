#ifndef OPENMW_MECHANICS_SERVICES_H
#define OPENMW_MECHANICS_SERVICES_H

#include <components/esm/refid.hpp>

namespace ESM
{
    struct Spell;
}

namespace MWWorld
{
    class Ptr;
}

namespace MWMechanics
{
    /// The gold @a trainer asks to raise the player's @a skill by one point.
    int getTrainingPrice(const MWWorld::Ptr& trainer, ESM::RefId skill);

    /// The gold @a merchant asks to teach the player @a spell.
    int getSpellPrice(const MWWorld::Ptr& merchant, const ESM::Spell& spell);

    /// The gold @a merchant asks to restore @a item to full condition.
    int getRepairPrice(const MWWorld::Ptr& merchant, const MWWorld::Ptr& item);
}

#endif
