local self = require('openmw.self')
local types = require('openmw.types')
self:enableAI(false)
return {
    eventHandlers = {
        -- Global scripts can't change an actor's health, so a test sends this event to kill the actor.
        TestKill = function()
            types.Actor.stats.dynamic.health(self).current = 0
        end,
    },
}
