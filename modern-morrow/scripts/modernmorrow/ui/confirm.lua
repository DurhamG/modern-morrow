-- Asks the player a yes or no question, such as whether to delete a spell. It is a panel in the middle of the screen,
-- over a darkened view of the screen behind it. Left and right pick an answer, A gives it, and B answers no.

local core = require('openmw.core')
local ui = require('openmw.ui')
local ambient = require('openmw.ambient')

local draw = require('scripts.modernmorrow.ui.draw')
local layer = require('scripts.modernmorrow.ui.layer')
local style = require('scripts.modernmorrow.ui.style')
local text = require('scripts.modernmorrow.ui.text')
local router = require('scripts.modernmorrow.input.router')

local confirm = {}

local WIDTH = 520
local BUTTON_WIDTH = 140
local BUTTON_HEIGHT = 40
local LINE_HEIGHT = 26

local element = nil
local current = nil

local function gmst(name, fallback)
    return text.escape(core.getGMST(name) or fallback)
end

local function layout()
    local size = layer.size(layer.modal)
    local canvas = draw.canvas()
    local color = style.color
    local textSize = style.textSize
    local innerW = WIDTH - 56
    local lines = text.wrap(current.question, innerW, textSize.large, 4)
    local height = 28 + #lines * LINE_HEIGHT + 28 + BUTTON_HEIGHT + 28

    canvas:rect(0, 0, size.x, size.y, color.black, 0.55)
    local x = math.floor((size.x - WIDTH) / 2)
    local y = math.floor((size.y - height) / 2)
    canvas:rect(x, y, WIDTH, height, color.modal)
    canvas:rect(x, y, WIDTH, 1, color.accent, 0.6)
    local ty = y + 28
    for _, line in ipairs(lines) do
        canvas:text(x + 28, ty, innerW, LINE_HEIGHT, line, textSize.large, color.bright, ui.ALIGNMENT.Center)
        ty = ty + LINE_HEIGHT
    end

    local answers = { current.yes and text.escape(current.yes) or gmst('sYes', 'Yes'),
        current.no and text.escape(current.no) or gmst('sNo', 'No') }
    local by = ty + 28
    local bx = x + math.floor((WIDTH - 2 * BUTTON_WIDTH - 24) / 2)
    for i, caption in ipairs(answers) do
        local focused = i == current.focus
        canvas:rect(bx, by, BUTTON_WIDTH, BUTTON_HEIGHT, color.accent, focused and 0.22 or 0.06)
        if focused then
            canvas:rect(bx, by + BUTTON_HEIGHT - 2, BUTTON_WIDTH, 2, color.accent)
        end
        canvas:text(bx, by, BUTTON_WIDTH, BUTTON_HEIGHT, caption, textSize.normal,
            focused and color.active or color.text, ui.ALIGNMENT.Center)
        bx = bx + BUTTON_WIDTH + 24
    end
    return canvas:layout({ size = size })
end

local function redraw()
    local root = layout()
    root.layer = layer.modal
    if element then
        element.layout = root
        element:update()
    else
        element = ui.create(root)
    end
end

local function close()
    router.remove(confirm.handler)
    if element then
        element:destroy()
        element = nil
    end
    current = nil
end

confirm.handler = {
    onButton = function(name)
        if name == 'a' then
            local asked = current
            close()
            ambient.playSound('menu click')
            if asked.focus == 1 then
                asked.onYes()
            end
        elseif name == 'b' then
            close()
        end
    end,
    onMove = function(dx, _)
        if dx ~= 0 then
            current.focus = dx < 0 and 1 or 2
            redraw()
        end
    end,
}

-- Opens the question. `options` has `question`, as plain text, and `onYes()`, which is called if the player answers
-- yes. `yes` and `no` can name the answers, as plain text, in place of the game's Yes and No. The selection starts on
-- yes.
function confirm.open(options)
    current = { question = options.question, onYes = options.onYes, yes = options.yes, no = options.no, focus = 1 }
    router.push(confirm.handler)
    redraw()
end

function confirm.isOpen()
    return current ~= nil
end

function confirm.close()
    if current then
        close()
    end
end

return confirm
