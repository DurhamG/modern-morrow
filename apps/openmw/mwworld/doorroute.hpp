#ifndef OPENMW_MWWORLD_DOORROUTE_H
#define OPENMW_MWWORLD_DOORROUTE_H

#include <optional>
#include <span>
#include <vector>

#include <osg/Vec3f>

#include <components/esm/refid.hpp>

namespace MWWorld
{
    /// A door placed in the content that takes the player to another place.
    struct DoorLink
    {
        ESM::RefId mCell;
        osg::Vec3f mPos;
        ESM::RefId mDestCell;
        osg::Vec3f mDestPos;
    };

    /// The place a cell belongs to for routes. Every exterior cell of Morrowind's exterior is one place, since the
    /// player can walk between them. An interior is a place of its own.
    ESM::RefId routePlace(const ESM::RefId& cell);

    /// The doors the player goes through from one cell to another, fewest first. Empty when both cells are in the
    /// same place, and nothing when no doors lead there, such as to Mournhold, which the player is teleported to.
    std::optional<std::vector<const DoorLink*>> findDoorRoute(
        std::span<const DoorLink> doors, const ESM::RefId& from, const ESM::RefId& to);
}

#endif
