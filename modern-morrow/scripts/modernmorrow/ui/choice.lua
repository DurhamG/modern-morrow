-- The full screen choice from a list that character creation uses for the classes and the birth signs, as in the
-- fork's ChoiceScreen: a black screen with the title at the top, the choices on the left, a picture in the middle
-- above the highlighted choice's name and description, its facts and sections on the right, and the button hints
-- along the bottom.

local ui = require('openmw.ui')

local draw = require('scripts.modernmorrow.ui.draw')
local style = require('scripts.modernmorrow.ui.style')
local text = require('scripts.modernmorrow.ui.text')
local hints = require('scripts.modernmorrow.ui.hints')
local layer = require('scripts.modernmorrow.ui.layer')

local choice = {}

local color = style.color
local size = style.textSize

local MARGIN = style.space.margin
local TOP = 82
local ENTRY_HEIGHT = 40
local LIST_WIDTH = 230

-- Draws the screen into a new canvas and returns it. `screen` has:
--   title         the title, escaped
--   choices       the names of the choices, plain text
--   focus         the highlighted choice's number
--   scroll        a table whose `first` field keeps the first choice in view between draws
--   name          the highlighted choice's name, plain text
--   subtitle      a line under the name, plain text (optional)
--   description   plain text (optional)
--   picture       the path of the picture's texture (optional)
--   facts         a list of `{ label, value }`, escaped, shown first on the right (optional)
--   sections      a list of `{ heading, lines }`, shown on the right below the facts (optional). The heading is
--                 escaped. Each line is plain text, or a table with `text`, and the optional `color` and `indent`.
--   hints         the button hints, as for ui/hints.lua
function choice.draw(screen)
    local layerSize = layer.size(layer.menu)
    local w, h = layerSize.x, layerSize.y
    local canvas = draw.canvas()
    canvas:rect(0, 0, w, h, color.black)
    canvas:text(MARGIN, 20, w - 2 * MARGIN, 40, screen.title, size.title, color.bright)
    canvas:rect(MARGIN, 66, w - 2 * MARGIN, 1, color.accent, 0.2)
    local bottom = h - 64

    -- The choices on the left.
    local count = #screen.choices
    local visible = math.max(1, math.floor((bottom - TOP - 12) / ENTRY_HEIGHT))
    local focus = screen.focus
    local first = screen.scroll.first or 1
    first = math.max(first, focus - visible + 1)
    first = math.min(first, focus)
    first = math.max(1, first)
    screen.scroll.first = first
    canvas:rect(MARGIN, TOP, LIST_WIDTH, bottom - TOP, color.panel, 0.9)
    for i = first, math.min(count, first + visible - 1) do
        local ry = TOP + 6 + (i - first) * ENTRY_HEIGHT
        local focused = i == focus
        if focused then
            canvas:rect(MARGIN + 4, ry, LIST_WIDTH - 8, ENTRY_HEIGHT - 4, color.accent, 0.16)
            canvas:rect(MARGIN + 4, ry, 3, ENTRY_HEIGHT - 4, color.accent)
        end
        canvas:text(MARGIN + 18, ry, LIST_WIDTH - 30, ENTRY_HEIGHT - 4,
            text.fit(screen.choices[i], LIST_WIDTH - 30, size.normal), size.normal,
            focused and color.active or color.bright)
    end

    -- The picture in the middle, above the name and the description.
    local px = MARGIN + LIST_WIDTH + 20
    local pictureW = math.min(380, math.floor((w - px - MARGIN) / 2))
    local pictureH = math.floor(pictureW / 2)
    canvas:rect(px, TOP, pictureW, pictureH, color.black)
    if screen.picture then
        canvas:image(px, TOP, pictureW, pictureH, draw.texture(screen.picture))
    end
    local ty = TOP + pictureH + 16
    canvas:text(px, ty, pictureW, 34, text.fit(screen.name or '', pictureW, size.large), size.large, color.bright)
    ty = ty + 34
    if screen.subtitle and screen.subtitle ~= '' then
        canvas:text(px, ty, pictureW, 24, text.escape(screen.subtitle), size.small, color.dim)
        ty = ty + 30
    end
    for _, line in ipairs(text.wrap(screen.description or '', pictureW, size.small, 12)) do
        if ty + 22 > bottom then
            break
        end
        canvas:text(px, ty, pictureW, 22, line, size.small, color.text)
        ty = ty + 22
    end

    -- The facts and the sections on the right.
    local dx = px + pictureW + 28
    local dw = w - MARGIN - dx
    ty = TOP
    for _, fact in ipairs(screen.facts or {}) do
        canvas:text(dx, ty, dw, 28, fact[1], size.normal, color.text)
        canvas:text(dx, ty, dw, 28, fact[2], size.normal, color.bright, ui.ALIGNMENT.End)
        ty = ty + 30
    end
    for _, section in ipairs(screen.sections or {}) do
        ty = ty + 8
        if ty + 26 > bottom then
            break
        end
        canvas:text(dx, ty, dw, 24, section[1], size.small, color.dim)
        ty = ty + 26
        for _, line in ipairs(section[2]) do
            if ty + 26 > bottom then
                break
            end
            if type(line) == 'string' then
                line = { text = line }
            end
            local indent = line.indent or 0
            canvas:text(dx + indent, ty, dw - indent, 26, text.fit(line.text, dw - indent, size.normal), size.normal,
                line.color or color.text)
            ty = ty + 26
        end
    end

    hints.draw(canvas, MARGIN, w - 2 * MARGIN, h - 10, screen.hints or {})
    return canvas
end

return choice
