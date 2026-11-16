#ifndef MWGUI_MAINMENUBUTTONS_H
#define MWGUI_MAINMENUBUTTONS_H

namespace MWGui
{
    /// How many times bigger the main menu draws its buttons than Morrowind's textures, on a screen this many
    /// interface units tall. In controller menus mode they grow with the screen, as the controller screens do, which
    /// draw on a canvas 720 units tall, so they're as easy to read on a TV or a phone as the rest of the menus. They
    /// never get smaller than the textures. Otherwise they keep the textures' size, as in Morrowind.
    float mainMenuButtonScale(bool controllerMenus, int screenHeight);
}

#endif
