#include "mainmenubuttons.hpp"

#include <algorithm>

namespace MWGui
{
    float mainMenuButtonScale(bool controllerMenus, int screenHeight)
    {
        if (!controllerMenus)
            return 1.f;
        return std::max(1.f, static_cast<float>(screenHeight) / 720.f);
    }

    std::string_view mainMenuFirstButton(bool anySaves)
    {
        return anySaves ? "loadgame" : "newgame";
    }
}
