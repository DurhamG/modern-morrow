#include <sol/sol.hpp>

#include "../../mwbase/environment.hpp"
#include "../../mwbase/mechanicsmanager.hpp"
#include "../../mwmechanics/spellutil.hpp"
#include "../../mwworld/class.hpp"

#include "../itemdata.hpp"

#include "types.hpp"

namespace MWLua
{
    void addItemBindings(sol::table item, const Context& context)
    {
        // Deprecated. Moved to itemData; should be removed later
        item["getEnchantmentCharge"] = [](const Object& object) -> sol::optional<float> {
            float charge = object.ptr().getCellRef().getEnchantmentCharge();
            if (charge == -1)
                return sol::nullopt;
            else
                return charge;
        };
        item["setEnchantmentCharge"] = [](const GObject& object, sol::optional<float> charge) {
            object.ptr().getCellRef().setEnchantmentCharge(charge.value_or(-1));
        };
        item["isRestocking"]
            = [](const Object& object) -> bool { return object.ptr().getCellRef().getCount(false) < 0; };

        item["isCarriable"] = [](const Object& object) -> bool { return object.ptr().getClass().isItem(object.ptr()); };

        item["getValue"] = [](const Object& object) -> int {
            const MWWorld::Ptr& ptr = object.ptr();
            if (!ptr.getClass().isItem(ptr))
                throw std::runtime_error("Item expected");
            return ptr.getClass().getValue(ptr);
        };

        item["getStolenFrom"] = [](sol::this_state thisState, const Object& object) {
            const MWWorld::Ptr& ptr = object.ptr();
            if (!ptr.getClass().isItem(ptr))
                throw std::runtime_error("Item expected");
            sol::table result(thisState, sol::create);
            for (const MWBase::MechanicsManager::StolenItemOwner& owner :
                MWBase::Environment::get().getMechanicsManager()->getStolenItemOwners(ptr.getCellRef().getRefId()))
            {
                sol::table entry(thisState, sol::create);
                entry[owner.mIsFaction ? "factionId" : "recordId"] = owner.mId.serializeText();
                entry["count"] = owner.mCount;
                result.add(entry);
            }
            return result;
        };

        item["getEquipmentSlots"] = [](sol::this_state thisState, const Object& object) {
            const MWWorld::Ptr& ptr = object.ptr();
            if (!ptr.getClass().isItem(ptr))
                throw std::runtime_error("Item expected");
            sol::table result(thisState, sol::create);
            for (int slot : ptr.getClass().getEquipmentSlots(ptr).first)
                result.add(slot);
            return result;
        };

        addItemDataBindings(item, context);
        item["levelledItems"] = addLevelledItemBindings(context);
    }
}
