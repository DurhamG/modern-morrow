-- Moves the player to stand in front of an NPC, facing them, so a dialogue mock-up has them in view. The mock-up
-- generator fills in NPC_ID before sending this to the game as a global script. The NPC must be near enough to be
-- loaded.
local world = require('openmw.world')
local util = require('openmw.util')

local NPC_ID = ''
local DISTANCE = 160

local player = world.players[1]
for _, actor in ipairs(world.activeActors) do
    if actor.recordId == NPC_ID then
        local facing = actor.rotation:apply(util.vector3(0, 1, 0))
        local target = actor.position + facing * DISTANCE
        local toNpc = actor.position - target
        player:teleport(actor.cell, target, util.transform.rotateZ(math.atan2(toNpc.x, toNpc.y)))
        print('facing ' .. NPC_ID)
        return
    end
end
print('not found ' .. NPC_ID)
