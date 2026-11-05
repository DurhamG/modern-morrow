#include "services.hpp"

#include <algorithm>

#include <components/esm3/loadgmst.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/ptr.hpp"

#include "actorutil.hpp"
#include "npcstats.hpp"
#include "spellutil.hpp"

namespace MWMechanics
{
    namespace
    {
        const MWWorld::Store<ESM::GameSetting>& getGameSettings()
        {
            return MWBase::Environment::get().getESMStore()->get<ESM::GameSetting>();
        }
    }

    int getTrainingPrice(const MWWorld::Ptr& trainer, ESM::RefId skill)
    {
        const MWWorld::Ptr player = getPlayer();
        const NpcStats& playerStats = player.getClass().getNpcStats(player);
        const int price = static_cast<int>(
            playerStats.getSkill(skill).getBase() * getGameSettings().find("iTrainingMod")->mValue.getInteger());
        return MWBase::Environment::get().getMechanicsManager()->getBarterOffer(trainer, std::max(1, price), true);
    }

    int getSpellPrice(const MWWorld::Ptr& merchant, const ESM::Spell& spell)
    {
        const int price = std::max(
            1, static_cast<int>(calcSpellCost(spell) * getGameSettings().find("fSpellValueMult")->mValue.getFloat()));
        return MWBase::Environment::get().getMechanicsManager()->getBarterOffer(merchant, price, true);
    }

    int getRepairPrice(const MWWorld::Ptr& merchant, const MWWorld::Ptr& item)
    {
        const int maxDurability = item.getClass().getItemMaxHealth(item);
        const int durability = item.getClass().getItemHealth(item);
        const int basePrice = item.getClass().getValue(item);
        const float fRepairMult = getGameSettings().find("fRepairMult")->mValue.getFloat();

        float p = static_cast<float>(std::max(1, basePrice));
        float r = static_cast<float>(std::max(1, static_cast<int>(maxDurability / p)));

        int x = static_cast<int>((maxDurability - durability) / r);
        x = static_cast<int>(fRepairMult * x);
        x = std::max(1, x);

        return MWBase::Environment::get().getMechanicsManager()->getBarterOffer(merchant, x, true);
    }
}
