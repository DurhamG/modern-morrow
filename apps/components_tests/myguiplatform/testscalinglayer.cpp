#include <components/myguiplatform/scalinglayer.hpp>

#include <gtest/gtest.h>

namespace MyGUIPlatform
{
    namespace
    {
        TEST(MyGuiPlatformScalingLayerTest, fixedHeightShouldKeepTheScreensShape)
        {
            EXPECT_EQ(fixedHeightViewSize(MyGUI::IntSize(1920, 1080), 720), MyGUI::IntSize(1280, 720));
            EXPECT_EQ(fixedHeightViewSize(MyGUI::IntSize(2448, 1848), 720), MyGUI::IntSize(954, 720));
            EXPECT_EQ(fixedHeightViewSize(MyGUI::IntSize(1280, 800), 720), MyGUI::IntSize(1152, 720));
        }

        TEST(MyGuiPlatformScalingLayerTest, layerCornersShouldMapToScreenCorners)
        {
            // A 1280x720 layer filling a 1920x1080 screen at 1.5 times its size.
            const MyGUI::IntSize layer(1280, 720);
            const MyGUI::IntSize screen(1920, 1080);
            EXPECT_EQ(layerToScreen(MyGUI::IntCoord(0, 0, 1280, 720), layer, screen, 1.5f),
                MyGUI::IntCoord(0, 0, 1920, 1080));
        }

        TEST(MyGuiPlatformScalingLayerTest, layerCentreShouldMapToTheScreensCentre)
        {
            const MyGUI::IntSize layer(1280, 720);
            const MyGUI::IntSize screen(1920, 1080);
            EXPECT_EQ(layerToScreen(MyGUI::IntCoord(640, 360, 0, 0), layer, screen, 1.5f),
                MyGUI::IntCoord(960, 540, 0, 0));
        }

        TEST(MyGuiPlatformScalingLayerTest, layerNarrowerThanTheScreenShouldBeCentred)
        {
            // A fixed-size 800x600 layer on a wide screen, scaled to the screen's height.
            const MyGUI::IntSize layer(800, 600);
            const MyGUI::IntSize screen(1920, 1080);
            EXPECT_EQ(layerToScreen(MyGUI::IntCoord(0, 0, 800, 600), layer, screen, 1.8f),
                MyGUI::IntCoord(240, 0, 1440, 1080));
        }

        TEST(MyGuiPlatformScalingLayerTest, sizeShouldScale)
        {
            const MyGUI::IntCoord box = layerToScreen(
                MyGUI::IntCoord(100, 100, 100, 50), MyGUI::IntSize(1000, 1000), MyGUI::IntSize(3000, 3000), 3.f);
            EXPECT_EQ(box.width, 300);
            EXPECT_EQ(box.height, 150);
            EXPECT_EQ(box.left, 300);
        }
    }
}
