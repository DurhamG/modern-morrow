#include "actiontake.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwgui/inventorywindow.hpp"

#include "class.hpp"
#include "containerstore.hpp"

namespace MWWorld
{
    Ptr takeFromWorld(const Ptr& actor, const Ptr& object)
    {
        int count = object.getCellRef().getCount();
        if (object.getClass().isGold(object))
            count *= object.getClass().getValue(object);

        MWBase::Environment::get().getMechanicsManager()->itemTaken(actor, object, MWWorld::Ptr(), count);
        MWWorld::Ptr newitem = *actor.getClass().getContainerStore(actor).add(object, count);
        MWBase::Environment::get().getWorld()->deleteObject(object);
        return newitem;
    }

    ActionTake::ActionTake(const MWWorld::Ptr& object)
        : Action(true, object)
    {
    }

    void ActionTake::executeImp(const Ptr& actor)
    {
        // When in GUI mode, we should use drag and drop
        if (actor == MWBase::Environment::get().getWorld()->getPlayerPtr())
        {
            MWGui::GuiMode mode = MWBase::Environment::get().getWindowManager()->getMode();
            if (mode == MWGui::GM_Inventory || mode == MWGui::GM_Container)
            {
                MWBase::Environment::get().getWindowManager()->getInventoryWindow()->pickUpObject(getTarget());
                return;
            }
        }

        setTarget(takeFromWorld(actor, getTarget()));
    }
}
