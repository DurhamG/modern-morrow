#ifndef MWGUI_MAINMENUBUTTONS_H
#define MWGUI_MAINMENUBUTTONS_H

#include <string_view>

namespace MWGui
{
    /// How many times bigger the main menu draws its buttons than Morrowind's textures, on a screen this many
    /// interface units tall. In controller menus mode they grow with the screen, as the controller screens do, which
    /// draw on a canvas 720 units tall, so they're as easy to read on a TV or a phone as the rest of the menus. They
    /// never get smaller than the textures. Otherwise they keep the textures' size, as in Morrowind.
    float mainMenuButtonScale(bool controllerMenus, int screenHeight);

    /// The button the main menu highlights first: Load when there's a save to load, and New otherwise. In controller
    /// menus mode Load shows even with no saves, for its Cloud tab, but a new player should start a new game.
    std::string_view mainMenuFirstButton(bool anySaves);
}

#endif
