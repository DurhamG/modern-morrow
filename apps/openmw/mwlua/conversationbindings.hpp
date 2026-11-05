#ifndef MWLUA_CONVERSATIONBINDINGS_H
#define MWLUA_CONVERSATIONBINDINGS_H

#include <sol/forward.hpp>

namespace MWLua
{
    struct Context;

    // Adds the functions for the player's current conversation to types.Player
    void addConversationBindings(sol::table& player, const Context& context);
}

#endif // MWLUA_CONVERSATIONBINDINGS_H
