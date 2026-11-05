#include "services.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

#include <osg/Vec2f>

#include <components/esm3/loadarmo.hpp>
#include <components/esm3/loadcell.hpp>
#include <components/esm3/loadcrea.hpp>
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

#include "../mwworld/actionteleport.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/containerstore.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/ptr.hpp"
#include "../mwworld/worldmodel.hpp"

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

        // Whether a travel destination is in an interior. Some exterior cells have names too.
        bool isInteriorDestination(const ESM::Transport::Dest& destination)
        {
            if (destination.mCellName.empty())
                return false;
            const MWWorld::CellStore* cell
                = MWBase::Environment::get().getWorldModel()->findCell(destination.mCellName, false);
            if (cell == nullptr)
                throw std::runtime_error("Unknown travel destination: " + destination.mCellName);
            return !cell->getCell()->isExterior();
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

    const std::vector<ESM::Transport::Dest>& getTravelDestinations(const MWWorld::Ptr& merchant)
    {
        static const std::vector<ESM::Transport::Dest> none;
        if (merchant.getClass().isNpc())
            return merchant.get<ESM::NPC>()->mBase->getTransport();
        if (merchant.getType() == ESM::Creature::sRecordId)
            return merchant.get<ESM::Creature>()->mBase->getTransport();
        return none;
    }

    int getTravelPrice(const MWWorld::Ptr& merchant, const ESM::Transport::Dest& destination)
    {
        int price;

        const MWWorld::Store<ESM::GameSetting>& gmst = getGameSettings();

        const MWWorld::Ptr player = getPlayer();
        const ESM::Position& pos = destination.mPos;

        if (!merchant.getCell()->isExterior())
        {
            price = gmst.find("fMagesGuildTravel")->mValue.getInteger();
        }
        else
        {
            const ESM::Position playerPos = player.getRefData().getPosition();
            double d = std::sqrt(std::pow(pos.pos[0] - playerPos.pos[0], 2) + std::pow(pos.pos[1] - playerPos.pos[1], 2)
                + std::pow(pos.pos[2] - playerPos.pos[2], 2));
            float fTravelMult = gmst.find("fTravelMult")->mValue.getFloat();
            if (fTravelMult != 0)
                price = static_cast<int>(d / fTravelMult);
            else
                price = static_cast<int>(d);
        }

        // Add price for the travelling followers
        std::set<MWWorld::Ptr> followers;
        MWWorld::ActionTeleport::getFollowers(player, followers, !isInteriorDestination(destination));

        // Apply followers cost, unlike vanilla the first follower doesn't travel for free
        price *= 1 + static_cast<int>(followers.size());

        price = std::max(1, price);
        return MWBase::Environment::get().getMechanicsManager()->getBarterOffer(merchant, price, true);
    }

    int getTravelHours(const MWWorld::Ptr& merchant, const ESM::Transport::Dest& destination)
    {
        if (!merchant.getCell()->isExterior())
            return 0;
        const ESM::Position& pos = destination.mPos;
        const ESM::Position playerPos = getPlayer().getRefData().getPosition();
        float d = (osg::Vec2f(pos.pos[0], pos.pos[1]) - osg::Vec2f(playerPos.pos[0], playerPos.pos[1])).length();
        const float fTravelTimeMult = getGameSettings().find("fTravelTimeMult")->mValue.getFloat();
        if (fTravelTimeMult == 0)
            return 0;
        return static_cast<int>(d / fTravelTimeMult);
    }

    void travel(const MWWorld::Ptr& merchant, const ESM::Transport::Dest& destination, int price)
    {
        const bool interior = isInteriorDestination(destination);

        // Set "traveling" flag, so GetPCTraveling can detect teleportation.
        // We will reset this flag during next world update.
        MWBase::Environment::get().getWorld()->setPlayerTraveling(true);

        payMerchant(merchant, price);

        if (merchant.getCell()->isExterior())
        {
            const int hours = getTravelHours(merchant, destination);
            MWBase::Environment::get().getMechanicsManager()->rest(hours, true);
            MWBase::Environment::get().getWorld()->advanceTime(hours);
        }

        const ESM::Position& pos = destination.mPos;
        const ESM::ExteriorCellLocation posCell = ESM::positionToExteriorCellLocation(pos.pos[0], pos.pos[1]);
        ESM::RefId cellId = ESM::Cell::generateIdForCell(!interior, destination.mCellName, posCell.mX, posCell.mY);

        // Teleports any followers, too.
        MWWorld::ActionTeleport action(cellId, pos, true);
        action.execute(getPlayer());
    }

    std::vector<DialogueService> getDialogueServices(const MWWorld::Ptr& actor)
    {
        std::vector<DialogueService> services;
        const int flags = actor.getClass().getServices(actor);
        if (actor.getClass().isNpc())
            services.push_back(DialogueService::Persuasion);
        if (flags & ESM::NPC::AllItems)
            services.push_back(DialogueService::Barter);
        if (flags & ESM::NPC::Spells)
            services.push_back(DialogueService::Spells);
        if (!getTravelDestinations(actor).empty())
            services.push_back(DialogueService::Travel);
        if (flags & ESM::NPC::Spellmaking)
            services.push_back(DialogueService::Spellmaking);
        if (flags & ESM::NPC::Enchanting)
            services.push_back(DialogueService::Enchanting);
        if (flags & ESM::NPC::Training)
            services.push_back(DialogueService::Training);
        if (flags & ESM::NPC::Repair)
            services.push_back(DialogueService::Repair);
        if (isCompanion(actor))
            services.push_back(DialogueService::CompanionShare);
        return services;
    }

    bool isCompanion(const MWWorld::Ptr& actor)
    {
        const ESM::RefId& script = actor.getClass().getScript(actor);
        return !script.empty() && actor.getRefData().getLocals().getIntVar(script, "companion");
    }
}
