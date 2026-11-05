#include "services.hpp"

#include <algorithm>

#include <components/esm3/loadarmo.hpp>
#include <components/esm3/loadgmst.hpp>
#include <components/esm3/loadnpc.hpp>
#include <components/esm3/loadrace.hpp>
#include <components/esm3/loadskil.hpp>
#include <components/esm3/loadspel.hpp>
#include <components/esm3/loadweap.hpp>
#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/luamanager.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/containerstore.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/ptr.hpp"

#include "actorutil.hpp"
#include "npcstats.hpp"
#include "spells.hpp"
#include "spellutil.hpp"

namespace MWMechanics
{
    namespace
    {
        const MWWorld::Store<ESM::GameSetting>& getGameSettings()
        {
            return MWBase::Environment::get().getESMStore()->get<ESM::GameSetting>();
        }

        void payMerchant(const MWWorld::Ptr& merchant, int price)
        {
            const MWWorld::Ptr player = getPlayer();
            player.getClass().getContainerStore(player).remove(MWWorld::ContainerStore::sGoldId, price);

            // add gold to NPC trading gold pool
            CreatureStats& merchantStats = merchant.getClass().getCreatureStats(merchant);
            merchantStats.setGoldPool(merchantStats.getGoldPool() + price);
        }

        // Retrieve the base skill value if the setting 'training skills based on base skill' is set;
        // otherwise returns the modified skill
        float getSkillForTraining(const NpcStats& stats, ESM::RefId id)
        {
            if (Settings::game().mTrainersTrainingSkillsBasedOnBaseSkill)
                return stats.getSkill(id).getBase();
            return stats.getSkill(id).getModified();
        }
    }

    std::vector<ESM::RefId> getTrainingSkills(const MWWorld::Ptr& trainer)
    {
        // NPC can train you in their best 3 skills
        constexpr size_t maxSkills = 3;
        std::vector<std::pair<ESM::RefId, float>> skills;
        skills.reserve(maxSkills);

        const auto sortByValue = [](const std::pair<ESM::RefId, float>& lhs, const std::pair<ESM::RefId, float>& rhs) {
            return lhs.second > rhs.second;
        };
        // Maintain a sorted vector of max maxSkills elements, ordering skills by value and content file order
        const NpcStats& trainerStats = trainer.getClass().getNpcStats(trainer);
        for (const ESM::Skill& skill : MWBase::Environment::get().getESMStore()->get<ESM::Skill>())
        {
            float value = getSkillForTraining(trainerStats, skill.mId);
            if (skills.size() < maxSkills)
            {
                skills.emplace_back(skill.mId, value);
                std::stable_sort(skills.begin(), skills.end(), sortByValue);
            }
            else
            {
                auto& lowest = skills[maxSkills - 1];
                if (lowest.second < value)
                {
                    lowest.first = skill.mId;
                    lowest.second = value;
                    std::stable_sort(skills.begin(), skills.end(), sortByValue);
                }
            }
        }

        std::vector<ESM::RefId> result;
        result.reserve(skills.size());
        for (const auto& [skill, value] : skills)
            result.push_back(skill);
        return result;
    }

    TrainingRefusal getTrainingRefusal(const MWWorld::Ptr& trainer, ESM::RefId skill)
    {
        const MWWorld::Ptr player = getPlayer();
        const NpcStats& playerStats = player.getClass().getNpcStats(player);
        const float playerSkill = playerStats.getSkill(skill).getBase();

        if (getSkillForTraining(trainer.getClass().getNpcStats(trainer), skill) <= playerSkill)
            return TrainingRefusal::TrainerSkillTooLow;

        // You can not train a skill above its governing attribute
        const ESM::Skill* record = MWBase::Environment::get().getESMStore()->get<ESM::Skill>().find(skill);
        if (playerSkill >= playerStats.getAttribute(record->mData.mAttribute).getModified())
            return TrainingRefusal::AttributeTooLow;

        return TrainingRefusal::None;
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

    bool isSpellForSale(const MWWorld::Ptr& merchant, const ESM::Spell& spell)
    {
        if (spell.mData.mType != ESM::Spell::ST_Spell)
            return false; // don't try to sell diseases, curses or powers

        if (!merchant.getClass().getCreatureStats(merchant).getSpells().hasSpell(&spell))
            return false;

        if (merchant.getClass().isNpc())
        {
            const ESM::Race* race = MWBase::Environment::get().getESMStore()->get<ESM::Race>().find(
                merchant.get<ESM::NPC>()->mBase->mRace);
            if (race->mPowers.exists(spell.mId))
                return false;
        }

        const MWWorld::Ptr player = getPlayer();
        return !player.getClass().getCreatureStats(player).getSpells().hasSpell(spell.mId);
    }

    bool canMerchantRepair(const MWWorld::Ptr& item)
    {
        if (item.getType() != ESM::Weapon::sRecordId && item.getType() != ESM::Armor::sRecordId)
            return false;
        if (!item.getClass().hasItemHealth(item))
            return false;
        const int maxDurability = item.getClass().getItemMaxHealth(item);
        return maxDurability != 0 && item.getClass().getItemHealth(item) < maxDurability;
    }

    void trainSkill(const MWWorld::Ptr& trainer, ESM::RefId skill)
    {
        const int price = getTrainingPrice(trainer, skill);

        // increase skill
        MWBase::Environment::get().getLuaManager()->skillLevelUp(getPlayer(), skill, "trainer");

        payMerchant(trainer, price);
    }

    void passTrainingTime()
    {
        MWBase::Environment::get().getMechanicsManager()->rest(sTrainingHours, false);
        MWBase::Environment::get().getWorld()->advanceTime(sTrainingHours);
    }

    void buySpell(const MWWorld::Ptr& merchant, const ESM::Spell& spell)
    {
        const int price = getSpellPrice(merchant, spell);

        const MWWorld::Ptr player = getPlayer();
        player.getClass().getCreatureStats(player).getSpells().add(&spell);

        payMerchant(merchant, price);
    }

    void repairItem(const MWWorld::Ptr& merchant, const MWWorld::Ptr& item)
    {
        const int price = getRepairPrice(merchant, item);

        item.getCellRef().setCharge(item.getClass().getItemMaxHealth(item));

        const MWWorld::Ptr player = getPlayer();
        player.getClass().getContainerStore(player).restack(item);

        payMerchant(merchant, price);
    }
}
