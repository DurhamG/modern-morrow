#ifndef OPENMW_MECHANICS_SERVICES_H
#define OPENMW_MECHANICS_SERVICES_H

#include <vector>

#include <components/esm/refid.hpp>

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
}

#endif
