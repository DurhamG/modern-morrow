-- The full screens that crafting, the level-up and the rest open, as in the fork's crafting panels: a black screen
-- with the title at the top left, a status line at the top right, and boxes below them. It also draws the lists of fat
-- entries in those boxes, and the panel with a label and a progress bar that resting and jail show.

local ui = require('openmw.ui')

local style = require('scripts.modernmorrow.ui.style')
local text = require('scripts.modernmorrow.ui.text')
local layer = require('scripts.modernmorrow.ui.layer')

local screen = {}

local color = style.color
local size = style.textSize

screen.MARGIN = style.space.margin
screen.TOP = 82
screen.HEADER_HEIGHT = 30

-- Draws the black screen, the title and the status line. Returns the geometry: the layer's width `w` and height `h`,
-- and the `top` and `bottom` of the area below the title. `title` and `status` are escaped.
function screen.drawFrame(canvas, title, status)
    local layerSize = layer.size(layer.menu)
    local w, h = layerSize.x, layerSize.y
    local margin = screen.MARGIN
    canvas:rect(0, 0, w, h, color.black)
    canvas:text(margin, 20, w - 2 * margin, 40, title, size.title, color.bright)
    if status and status ~= '' then
        canvas:text(margin, 20, w - 2 * margin, 40, status, size.small, color.dim, ui.ALIGNMENT.End)
    end
    canvas:rect(margin, 66, w - 2 * margin, 1, color.accent, 0.2)
    return { w = w, h = h, top = screen.TOP, bottom = h - style.space.contentBottomMargin }
end

-- Splits the area below the title into a left and a right box, the left one `fraction` of the width.
function screen.columns(g, fraction)
    local margin = screen.MARGIN
    local leftW = math.floor((g.w - 2 * margin - 16) * fraction)
    local rightX = margin + leftW + 16
    return { leftX = margin, leftW = leftW, rightX = rightX, rightW = g.w - margin - rightX }
end

-- A box's background, with a thin rule along its top when `ruled` is set.
function screen.box(canvas, x, y, w, h, ruled)
    canvas:rect(x, y, w, h, color.panel, 0.9)
    if ruled then
        canvas:rect(x, y, w, 1, color.accent, 0.3)
    end
end

-- Draws rows (see ui/list.lua) between `y` and `bottom`, scrolled to keep the selection in view. Headers are
-- `{ header = caption }` with an escaped caption. `drawEntry(canvas, x, y, w, h, row, focused)` draws an entry, which
-- is `entryHeight` tall. The selection only shows when `focused` isn't false, so a list can keep its place while the
-- other side of the screen has the input.
function screen.drawRows(canvas, x, y, w, bottom, rows, listState, entryHeight, drawEntry, focused)
    local placed = listState:place(rows, y, bottom, function(row)
        return row.header and screen.HEADER_HEIGHT or entryHeight
    end)
    for _, place in ipairs(placed) do
        local row = place.row
        if row.header then
            canvas:text(x + 8, place.y + 4, w - 16, 24, row.header, size.small, color.bright)
            canvas:rect(x + 8, place.y + 27, w - 16, 1, color.accent, 0.25)
        else
            local isFocused = place.focused and focused ~= false
            if isFocused then
                canvas:rect(x, place.y, w, entryHeight - 2, color.accent, 0.16)
                canvas:rect(x, place.y, 3, entryHeight - 2, color.accent)
            end
            drawEntry(canvas, x, place.y, w, entryHeight - 2, row, isFocused)
        end
    end
end

-- Draws escaped text wrapped to the width, from `y` down. Returns the y below it.
function screen.lines(canvas, x, y, w, s, textSize, textColor, maxLines, lineHeight)
    lineHeight = lineHeight or textSize + 6
    for _, line in ipairs(text.wrap(s, w, textSize, maxLines or 4)) do
        canvas:text(x, y, w, lineHeight, line, textSize, textColor)
        y = y + lineHeight
    end
    return y
end

-- The number of lines that screen.lines would draw.
function screen.lineCount(s, w, textSize, maxLines)
    return #text.wrap(s, w, textSize, maxLines or 4)
end

-- A thin bar that is filled from the left, such as an item's condition.
function screen.bar(canvas, x, y, w, h, value, most)
    local fraction = most > 0 and math.max(0, math.min(1, value / most)) or 0
    canvas:rect(x, y, w, h, color.black, 0.6)
    canvas:rect(x, y, math.max(2, math.floor(w * fraction)), h, color.accent, 0.8)
end

-- The panel with a label and a progress bar in the middle of the screen, such as while the player rests. `label` is
-- escaped, and `fraction` goes from 0 to 1.
function screen.drawProgress(canvas, label, fraction)
    local layerSize = layer.size(layer.menu)
    local h = 96
    local w = math.max(420, math.min(text.width(label, size.normal) + 48, layerSize.x - 2 * screen.MARGIN))
    local x = math.floor((layerSize.x - w) / 2)
    local y = math.floor((layerSize.y - h) / 2)
    canvas:rect(x, y, w, h, color.modal)
    canvas:rect(x, y, w, 1, color.accent, 0.6)
    canvas:text(x + 24, y + 14, w - 48, 30, label, size.normal, color.bright)
    canvas:rect(x + 24, y + 58, w - 48, 10, color.black, 0.6)
    local filled = math.floor((w - 48) * math.max(0, math.min(1, fraction)))
    if filled > 0 then
        canvas:rect(x + 24, y + 58, filled, 10, color.accent)
    end
end

return screen
