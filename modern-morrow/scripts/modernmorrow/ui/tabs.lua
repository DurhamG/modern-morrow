-- A screen's own row of tabs, which the shoulder buttons switch, with a status line at its right.
-- It sits below the top bar, between y = 50 and the rule at y = 94.

local ui = require('openmw.ui')

local style = require('scripts.modernmorrow.ui.style')
local text = require('scripts.modernmorrow.ui.text')

local tabs = {}

-- `captions` are escaped tab names, `selected` the index of the current tab, and `status` an escaped line of text
-- for the right side, such as the gold the player has.
function tabs.draw(canvas, width, captions, selected, status)
    local size = style.textSize
    local color = style.color
    local x = style.space.margin
    canvas:glyph(x, 59, 22, 'lb')
    x = x + 34
    for i, caption in ipairs(captions) do
        local isSelected = i == selected
        local labelWidth = text.width(caption, size.normal) + 24
        canvas:text(x, 50, labelWidth, 40, caption, size.normal, isSelected and color.bright or color.dim,
            ui.ALIGNMENT.Center)
        if isSelected then
            canvas:rect(x + 8, 84, labelWidth - 16, 2, color.accent)
        end
        x = x + labelWidth + 6
    end
    canvas:glyph(x + 4, 59, 22, 'rb')

    if status and status ~= '' then
        canvas:text(width - style.space.margin - 420, 50, 420, 40, status, size.small, color.dim, ui.ALIGNMENT.End)
    end
    canvas:rect(0, 94, width, 1, color.accent, 0.2)
end

return tabs
