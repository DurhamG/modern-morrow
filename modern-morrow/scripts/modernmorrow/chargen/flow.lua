-- Moving between the character creation steps.
--
-- A step that the player is done with ends with types.Player.finishCharGenStep, which goes on as the engine's window
-- of the step would: the game's script opens the next step when the player reaches a step for the first time, and a
-- step that the player went back to goes on to the next step itself.
--
-- The engine's review window also marks the step that it sends the player to, so that the step goes back to the
-- review. The mod's review can't, so the mod remembers it, and takes the player back to the review itself.
--
-- Going back to an earlier step, or to the class question, the class list or the class form, swaps the mode.

local self = require('openmw.self')
local types = require('openmw.types')
local I = require('openmw.interfaces')

local flow = {}

-- Whether the step being done goes back to the review.
local toReview = false
-- The player's name when the game started, from the content files, before the player chose one.
local startName = nil

-- Ends the step of `mode`, after the step has made its choice.
function flow.done(mode)
    if toReview then
        toReview = false
        I.UI.setMode('ChargenClassReview')
    else
        types.Player.finishCharGenStep(self, mode)
    end
end

-- Goes to another step, or to another part of the class step, such as back from the race to the name.
function flow.go(mode)
    I.UI.setMode(mode)
end

-- Goes from the review to a step, which comes back to the review when it is done.
function flow.fromReview(mode)
    toReview = true
    I.UI.setMode(mode)
end

-- Whether the player has chosen a name, so that the name step starts from it, as the engine's window does. The engine
-- knows which steps the player has done, but Lua can't ask, so this compares the name with the one the game started
-- with.
function flow.hasName()
    local name = types.NPC.record(self).name
    return name ~= '' and name ~= startName
end

-- From the player script's onInit, when a new game starts, and onLoad.
function flow.load(data)
    toReview = false
    if data then
        startName = data.startName
    else
        startName = types.NPC.record(self).name
    end
end

function flow.save()
    return { startName = startName }
end

return flow
