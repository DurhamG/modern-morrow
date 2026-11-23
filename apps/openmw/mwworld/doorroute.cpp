#include "doorroute.hpp"

#include <deque>
#include <unordered_map>

#include <components/esm3/loadcell.hpp>

namespace MWWorld
{
    ESM::RefId routePlace(const ESM::RefId& cell)
    {
        if (cell.is<ESM::ESM3ExteriorCellRefId>())
            return ESM::Cell::sDefaultWorldspaceId;
        return cell;
    }

    std::optional<std::vector<const DoorLink*>> findDoorRoute(
        std::span<const DoorLink> doors, const ESM::RefId& from, const ESM::RefId& to)
    {
        const ESM::RefId start = routePlace(from);
        const ESM::RefId goal = routePlace(to);
        if (start == goal)
            return std::vector<const DoorLink*>();

        std::unordered_multimap<ESM::RefId, const DoorLink*> doorsFrom;
        for (const DoorLink& door : doors)
            doorsFrom.emplace(routePlace(door.mCell), &door);

        // A search by doors, so the first route found goes through the fewest. Each place remembers the door the
        // player came in through.
        std::unordered_map<ESM::RefId, const DoorLink*> cameThrough{ { start, nullptr } };
        std::deque<ESM::RefId> queue{ start };
        while (!queue.empty())
        {
            const ESM::RefId place = queue.front();
            queue.pop_front();
            const auto [begin, end] = doorsFrom.equal_range(place);
            for (auto it = begin; it != end; ++it)
            {
                const ESM::RefId next = routePlace(it->second->mDestCell);
                if (cameThrough.contains(next))
                    continue;
                cameThrough.emplace(next, it->second);
                if (next == goal)
                {
                    std::vector<const DoorLink*> route;
                    for (ESM::RefId at = goal; at != start;)
                    {
                        const DoorLink* door = cameThrough.at(at);
                        route.push_back(door);
                        at = routePlace(door->mCell);
                    }
                    return std::vector<const DoorLink*>(route.rbegin(), route.rend());
                }
                queue.push_back(next);
            }
        }
        return std::nullopt;
    }
}
