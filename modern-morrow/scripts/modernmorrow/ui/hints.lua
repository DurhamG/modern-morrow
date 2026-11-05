-- Button hints: a controller button's icon and what it does, in rows along the bottom of an area.
--
-- Screens list only the buttons whose use isn't obvious. B never gets a hint when it only goes back or closes, as in
-- other console games.

local style = require('scripts.modernmorrow.ui.style')
local text = require('scripts.modernmorrow.ui.text')

local hints = {}

local GLYPH_SIZE = 26
local LABEL_X = GLYPH_SIZE + 8
local GAP = 24
local ROW_HEIGHT = 40

-- Draws hints in rows along the bottom of an area, starting another row above when one is full. Each hint is
-- `{ button, label }`, with a router button name (see input/router.lua) and an escaped label. Returns the top of the
-- highest row, or the bottom when there are no hints.
function hints.draw(canvas, x, width, bottom, list)
    if #list == 0 then
        return bottom
    end
    local rows = { {} }
    local rowWidth = 0
    for _, hint in ipairs(list) do
        local hintWidth = LABEL_X + text.width(hint[2], style.textSize.normal)
        local row = rows[#rows]
        if #row > 0 and rowWidth + GAP + hintWidth > width then
            row = {}
            rows[#rows + 1] = row
            rowWidth = 0
        end
        rowWidth = rowWidth + (#row > 0 and GAP or 0) + hintWidth
        row[#row + 1] = { button = hint[1], label = hint[2], width = hintWidth }
    end
    local top = bottom - ROW_HEIGHT * #rows
    for r, row in ipairs(rows) do
        local y = top + (r - 1) * ROW_HEIGHT
        local hx = x
        for _, hint in ipairs(row) do
            canvas:glyph(hx, y, GLYPH_SIZE, hint.button)
            canvas:text(hx + LABEL_X, y - 4, hint.width - LABEL_X + 8, 34, hint.label, style.textSize.normal,
                style.color.bright)
            hx = hx + hint.width + GAP
        end
    end
    return top
end

return hints
