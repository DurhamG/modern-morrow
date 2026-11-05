#ifndef MWLUA_CONVERSATIONBINDINGS_H
#define MWLUA_CONVERSATIONBINDINGS_H

#include <sol/forward.hpp>

namespace MWLua
{
    struct Context;

    // Adds the functions for the player's current conversation to types.Player
    void addConversationBindings(sol::table& player, const Context& context);

    // Adds the functions for the services that actors offer in a conversation to types.Actor
    void addDialogueServiceBindings(sol::table& actor);
}

#endif // MWLUA_CONVERSATIONBINDINGS_H
