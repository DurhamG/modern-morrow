#ifndef OPENMW_MECHANICS_BARTER_H
#define OPENMW_MECHANICS_BARTER_H

#include <vector>

namespace MWWorld
{
    class Ptr;
}

namespace MWMechanics
{
    /// The value of @a count of @a item in a trade, before the merchant's offer.
    /// Items that wear down are worth less as their condition drops.
    int getEffectiveItemValue(const MWWorld::Ptr& item, int count);

    /// Finds the goods that @a merchant offers in a trade. They are the merchant's own inventory, the containers in
    /// the active cells that the merchant owns, and the items lying in the active cells that the merchant owns.
    /// Containers that can't hold anything don't count, as in the original game.
    /// @param containers Gets the merchant first, so that the player buys from the merchant's own inventory before the
    /// containers, and then the containers.
    /// @param worldItems Gets the items lying in the world.
    void getBarterSources(
        const MWWorld::Ptr& merchant, std::vector<MWWorld::Ptr>& containers, std::vector<MWWorld::Ptr>& worldItems);

    /// The gold that changes hands when the player trades @a count of @a item with @a merchant, before haggling.
    /// @param playerBuys true when the player buys the item from the merchant, false when the player sells it.
    int getBarterPrice(const MWWorld::Ptr& merchant, const MWWorld::Ptr& item, int count, bool playerBuys);

    /// Whether @a merchant buys @a item from the player or sells it to them. Merchants don't trade gold, bound items,
    /// the items that they wear, or the kinds of item that their services leave out.
    bool canBarterItem(const MWWorld::Ptr& merchant, const MWWorld::Ptr& item);

    /// A reason why a merchant doesn't consider an offer.
    enum class BarterProblem
    {
        None,
        PlayerLacksGold,
        MerchantLacksGold,
    };

    /// The first check that an offer fails before the merchant haggles over it.
    /// @param balance The gold that the player asks for. It is negative when the player pays.
    BarterProblem getBarterProblem(const MWWorld::Ptr& merchant, int balance);

    /// Rolls whether @a merchant accepts the player's offer. When the merchant accepts, the player's Mercantile skill
    /// progresses. If the player is talking to @a merchant, the merchant's disposition goes up or down.
    /// @param playerOffer The gold that the player asks for. It is negative when the player pays.
    /// @param merchantOffer The gold that the merchant offers, from getBarterPrice. It is negative when the player
    /// pays.
    bool haggle(const MWWorld::Ptr& merchant, int playerOffer, int merchantOffer);

    /// Moves @a balance gold from the merchant's barter gold to the player, or from the player to the merchant's
    /// barter gold when it is negative.
    void payBarterBalance(const MWWorld::Ptr& merchant, int balance);
}

#endif
