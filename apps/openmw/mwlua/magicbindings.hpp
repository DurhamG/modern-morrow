#ifndef MWLUA_MAGICBINDINGS_H
#define MWLUA_MAGICBINDINGS_H

#include <sol/forward.hpp>

namespace ESM
{
    struct Spell;
}

namespace MWLua
{
    struct Context;

    sol::table initCoreMagicBindings(const Context& context);
    void addActorMagicBindings(sol::table& actor, const Context& context);

    // Finds the spell that a Lua argument names, either as a spell record or as a record ID.
    const ESM::Spell* toSpell(const sol::object& spellOrId);
}

#endif // MWLUA_MAGICBINDINGS_H
