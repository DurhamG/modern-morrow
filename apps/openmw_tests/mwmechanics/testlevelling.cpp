#include "apps/openmw/mwmechanics/levelling.hpp"

#include <components/esm/attr.hpp>

#include <gtest/gtest.h>

namespace MWMechanics
{
    namespace
    {
        TEST(MWMechanicsRetroactiveHealthTest, levelOneShouldMatchCharacterCreation)
        {
            EXPECT_FLOAT_EQ(retroactiveHealth(40, 50, 50, 1, 0.1f), 45);
            EXPECT_FLOAT_EQ(retroactiveHealth(41, 50, 50, 1, 0.1f), 45);
        }

        TEST(MWMechanicsRetroactiveHealthTest, eachLevelShouldAddAShareOfTheCurrentEndurance)
        {
            EXPECT_FLOAT_EQ(retroactiveHealth(40, 50, 60, 5, 0.1f), 69);
        }

        TEST(MWMechanicsRetroactiveHealthTest, raisingEnduranceLateShouldCountForEveryLevel)
        {
            // Starting at Strength and Endurance 40, Morrowind's way gives 40 at level 1, then 4.5 at level 2 with
            // Endurance 45, and 5 at level 3 with Endurance 50, for 49.5 in all. Here both level ups count 50.
            EXPECT_FLOAT_EQ(retroactiveHealth(40, 40, 50, 3, 0.1f), 50);
        }

        ESM::Race raceWithStrength(int male, int female)
        {
            ESM::Race race;
            race.mData.setAttribute(ESM::Attribute::Strength, true, male);
            race.mData.setAttribute(ESM::Attribute::Strength, false, female);
            return race;
        }

        ESM::Class classFavouring(ESM::RefId first, ESM::RefId second)
        {
            ESM::Class playerClass;
            playerClass.mData.mAttribute = { first, second };
            return playerClass;
        }

        TEST(MWMechanicsStartingAttributeTest, shouldBeTheRacesValuePlusTheClassBonus)
        {
            const ESM::Race race = raceWithStrength(40, 30);
            const ESM::Class warrior = classFavouring(ESM::Attribute::Strength, ESM::Attribute::Endurance);
            const ESM::Class thief = classFavouring(ESM::Attribute::Agility, ESM::Attribute::Speed);
            EXPECT_FLOAT_EQ(startingAttribute(&race, true, &warrior, ESM::Attribute::Strength, 100), 50);
            EXPECT_FLOAT_EQ(startingAttribute(&race, false, &warrior, ESM::Attribute::Strength, 100), 40);
            EXPECT_FLOAT_EQ(startingAttribute(&race, true, &thief, ESM::Attribute::Strength, 100), 40);
        }

        TEST(MWMechanicsStartingAttributeTest, shouldBeNoMoreThanTheAttributeIsNow)
        {
            const ESM::Race race = raceWithStrength(40, 30);
            const ESM::Class warrior = classFavouring(ESM::Attribute::Strength, ESM::Attribute::Endurance);
            EXPECT_FLOAT_EQ(startingAttribute(&race, true, &warrior, ESM::Attribute::Strength, 30), 30);
            EXPECT_FLOAT_EQ(startingAttribute(nullptr, true, nullptr, ESM::Attribute::Strength, 35), 35);
        }
    }
}
