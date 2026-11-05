-- Asks how many of a stack to use, such as how many arrows to drop. It is a panel in the middle of the screen, over a
-- darkened view of the screen behind it. Left and right change the count and repeat while held, LB picks one, RB
-- picks all, A confirms and B cancels.

local core = require('openmw.core')
local ui = require('openmw.ui')
local util = require('openmw.util')
local ambient = require('openmw.ambient')

local draw = require('scripts.modernmorrow.ui.draw')
local hints = require('scripts.modernmorrow.ui.hints')
local layer = require('scripts.modernmorrow.ui.layer')
local style = require('scripts.modernmorrow.ui.style')
local text = require('scripts.modernmorrow.ui.text')
local router = require('scripts.modernmorrow.input.router')

local l10n = core.l10n('ModernMorrow')
local v2 = util.vector2

local countPicker = {}

local WIDTH = 520
local HEIGHT = 250

local element = nil
local current = nil

local function layout()
    local size = layer.size(layer.modal)
    local canvas = draw.canvas()
    local color = style.color
    local textSize = style.textSize
    canvas:rect(0, 0, size.x, size.y, color.black, 0.55)
    local x = math.floor((size.x - WIDTH) / 2)
    local y = math.floor((size.y - HEIGHT) / 2)
    canvas:rect(x, y, WIDTH, HEIGHT, color.modal)
    canvas:rect(x, y, WIDTH, 1, color.accent, 0.6)
    local innerX = x + 28
    local innerW = WIDTH - 56

    -- What happens to the item, such as "Drop", and which item.
    canvas:text(innerX, y + 16, innerW, 34, text.escape(current.title), textSize.title, color.bright)
    canvas:text(innerX, y + 52, innerW, 26, text.fit(current.item, innerW, textSize.normal), textSize.normal,
        color.text)

    -- The count, with arrows for the d-pad, and a bar for how much of the stack it is.
    local ty = y + 92
    canvas:glyph(innerX, ty + 10, 26, 'dpad')
    canvas:glyph(innerX + innerW - 26, ty + 10, 26, 'dpad')
    canvas:text(innerX, ty, innerW, 46, string.format('%d / %d', current.count, current.max), textSize.title,
        color.active, ui.ALIGNMENT.Center)
    canvas:rect(innerX, ty + 56, innerW, 8, color.black, 0.6)
    canvas:rect(innerX, ty + 56, math.floor(innerW * current.count / current.max), 8, color.accent)

    hints.draw(canvas, innerX, innerW, y + HEIGHT - 4, {
        { 'a', text.escape(l10n('OK')) },
        { 'lb', text.escape(l10n('CountOne')) },
        { 'rb', text.escape(l10n('CountAll')) },
    })
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
    router.remove(countPicker.handler)
    if element then
        element:destroy()
        element = nil
    end
    current = nil
end

local function setCount(count)
    current.count = math.max(1, math.min(current.max, count))
    redraw()
end

countPicker.handler = {
    onButton = function(name)
        if name == 'a' then
            local chosen = current
            close()
            ambient.playSound('menu click')
            chosen.onChosen(chosen.count)
        elseif name == 'b' then
            close()
        elseif name == 'lb' then
            setCount(1)
        elseif name == 'rb' then
            setCount(current.max)
        end
    end,
    onMove = function(dx, _)
        if dx ~= 0 then
            setCount(current.count + dx)
        end
    end,
}

-- Opens the picker. `options` has `title` (such as "Drop"), `item` (the item's name), `max`, and `onChosen(count)`.
-- The count starts at all of the stack, as the game's count dialog does.
function countPicker.open(options)
    current = {
        title = options.title,
        item = options.item,
        max = options.max,
        count = options.max,
        onChosen = options.onChosen,
    }
    router.push(countPicker.handler)
    redraw()
end

function countPicker.isOpen()
    return current ~= nil
end

function countPicker.close()
    if current then
        close()
    end
end

return countPicker
