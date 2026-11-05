#include "services.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

#include <osg/Vec2f>

#include <components/esm/util.hpp>
#include <components/esm3/loadcell.hpp>
#include <components/esm3/loadcrea.hpp>
#include <components/esm3/loadgmst.hpp>
#include <components/esm3/loadnpc.hpp>

#include "../mwbase/environment.hpp"
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
#include "creaturestats.hpp"

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
}
