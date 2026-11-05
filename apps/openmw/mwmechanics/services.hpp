#ifndef OPENMW_MECHANICS_SERVICES_H
#define OPENMW_MECHANICS_SERVICES_H

#include <vector>

#include <components/esm/refid.hpp>
#include <components/esm3/transport.hpp>

namespace ESM
{
    struct Spell;
}

namespace MWWorld
{
    class Ptr;
}

namespace MWMechanics
{
    /// The skills @a trainer teaches: their three best skills, best first.
    std::vector<ESM::RefId> getTrainingSkills(const MWWorld::Ptr& trainer);

    /// The reason a trainer can't raise one of the player's skills.
    enum class TrainingRefusal
    {
        None,
        /// The player's skill is already as high as the trainer's.
        TrainerSkillTooLow,
        /// The player's skill is already as high as the attribute that governs it.
        AttributeTooLow,
    };

    /// Whether @a trainer can raise the player's @a skill by one point, and if not, why.
    /// This does not check whether the player can pay for it.
    TrainingRefusal getTrainingRefusal(const MWWorld::Ptr& trainer, ESM::RefId skill);

    /// The gold @a trainer asks to raise the player's @a skill by one point.
    int getTrainingPrice(const MWWorld::Ptr& trainer, ESM::RefId skill);

    /// The gold @a merchant asks to teach the player @a spell.
    int getSpellPrice(const MWWorld::Ptr& merchant, const ESM::Spell& spell);

    /// The gold @a merchant asks to restore @a item to full condition.
    int getRepairPrice(const MWWorld::Ptr& merchant, const MWWorld::Ptr& item);

    /// Whether @a merchant offers to teach the player @a spell. Merchants teach the spells they know, except
    /// the ones the player knows already, their race's powers, and anything that isn't a spell, such as a disease.
    bool isSpellForSale(const MWWorld::Ptr& merchant, const ESM::Spell& spell);

    /// Whether a merchant can repair @a item: a weapon or a piece of armor that has lost some of its condition.
    bool canMerchantRepair(const MWWorld::Ptr& item);

    /// The hours that pass while a trainer raises one of the player's skills.
    static constexpr int sTrainingHours = 2;

    /// Raises the player's @a skill by one point and makes the player pay @a trainer for it.
    /// This does not pass the time that training takes. passTrainingTime does that.
    void trainSkill(const MWWorld::Ptr& trainer, ESM::RefId skill);

    /// Passes the sTrainingHours that training takes. The player and other actors wait, but don't sleep.
    void passTrainingTime();

    /// Teaches the player @a spell and makes the player pay @a merchant for it.
    void buySpell(const MWWorld::Ptr& merchant, const ESM::Spell& spell);

    /// Restores @a item, which the player carries, to full condition and makes the player pay @a merchant for it.
    void repairItem(const MWWorld::Ptr& merchant, const MWWorld::Ptr& item);

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
