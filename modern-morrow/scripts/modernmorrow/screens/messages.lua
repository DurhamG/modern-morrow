-- Shows the game's messages in the mod's style.
--
-- The message boxes with buttons, such as a game script's MessageBox with buttons, the soul gem's question whether to
-- recharge or enchant, the companion's warning and the question after the player's death, are the question dialog
-- (see ui/question.lua). As in the fork, B picks the answer of a message that has only one.
--
-- The short messages without buttons, such as "Your journal has been updated", are the fork's: boxes in large text,
-- centred above the bottom of the screen and fitted to their text. Up to three stack up, the oldest at the bottom, and
-- each fades out at the end of its time. A message shows for as long as the engine shows it, which grows with its
-- length.

local core = require('openmw.core')
local ui = require('openmw.ui')
local util = require('openmw.util')
local I = require('openmw.interfaces')

local style = require('scripts.modernmorrow.ui.style')
local layer = require('scripts.modernmorrow.ui.layer')
local draw = require('scripts.modernmorrow.ui.draw')
local text = require('scripts.modernmorrow.ui.text')
local question = require('scripts.modernmorrow.ui.question')

local messages = {}

local color = style.color
local textSize = style.textSize.large

-- The fork's sizes for the message boxes.
local MAX_WIDTH = 640
local PADDING = 14
local LINE_HEIGHT = 30
local BOTTOM_MARGIN = 120
local GAP = 8
local MAX_LINES = 4
local MAX_MESSAGES = 3
local FADE_TIME = 0.4

-- The messages on screen, oldest first, each with its `text` and the real time when it goes.
local shown = {}
-- Messages that arrived since the last frame. They are drawn in onFrame, because a handler may run on the Lua worker
-- thread, where text can't be measured.
local arrived = {}
local element = nil
local changed = false

local function timePerChar()
    return core.getGMST('fMessageTimePerChar') or 0.1
end

local function layout(now)
    local size = layer.size(layer.messages)
    local canvas = draw.canvas()
    local bottom = size.y - BOTTOM_MARGIN
    -- The oldest message is at the bottom, and the newest at the top.
    for _, message in ipairs(shown) do
        local lines = text.wrap(message.text, MAX_WIDTH - 2 * PADDING, textSize, MAX_LINES)
        local textW = 0
        for _, line in ipairs(lines) do
            textW = math.max(textW, text.width(line, textSize))
        end
        local w = math.min(MAX_WIDTH, textW + 2 * PADDING)
        local h = #lines * LINE_HEIGHT + 2 * PADDING - 6
        local box = draw.canvas()
        box:rect(0, 0, w, h, color.panel, 0.88)
        box:rect(0, 0, w, 1, color.accent, 0.4)
        for i, line in ipairs(lines) do
            box:text(PADDING, PADDING - 3 + (i - 1) * LINE_HEIGHT, w - 2 * PADDING, LINE_HEIGHT, line, textSize,
                color.bright, ui.ALIGNMENT.Center)
        end
        local alpha = math.max(0, math.min(1, (message.endsAt - now) / FADE_TIME))
        canvas:add(box:layout({
            position = util.vector2(math.floor((size.x - w) / 2), bottom - h),
            size = util.vector2(w, h),
            alpha = alpha,
        }))
        bottom = bottom - h - GAP
    end
    local root = canvas:layout({ size = size })
    root.layer = layer.messages
    return root
end

-- From the player script's onFrame: adds the messages that arrived, drops those whose time is up, and fades the rest.
function messages.onFrame()
    local now = core.getRealTime()
    for _, message in ipairs(arrived) do
        shown[#shown + 1] = { text = message, endsAt = now + (utf8.len(message) or #message) * timePerChar() }
        changed = true
    end
    arrived = {}
    while #shown > MAX_MESSAGES do
        table.remove(shown, 1)
    end
    local fading = false
    for i = #shown, 1, -1 do
        local left = shown[i].endsAt - now
        if left <= 0 then
            table.remove(shown, i)
            changed = true
        elseif left < FADE_TIME then
            fading = true
        end
    end
    if not changed and not fading then
        return
    end
    changed = false
    if #shown == 0 then
        if element then
            element:destroy()
            element = nil
        end
        return
    end
    if element then
        element.layout = layout(now)
        element:update()
    else
        element = ui.create(layout(now))
    end
end

function messages.register()
    I.UI.registerInteractiveMessageHandler(function(message, buttons, answer)
        question.open({
            message = message,
            answers = buttons,
            cancel = #buttons == 1 and 1 or nil,
            onAnswer = answer,
        })
    end)
    I.UI.registerMessageHandler(function(message)
        arrived[#arrived + 1] = message
    end)
end

return messages
