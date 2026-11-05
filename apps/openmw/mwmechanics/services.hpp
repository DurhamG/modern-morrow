#ifndef OPENMW_MECHANICS_SERVICES_H
#define OPENMW_MECHANICS_SERVICES_H

#include <vector>

#include <components/esm3/transport.hpp>

namespace MWWorld
{
    class Ptr;
}

namespace MWMechanics
{
    /// The places @a merchant can take the player to.
    const std::vector<ESM::Transport::Dest>& getTravelDestinations(const MWWorld::Ptr& merchant);

    /// The gold @a merchant asks to take the player and their followers to @a destination.
    int getTravelPrice(const MWWorld::Ptr& merchant, const ESM::Transport::Dest& destination);

    /// The hours that pass on a trip with @a merchant to @a destination.
    /// A trip from an interior, such as with a guild guide, takes no time.
    int getTravelHours(const MWWorld::Ptr& merchant, const ESM::Transport::Dest& destination);

    /// Makes the player pay @a price to @a merchant, passes the hours the trip takes, and takes the player and their
    /// followers to @a destination.
    /// @param price The price from getTravelPrice. The caller works it out before anything can change it, such as the
    /// end of the conversation, which takes back the disposition that the player gained by persuasion.
    void travel(const MWWorld::Ptr& merchant, const ESM::Transport::Dest& destination, int price);
}

#endif
