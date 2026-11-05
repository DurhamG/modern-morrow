#ifndef OPENMW_MWMECHANICS_REPAIR_H
#define OPENMW_MWMECHANICS_REPAIR_H

#include "../mwworld/ptr.hpp"

namespace MWMechanics
{
    /// The chance, from 0 to 100, that one use of a repair tool restores some of an item's condition.
    /// It depends on the player, not on the tool.
    int getRepairChance();

    class Repair
    {
    public:
        void setTool(const MWWorld::Ptr& tool) { mTool = tool; }
        MWWorld::Ptr getTool() { return mTool; }

        /// Use the tool once on @a itemToRepair, which the player carries.
        /// @return Whether the item regained some condition.
        bool repair(const MWWorld::Ptr& itemToRepair);

    private:
        MWWorld::Ptr mTool;
    };

}

#endif
