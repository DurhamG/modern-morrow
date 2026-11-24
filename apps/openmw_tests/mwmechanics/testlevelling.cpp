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

        TEST(MWMechanicsBaseHealthToSetTest, turningTheSettingOnShouldRememberTheHealthBefore)
        {
            std::optional<float> morrowindHealth;
            std::optional<float> lastSet;
            EXPECT_FLOAT_EQ(baseHealthToSet(morrowindHealth, lastSet, 80, true, 99.f), 99);
            EXPECT_EQ(morrowindHealth, 80.f);
            EXPECT_EQ(lastSet, 99.f);
        }

        TEST(MWMechanicsBaseHealthToSetTest, turningTheSettingOffShouldGiveTheHealthBack)
        {
            std::optional<float> morrowindHealth = 80.f;
            std::optional<float> lastSet = 99.f;
            EXPECT_FLOAT_EQ(baseHealthToSet(morrowindHealth, lastSet, 99, false, std::nullopt), 80);
            EXPECT_EQ(morrowindHealth, std::nullopt);
            EXPECT_EQ(lastSet, std::nullopt);
        }

        TEST(MWMechanicsBaseHealthToSetTest, aLevelUpShouldCountTowardTheHealthUnderMorrowindsRules)
        {
            // The level up added 6, at Endurance 60, and the setting gives 105 at the new level.
            std::optional<float> morrowindHealth = 80.f;
            std::optional<float> lastSet = 99.f;
            EXPECT_FLOAT_EQ(baseHealthToSet(morrowindHealth, lastSet, 105, true, 105.f), 105);
            EXPECT_EQ(morrowindHealth, 86.f);
            EXPECT_FLOAT_EQ(baseHealthToSet(morrowindHealth, lastSet, 105, false, std::nullopt), 86);
        }

        TEST(MWMechanicsBaseHealthToSetTest, otherChangesShouldStayAndCountTowardTheHealthUnderMorrowindsRules)
        {
            // A werewolf's health is half as much again, and goes back when they change back.
            std::optional<float> morrowindHealth = 80.f;
            std::optional<float> lastSet = 100.f;
            EXPECT_FLOAT_EQ(baseHealthToSet(morrowindHealth, lastSet, 150, true, std::nullopt), 150);
            EXPECT_EQ(morrowindHealth, 130.f);
            EXPECT_FLOAT_EQ(baseHealthToSet(morrowindHealth, lastSet, 100, true, std::nullopt), 100);
            EXPECT_EQ(morrowindHealth, 80.f);
        }

        TEST(MWMechanicsBaseHealthToSetTest, aChangeInTheFrameTheSettingIsTurnedOffShouldCount)
        {
            std::optional<float> morrowindHealth = 80.f;
            std::optional<float> lastSet = 99.f;
            EXPECT_FLOAT_EQ(baseHealthToSet(morrowindHealth, lastSet, 104, false, std::nullopt), 85);
        }

        TEST(MWMechanicsBaseHealthToSetTest, loadingAGameShouldKeepItsHealth)
        {
            // A game saved with the setting on has the health under Morrowind's rules, but not the last health set.
            std::optional<float> morrowindHealth = 80.f;
            std::optional<float> lastSet;
            EXPECT_FLOAT_EQ(baseHealthToSet(morrowindHealth, lastSet, 99, true, std::nullopt), 99);
            EXPECT_EQ(morrowindHealth, 80.f);
            EXPECT_EQ(lastSet, 99.f);
        }

        TEST(MWMechanicsBaseHealthToSetTest, withTheSettingOffHealthShouldNotChange)
        {
            std::optional<float> morrowindHealth;
            std::optional<float> lastSet;
            EXPECT_FLOAT_EQ(baseHealthToSet(morrowindHealth, lastSet, 80, false, std::nullopt), 80);
            EXPECT_EQ(morrowindHealth, std::nullopt);
        }

        int morrowindMultiplier(int increases)
        {
            if (increases <= 4)
                return 2;
            if (increases <= 7)
                return 3;
            if (increases <= 9)
                return 4;
            return 5;
        }

        TEST(MWMechanicsIncreasesSpentTest, shouldSpendTheFewestForTheSameMultiplier)
        {
            EXPECT_EQ(increasesSpent(3, morrowindMultiplier), 1);
            EXPECT_EQ(increasesSpent(5, morrowindMultiplier), 5);
            EXPECT_EQ(increasesSpent(6, morrowindMultiplier), 5);
            EXPECT_EQ(increasesSpent(9, morrowindMultiplier), 8);
            EXPECT_EQ(increasesSpent(10, morrowindMultiplier), 10);
        }

        TEST(MWMechanicsIncreasesSpentTest, increasesPastTenShouldBeKept)
        {
            EXPECT_EQ(increasesSpent(14, morrowindMultiplier), 10);
        }

        TEST(MWMechanicsIncreasesSpentTest, noIncreasesShouldSpendNothing)
        {
            EXPECT_EQ(increasesSpent(0, morrowindMultiplier), 0);
            EXPECT_EQ(increasesSpent(-2, morrowindMultiplier), 0);
        }
    }
}
