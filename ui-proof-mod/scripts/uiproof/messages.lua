-- Draws the interactive message boxes, the messages with buttons, such as a game script's MessageBox.

local ui = require('openmw.ui')
local util = require('openmw.util')
local I = require('openmw.interfaces')

local style = require('scripts.uiproof.style')

local messages = {}

local WIDTH = 600
local BUTTON_HEIGHT = 34

local element = nil
local current = nil

-- Lua can't measure text yet, so the height of the message is a guess from its length.
local function guessLines(text, width, textSize)
    local charsPerLine = math.max(1, math.floor(width / (textSize * 0.5)))
    local lines = 0
    for paragraph in (text .. '\n'):gmatch('(.-)\n') do
        lines = lines + math.max(1, math.ceil(#paragraph / charsPerLine))
    end
    return lines
end

local function buttonLayout(label, isSelected, width)
    local children = {}
    children[#children + 1] = style.fill(isSelected and style.selection or style.frame, isSelected and 1 or 0.35)
    children[#children + 1] = style.text(label, {
        relativePosition = util.vector2(0.5, 0.5),
        anchor = util.vector2(0.5, 0.5),
        textColor = isSelected and style.over or style.normal,
    })
    return {
        type = ui.TYPE.Widget,
        props = { size = util.vector2(width, BUTTON_HEIGHT) },
        content = ui.content(children),
    }
end

local function layout()
    local textWidth = WIDTH - 40
    local textHeight = guessLines(current.message, textWidth, style.textSize) * (style.textSize + 4)
    local buttons = {}
    local vertical = #current.buttons > 3
    for i, label in ipairs(current.buttons) do
        if i > 1 then
            buttons[#buttons + 1] = {
                props = { size = vertical and util.vector2(0, 6) or util.vector2(10, 0) },
            }
        end
        buttons[#buttons + 1] = buttonLayout(label, i == current.selected, vertical and textWidth or 150)
    end
    local buttonsHeight = vertical and (#current.buttons * (BUTTON_HEIGHT + 6)) or BUTTON_HEIGHT
    local height = 20 + textHeight + 20 + buttonsHeight + 20
    return {
        layer = style.menuLayer,
        type = ui.TYPE.Widget,
        props = {
            relativePosition = util.vector2(0.5, 0.45),
            anchor = util.vector2(0.5, 0.5),
            size = util.vector2(WIDTH, height),
        },
        content = ui.content({
            style.panel({ relativeSize = util.vector2(1, 1) }, {
                style.text(style.escape(current.message), {
                    position = util.vector2(20, 20),
                    size = util.vector2(textWidth, textHeight),
                    autoSize = false,
                    wordWrap = true,
                    multiline = true,
                    textAlignH = ui.ALIGNMENT.Center,
                }),
                {
                    type = ui.TYPE.Flex,
                    props = {
                        horizontal = not vertical,
                        position = util.vector2(WIDTH / 2, 20 + textHeight + 20),
                        anchor = util.vector2(0.5, 0),
                    },
                    content = ui.content(buttons),
                },
            }),
        }),
    }
end

local function redraw()
    if element then
        element.layout = layout()
        element:update()
    else
        element = ui.create(layout())
    end
end

local function close()
    current = nil
    if element then
        element:destroy()
        element = nil
    end
end

function messages.register()
    I.UI.registerInteractiveMessageHandler(function(message, buttons, answer)
        current = { message = message, buttons = buttons, answer = answer, selected = 1 }
        redraw()
    end)
end

function messages.isActive()
    return current ~= nil
end

function messages.onAction(action)
    local vertical = #current.buttons > 3
    local back, forward = 'left', 'right'
    if vertical then
        back, forward = 'up', 'down'
    end
    if action == back and current.selected > 1 then
        current.selected = current.selected - 1
        redraw()
    elseif action == forward and current.selected < #current.buttons then
        current.selected = current.selected + 1
        redraw()
    elseif action == 'accept' then
        local answer, selected = current.answer, current.selected
        close()
        answer(selected)
    end
end

return messages
