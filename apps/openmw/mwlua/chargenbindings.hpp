#ifndef MWLUA_CHARGENBINDINGS_H
#define MWLUA_CHARGENBINDINGS_H

#include <sol/forward.hpp>

namespace MWLua
{
    struct Context;

    // Adds the functions for creating the player's character to types.Player
    void addCharGenBindings(sol::table& player, const Context& context);
}

#endif // MWLUA_CHARGENBINDINGS_H
