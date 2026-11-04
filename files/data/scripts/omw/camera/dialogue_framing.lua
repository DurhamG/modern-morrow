-- Turns the camera to the NPC's face during a conversation in controller menus mode, so the face sits in the
-- space above the dialogue panel, then turns it back once the conversation ends. This is only done in first person.
-- The third person camera sits behind and above the player, so it would look steeply down at the NPC, past the
-- player's own head. The turn is an extra pitch and yaw on top of where the player looks, so the player's own view
-- direction never changes.

local camera = require('openmw.camera')
local core = require('openmw.core')
local input = require('openmw.input')
local ui = require('openmw.ui')
local util = require('openmw.util')

local M = {}

-- How high on the screen the face goes, as a fraction of the screen height from the top. The dialogue panel covers
-- the screen from 56% of its height down, so this is the middle of the space above it.
local faceHeight = 0.28
-- How fast the camera turns, as the fraction of the remaining turn made per second.
local turnRate = 6
-- Where the face goes across the screen during barter and the other services, as a fraction of the screen width from
-- the left. Their panels cover the screen from 40% of its width rightward, so this is the middle of the space beside
-- them.
local panelFaceLeft = 0.2

-- The services that show a panel on the right side of the screen.
local panelModes = {
    Barter = true, SpellBuying = true, Travel = true, Training = true, MerchantRepair = true, Companion = true,
}

local speaker = nil
local besidePanel = false
local yaw = 0
local pitch = 0

local function faceTarget()
    if not speaker or not speaker:isValid() or not speaker.enabled then
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
    if besidePanel then
        -- Looking to the right of the face puts it in the space left of the panel.
        local size = ui.screenSize()
        local halfWidth = math.atan(math.tan(camera.getFieldOfView() / 2) * size.x / size.y)
        targetYaw = targetYaw + math.atan((1 - 2 * panelFaceLeft) * math.tan(halfWidth))
    end
    -- Looking further down than the face puts it above the middle of the screen.
    local belowFace = math.atan((1 - 2 * faceHeight) * math.tan(camera.getFieldOfView() / 2))
    local targetPitch = -math.atan2(toFace.z, flat) + belowFace
    return util.normalizeAngle(targetYaw - camera.getYaw()), targetPitch - camera.getPitch()
end

function M.onUiModeChanged(data)
    besidePanel = panelModes[data.newMode] == true
    if data.newMode == 'Dialogue' and data.arg and input._isControllerMenusEnabled()
        and camera.getMode() == camera.MODE.FirstPerson then
        speaker = data.arg
    elseif data.newMode ~= 'Dialogue' and data.newMode ~= 'Barter' and data.newMode ~= 'SpellBuying'
        and data.newMode ~= 'Travel' and data.newMode ~= 'Training' and data.newMode ~= 'MerchantRepair'
        and data.newMode ~= 'Enchanting' and data.newMode ~= 'SpellCreation' and data.newMode ~= 'Companion' then
        -- Services opened from the conversation keep the NPC framed. Anything else ends the conversation.
        speaker = nil
    end
end

-- Eases the turn toward the NPC's face, or back to nothing after the conversation, and applies it. Call once a frame.
function M.update()
    local targetYaw, targetPitch = 0, 0
    if speaker then
        targetYaw, targetPitch = faceTarget()
    end
    -- The game is paused during conversations, so the turn uses real time.
    local step = math.min(1, turnRate * core.getRealFrameDuration())
    yaw = yaw + (targetYaw - yaw) * step
    pitch = pitch + (targetPitch - pitch) * step
    if not speaker and math.abs(yaw) < 0.001 and math.abs(pitch) < 0.001 then
        yaw, pitch = 0, 0
    end
    M.apply()
end

-- Applies the turn again, after something reset the camera's extra pitch and yaw.
function M.apply()
    camera.setExtraYaw(yaw)
    camera.setExtraPitch(pitch)
end

function M.active()
    return speaker ~= nil or yaw ~= 0 or pitch ~= 0
end

return M
