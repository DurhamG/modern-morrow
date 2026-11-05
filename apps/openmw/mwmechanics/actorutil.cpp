#include "actorutil.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/scriptmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/player.hpp"

#include "../mwmechanics/aipackage.hpp"
#include "../mwmechanics/creaturestats.hpp"
#include "../mwmechanics/magiceffects.hpp"
#include "../mwmechanics/summoning.hpp"

#include "../mwscript/interpretercontext.hpp"

#include <algorithm>

#include <components/esm3/loadmgef.hpp>

namespace MWMechanics
{
    MWWorld::Ptr getPlayer()
    {
        return MWBase::Environment::get().getWorld()->getPlayerPtr();
    }

    bool isPlayerInCombat()
    {
        return MWBase::Environment::get().getWorld()->getPlayer().isInCombat();
    }

    bool canActorMoveByZAxis(const MWWorld::Ptr& actor)
    {
        MWBase::World* world = MWBase::Environment::get().getWorld();
        return (actor.getClass().canSwim(actor) && world->isSwimming(actor)) || world->isFlying(actor);
    }

    bool hasWaterWalking(const MWWorld::Ptr& actor)
    {
        const MWMechanics::MagicEffects& effects = actor.getClass().getCreatureStats(actor).getMagicEffects();
        return effects.getOrDefault(ESM::MagicEffect::WaterWalking).getMagnitude() > 0;
    }

    bool isTargetMagicallyHidden(const MWWorld::Ptr& actor)
    {
        const MagicEffects& magicEffects = actor.getClass().getCreatureStats(actor).getMagicEffects();
        return (magicEffects.getOrDefault(ESM::MagicEffect::Invisibility).getMagnitude() > 0)
            || (magicEffects.getOrDefault(ESM::MagicEffect::Chameleon).getMagnitude() >= 75);
    }

    bool disposeCorpse(const MWWorld::Ptr& actor)
    {
        if (actor.getClass().isPersistent(actor))
            return false;

        CreatureStats& creatureStats = actor.getClass().getCreatureStats(actor);

        // If we dispose corpse before end of death animation, we should update death counter counter manually.
        // Also we should run actor's script - it may react on actor's death.
        if (creatureStats.isDead() && !creatureStats.isDeathAnimationFinished())
        {
            creatureStats.setDeathAnimationFinished(true);
            MWBase::Environment::get().getMechanicsManager()->notifyDied(actor);

            const ESM::RefId& script = actor.getClass().getScript(actor);
            if (!script.empty() && MWBase::Environment::get().getWorld()->getScriptsEnabled()
                && actor.getRefData().isLocalScriptEnabled())
            {
                MWScript::InterpreterContext interpreterContext(&actor.getRefData().getLocals(), actor);
                MWBase::Environment::get().getScriptManager()->run(script, interpreterContext);
            }

            // Clean up summoned creatures as well
            auto& creatureMap = creatureStats.getSummonedCreatureMap();
            for (const auto& creature : creatureMap)
                MWBase::Environment::get().getMechanicsManager()->cleanupSummonedCreature(creature.second);
            creatureMap.clear();

            // Check if we are a summon and inform our master we've bit the dust
            for (const auto& package : creatureStats.getAiSequence())
            {
                if (package->followTargetThroughDoors() && !package->getTarget().isEmpty())
                {
                    const auto& summoner = package->getTarget();
                    auto& summons = summoner.getClass().getCreatureStats(summoner).getSummonedCreatureMap();
                    auto it = std::find_if(summons.begin(), summons.end(),
                        [&](const auto& entry) { return entry.second == actor.getCellRef().getRefNum(); });
                    if (it != summons.end())
                    {
                        auto summon = *it;
                        summons.erase(it);
                        purgeSummonEffect(summoner, summon);
                        break;
                    }
                }
            }
        }

        MWBase::Environment::get().getWorld()->deleteObject(actor);
        return true;
    }
}
