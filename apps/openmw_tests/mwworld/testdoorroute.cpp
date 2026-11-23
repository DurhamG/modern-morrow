#include "apps/openmw/mwworld/doorroute.hpp"

#include <components/esm3/loadcell.hpp>

#include <gtest/gtest.h>

namespace
{
    using namespace MWWorld;

    const ESM::RefId outsideA = ESM::RefId::esm3ExteriorCell(-3, -2);
    const ESM::RefId outsideB = ESM::RefId::esm3ExteriorCell(-2, -2);
    const ESM::RefId guild = ESM::RefId::stringRefId("Balmora, Guild of Fighters");
    const ESM::RefId guildUpstairs = ESM::RefId::stringRefId("Balmora, Guild of Fighters, Upstairs");
    const ESM::RefId house = ESM::RefId::stringRefId("Balmora, Caius Cosades' House");
    const ESM::RefId mournhold = ESM::RefId::stringRefId("Mournhold, Plaza Brindisi Dorom");

    DoorLink door(ESM::RefId from, ESM::RefId to, float x = 0)
    {
        return DoorLink{ from, osg::Vec3f(x, 0, 0), to, osg::Vec3f() };
    }

    const std::vector<DoorLink> doors{
        door(outsideA, guild, 1),
        door(guild, outsideA, 2),
        door(guild, guildUpstairs, 3),
        door(guildUpstairs, guild, 4),
        door(outsideB, house, 5),
        door(house, outsideB, 6),
    };

    TEST(MWWorldDoorRouteTest, everyExteriorCellIsOnePlace)
    {
        EXPECT_EQ(routePlace(outsideA), ESM::Cell::sDefaultWorldspaceId);
        EXPECT_EQ(routePlace(outsideA), routePlace(outsideB));
        EXPECT_EQ(routePlace(guild), guild);
    }

    TEST(MWWorldDoorRouteTest, noDoorsWithinOnePlace)
    {
        const auto route = findDoorRoute(doors, outsideA, outsideB);
        ASSERT_TRUE(route.has_value());
        EXPECT_TRUE(route->empty());
    }

    TEST(MWWorldDoorRouteTest, fromOutsideToAnUpperFloorGoesThroughTwoDoors)
    {
        const auto route = findDoorRoute(doors, outsideB, guildUpstairs);
        ASSERT_TRUE(route.has_value());
        ASSERT_EQ(route->size(), 2u);
        EXPECT_EQ((*route)[0]->mPos.x(), 1);
        EXPECT_EQ((*route)[1]->mPos.x(), 3);
    }

    TEST(MWWorldDoorRouteTest, fromOneInteriorToAnotherGoesOutside)
    {
        const auto route = findDoorRoute(doors, guildUpstairs, house);
        ASSERT_TRUE(route.has_value());
        ASSERT_EQ(route->size(), 3u);
        EXPECT_EQ((*route)[0]->mPos.x(), 4);
        EXPECT_EQ((*route)[1]->mPos.x(), 2);
        EXPECT_EQ((*route)[2]->mPos.x(), 5);
    }

    TEST(MWWorldDoorRouteTest, aPlaceNoDoorLeadsToHasNoRoute)
    {
        EXPECT_FALSE(findDoorRoute(doors, outsideA, mournhold).has_value());
    }
}
