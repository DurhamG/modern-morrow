local input = require('openmw.input')
local ui = require('openmw.ui')
local I = require('openmw.interfaces')

local style = require('scripts.uiproof.style')
local magic = require('scripts.uiproof.magic')
local messages = require('scripts.uiproof.messages')
local hud = require('scripts.uiproof.hud')
local dialogue = require('scripts.uiproof.dialogue')

-- The layers stay when a game is loaded, so only the first game of a session creates them.
if not ui.layers.indexOf(style.hudLayer) then
    ui.layers.insertAfter('HUD', style.hudLayer, { interactive = false, height = style.layerHeight })
end
if not ui.layers.indexOf(style.menuLayer) then
    ui.layers.insertAfter('Windows', style.menuLayer, { interactive = true, height = style.layerHeight })
end

-- input.isGuiInputCaptured() tells, while a press is handled, whether the input was captured when
-- the press happened. A press that closed an engine window, such as B in the Training window, then
-- doesn't also act in the Lua window shown after it.
I.UI.registerWindow('Magic', magic.show, magic.hide, { capturesInput = true })
I.UI.registerWindow('Dialogue', dialogue.show, dialogue.hide, { capturesInput = true })
messages.register()

local BUTTON = input.CONTROLLER_BUTTON
local BUTTON_ACTIONS = {
    [BUTTON.DPadUp] = 'up',
    [BUTTON.DPadDown] = 'down',
    [BUTTON.DPadLeft] = 'left',
    [BUTTON.DPadRight] = 'right',
    [BUTTON.A] = 'accept',
    [BUTTON.B] = 'back',
    [BUTTON.X] = 'detail',
}

local KEY_ACTIONS = {
    [input.KEY.UpArrow] = 'up',
    [input.KEY.DownArrow] = 'down',
    [input.KEY.LeftArrow] = 'left',
    [input.KEY.RightArrow] = 'right',
    [input.KEY.Enter] = 'accept',
    [input.KEY.Escape] = 'back',
    [input.KEY.X] = 'detail',
}

local OPEN_MAGIC_KEY = input.KEY.K

local function openMagic()
    I.UI.setMode(I.UI.MODE.Interface, { windows = { 'Magic' } })
end

local function dispatch(action)
    if messages.isActive() then
        messages.onAction(action)
    elseif dialogue.isVisible() then
        dialogue.onAction(action)
    elseif magic.isVisible() then
        magic.onAction(action)
    end
end

return {
    engineHandlers = {
        onInit = hud.start,
        onLoad = hud.start,
        onFrame = function()
            hud.onFrame()
            magic.onFrame()
            dialogue.onFrame()
        end,
        onControllerButtonPress = function(id)
            local action = BUTTON_ACTIONS[id]
            if action and input.isGuiInputCaptured() then
                dispatch(action)
            end
        end,
        onKeyPress = function(key)
            if key.code == OPEN_MAGIC_KEY and I.UI.getMode() == nil then
                openMagic()
                return
            end
            local action = KEY_ACTIONS[key.code]
            if action and input.isGuiInputCaptured() then
                dispatch(action)
            end
        end,
    },
    eventHandlers = {
        UiProofOpenMagic = openMagic,
    },
}
