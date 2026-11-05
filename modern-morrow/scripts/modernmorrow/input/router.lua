-- Sends controller input to whichever screen or dialog is on top.
--
-- A handler is a table with any of these functions:
--   onButton(name)       a button was pressed. Names: a, b, x, y, lb, rb, lt, rt, ls, rs, start, back.
--   onMove(dx, dy)       the d-pad or the left stick moved the selection. It repeats while held.
--   onStick(x, y, dt)    the right stick's position, every frame, with the real time since the last frame.
--
-- Input only reaches handlers while Lua has the menu input (input.isGuiInputCaptured). A press that arrived before
-- that was already used by the engine, such as the B that closed a built-in window, so it is ignored.

local core = require('openmw.core')
local input = require('openmw.input')

local style = require('scripts.modernmorrow.ui.style')

local router = {}

local BUTTON = input.CONTROLLER_BUTTON
local AXIS = input.CONTROLLER_AXIS

local BUTTON_NAMES = {
    [BUTTON.A] = 'a',
    [BUTTON.B] = 'b',
    [BUTTON.X] = 'x',
    [BUTTON.Y] = 'y',
    [BUTTON.LeftShoulder] = 'lb',
    [BUTTON.RightShoulder] = 'rb',
    [BUTTON.LeftStick] = 'ls',
    [BUTTON.RightStick] = 'rs',
    [BUTTON.Start] = 'start',
    [BUTTON.Back] = 'back',
}

-- A held direction repeats after this delay, then at this interval, in seconds.
local REPEAT_DELAY = 0.35
local REPEAT_INTERVAL = 0.08
-- How far the left stick must be pushed to move the selection.
local STICK_THRESHOLD = 0.5
-- How far a trigger must be pulled to count as a press.
local TRIGGER_THRESHOLD = 0.5

local stack = {}

-- The direction held last frame, and when it repeats next.
local heldX, heldY = 0, 0
local repeatAt = 0
local triggers = { lt = false, rt = false }
local lastFrame = nil

local function resetHeld()
    -- A direction that is already held when a handler takes over counts as a new press.
    heldX, heldY = 0, 0
end

-- Puts a handler on top. It gets the input until it is removed or another handler goes on top of it.
function router.push(handler)
    router.remove(handler)
    stack[#stack + 1] = handler
    resetHeld()
end

function router.remove(handler)
    for i = #stack, 1, -1 do
        if stack[i] == handler then
            table.remove(stack, i)
            resetHeld()
        end
    end
end

function router.top()
    return stack[#stack]
end

function router.clear()
    stack = {}
    resetHeld()
end

-- From the player script's onControllerButtonPress.
function router.onButtonPress(id)
    local name = BUTTON_NAMES[id]
    local handler = stack[#stack]
    if name == nil or handler == nil or not input.isGuiInputCaptured() then
        return
    end
    if handler.onButton then
        handler.onButton(name)
    end
end

local function direction()
    local stickX = input.getAxisValue(AXIS.LeftX)
    local stickY = input.getAxisValue(AXIS.LeftY)
    local dx, dy = 0, 0
    if input.isControllerButtonPressed(BUTTON.DPadLeft) or stickX < -STICK_THRESHOLD then
        dx = -1
    elseif input.isControllerButtonPressed(BUTTON.DPadRight) or stickX > STICK_THRESHOLD then
        dx = 1
    end
    if input.isControllerButtonPressed(BUTTON.DPadUp) or stickY < -STICK_THRESHOLD then
        dy = -1
    elseif input.isControllerButtonPressed(BUTTON.DPadDown) or stickY > STICK_THRESHOLD then
        dy = 1
    end
    return dx, dy
end

-- From the player script's onFrame. The d-pad and the sticks are read here rather than from press events, so that a
-- held direction repeats.
function router.onFrame()
    local now = core.getRealTime()
    local dt = lastFrame and math.min(now - lastFrame, 0.1) or 0
    lastFrame = now
    local handler = stack[#stack]
    if handler == nil or not input.isGuiInputCaptured() then
        resetHeld()
        return
    end

    local dx, dy = direction()
    local move = false
    if dx ~= heldX or dy ~= heldY then
        heldX, heldY = dx, dy
        repeatAt = now + REPEAT_DELAY
        move = dx ~= 0 or dy ~= 0
    elseif (dx ~= 0 or dy ~= 0) and now >= repeatAt then
        repeatAt = now + REPEAT_INTERVAL
        move = true
    end
    if move and handler.onMove then
        handler.onMove(dx, dy)
    end

    -- The triggers are axes, so a press is when one crosses the threshold.
    for name, axis in pairs({ lt = AXIS.TriggerLeft, rt = AXIS.TriggerRight }) do
        local pulled = input.getAxisValue(axis) > TRIGGER_THRESHOLD
        if pulled and not triggers[name] and handler == stack[#stack] and handler.onButton then
            handler.onButton(name)
        end
        triggers[name] = pulled
    end

    handler = stack[#stack]
    if handler and handler.onStick then
        local x = input.getAxisValue(AXIS.RightX)
        local y = input.getAxisValue(AXIS.RightY)
        if math.abs(x) < style.stickDeadzone then
            x = 0
        end
        if math.abs(y) < style.stickDeadzone then
            y = 0
        end
        handler.onStick(x, y, dt)
    end
end

return router
