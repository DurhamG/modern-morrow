-- Turns the camera to the NPC's face during a conversation, so the face sits in the space above the dialogue panel, and
-- turns it back once the conversation ends, as the fork does. The services opened from the conversation keep the NPC
-- in view: those with a panel on the right side of the screen, such as barter, put the face in the space left of the
-- panel.
--
-- This is only done in first person. The third person camera sits behind and above the player, so it would look
-- steeply down at the NPC, past the player's own head. The turn is an extra pitch and yaw on top of where the player
-- looks, so the player's own view direction never changes. The built-in camera script sets the extra pitch and yaw
-- back to zero in each update while the game runs, so the turn is applied again after it.

local camera = require('openmw.camera')
local core = require('openmw.core')
local self = require('openmw.self')
local types = require('openmw.types')
local ui = require('openmw.ui')
local util = require('openmw.util')
local I = require('openmw.interfaces')

local framing = {}

-- How high on the screen the face goes, as a fraction of the screen height from the top. The dialogue panel covers
-- the screen from 56% of its height down, so this is the middle of the space above it.
local FACE_HEIGHT = 0.28
-- How fast the camera turns, as the fraction of the remaining turn made per second.
local TURN_RATE = 6
-- Where the face goes across the screen beside a panel, as a fraction of the screen width from the left. The panels
-- cover the screen from 40% of its width rightward, so this is the middle of the space beside them.
local PANEL_FACE_LEFT = 0.2

-- The modes of the services that show a panel on the right side of the screen.
local PANEL_MODES = {
    Barter = true,
    SpellBuying = true,
    Travel = true,
    Training = true,
    MerchantRepair = true,
    Companion = true,
}

local speaker = nil
local besidePanel = false
local yaw = 0
local pitch = 0

local function inConversation()
    for _, mode in ipairs(I.UI.modes) do
        if mode == 'Dialogue' then
            return true
        end
    end
    return false
end

local function faceTarget()
    if not speaker:isValid() or not speaker.enabled then
        return 0, 0
    end
    -- The face is near the top of the NPC's bounding box.
    local box = speaker:getBoundingBox()
    local face = box.center + util.vector3(0, 0, box.halfSize.z * 0.75)
    local toFace = face - camera.getPosition()
    local flat = math.sqrt(toFace.x * toFace.x + toFace.y * toFace.y)
    if flat < 1 then
        return 0, 0
    end
    local targetYaw = math.atan2(toFace.x, toFace.y)
    local halfHeight = camera.getFieldOfView() / 2
    if besidePanel then
        -- Looking to the right of the face puts it in the space left of the panel.
        local screen = ui.screenSize()
        local halfWidth = math.atan(math.tan(halfHeight) * screen.x / screen.y)
        targetYaw = targetYaw + math.atan((1 - 2 * PANEL_FACE_LEFT) * math.tan(halfWidth))
    end
    -- Looking further down than the face puts it above the middle of the screen.
    local belowFace = math.atan((1 - 2 * FACE_HEIGHT) * math.tan(halfHeight))
    local targetPitch = -math.atan2(toFace.z, flat) + belowFace
    return util.normalizeAngle(targetYaw - camera.getYaw()), targetPitch - camera.getPitch()
end

local function apply()
    camera.setExtraYaw(yaw)
    camera.setExtraPitch(pitch)
end

-- Eases the turn toward the NPC's face, or back to nothing after the conversation. From the player script's onFrame.
function framing.onFrame()
    local actor = inConversation() and types.Player.getDialogueActor(self) or nil
    if actor == nil then
        speaker = nil
    elseif speaker ~= actor and camera.getMode() == camera.MODE.FirstPerson then
        speaker = actor
    end
    besidePanel = PANEL_MODES[I.UI.getMode()] == true
    if speaker == nil and yaw == 0 and pitch == 0 then
        return
    end

    local targetYaw, targetPitch = 0, 0
    if speaker then
        targetYaw, targetPitch = faceTarget()
    end
    -- The game is paused during conversations, so the turn uses real time.
    local step = math.min(1, TURN_RATE * core.getRealFrameDuration())
    yaw = yaw + (targetYaw - yaw) * step
    pitch = pitch + (targetPitch - pitch) * step
    if speaker == nil and math.abs(yaw) < 0.001 and math.abs(pitch) < 0.001 then
        yaw, pitch = 0, 0
    end
    apply()
end

-- From the player script's onUpdate, which runs after the built-in camera script set the extra turn back to zero.
function framing.onUpdate()
    if speaker ~= nil or yaw ~= 0 or pitch ~= 0 then
        apply()
    end
end

return framing
