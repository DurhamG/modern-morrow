#include "apps/openmw/mwgui/mainmenubuttons.hpp"

#include <gtest/gtest.h>

namespace MWGui
{
    namespace
    {
        TEST(MWGuiMainMenuButtonsTest, controllerMenusShouldGrowTheButtonsWithTheScreen)
        {
            EXPECT_FLOAT_EQ(mainMenuButtonScale(true, 720), 1.f);
            EXPECT_FLOAT_EQ(mainMenuButtonScale(true, 1080), 1.5f);
            EXPECT_FLOAT_EQ(mainMenuButtonScale(true, 2160), 3.f);
            // The Fold 8's main screen.
            EXPECT_FLOAT_EQ(mainMenuButtonScale(true, 1848), 1848.f / 720.f);
        }

        TEST(MWGuiMainMenuButtonsTest, controllerMenusShouldNotShrinkTheButtons)
        {
            EXPECT_FLOAT_EQ(mainMenuButtonScale(true, 600), 1.f);
            EXPECT_FLOAT_EQ(mainMenuButtonScale(true, 0), 1.f);
        }

        TEST(MWGuiMainMenuButtonsTest, otherwiseTheButtonsShouldKeepTheTexturesSize)
        {
            EXPECT_FLOAT_EQ(mainMenuButtonScale(false, 600), 1.f);
            EXPECT_FLOAT_EQ(mainMenuButtonScale(false, 1080), 1.f);
            EXPECT_FLOAT_EQ(mainMenuButtonScale(false, 2160), 1.f);
        }

        TEST(MWGuiMainMenuButtonsTest, firstButtonShouldBeLoadOnlyWithSavesToLoad)
        {
            EXPECT_EQ(mainMenuFirstButton(true), "loadgame");
            EXPECT_EQ(mainMenuFirstButton(false), "newgame");
        }
    }
}
