-- The panel that the screens opened from the world or a conversation share, such as barter, looting and the merchant
-- services. It covers the right side of the screen, so the merchant or the chest stays in view on the left, as in the
-- fork's barter and loot panels. It has a title and a status line at the top, a list of fat entries on the left, and
-- the highlighted entry's details beside it.

local ui = require('openmw.ui')

local style = require('scripts.modernmorrow.ui.style')
local text = require('scripts.modernmorrow.ui.text')
local layer = require('scripts.modernmorrow.ui.layer')
local info = require('scripts.modernmorrow.items.info')

local panel = {}

local color = style.color
local size = style.textSize

-- The panel's place on the layer, which is 720 units tall: from 40% of the width to a margin from the right edge.
local LEFT = 0.40
local TOP = 40
local BOTTOM = 680
local MARGIN = 40

panel.ENTRY_HEIGHT = 52
panel.HEADER_HEIGHT = 28
panel.BACK = style.color.modal

-- Where the parts of the panel go, for a layer of the given size. `contentTop` is below the title, the status line and
-- the tabs or warning, if the panel has them.
function panel.geometry(layerSize)
    local x = math.floor(layerSize.x * LEFT)
    local w = layerSize.x - MARGIN - x
    local contentTop = TOP + 82
    local listW = math.floor(w * 0.54)
    return {
        x = x,
        y = TOP,
        w = w,
        bottom = BOTTOM,
        contentTop = contentTop,
        listX = x + 8,
        listW = listW - 8,
        detailsX = x + listW + 8,
        detailsW = w - listW - 8,
        -- The details column starts a little higher than the list, so the item's icon lines up with the first entry.
        detailsY = contentTop - 10,
    }
end

-- Draws the panel's background, the title at the top left and the status line at the top right. Returns the panel's
-- geometry. `title` and `status` are escaped.
function panel.drawFrame(canvas, title, status)
    local g = panel.geometry(layer.size(layer.menu))
    canvas:rect(g.x, g.y, g.w, g.bottom - g.y, panel.BACK)
    canvas:rect(g.x, g.y, g.w, 1, color.accent, 0.6)
    canvas:text(g.x + 20, g.y + 10, g.w - 40, 34, text.fit(title, g.w * 0.5, size.title), size.title, color.bright)
    if status and status ~= '' then
        canvas:text(g.x + 20, g.y + 14, g.w - 40, 30, status, size.small, color.dim, ui.ALIGNMENT.End)
    end
    -- A thin rule above the list, and a faint line between the list and the details.
    canvas:rect(g.x + 20, g.contentTop - 6, g.w - 40, 1, color.accent, 0.2)
    canvas:rect(g.detailsX - 4, g.contentTop, 1, g.bottom - 16 - g.contentTop, color.accent, 0.15)
    return g
end

-- Draws a row of tabs below the title, which the shoulder buttons switch. `captions` are escaped.
function panel.drawTabs(canvas, g, captions, selected, textSize)
    textSize = textSize or size.normal
    local x = g.x + 20
    canvas:glyph(x, g.y + 47, 22, 'lb')
    x = x + 30
    for i, caption in ipairs(captions) do
        local isSelected = i == selected
        local labelW = text.width(caption, textSize) + 20
        canvas:text(x, g.y + 42, labelW, 30, caption, textSize, isSelected and color.bright or color.dim,
            ui.ALIGNMENT.Center)
        if isSelected then
            canvas:rect(x + 6, g.y + 68, labelW - 12, 2, color.accent)
        end
        x = x + labelW + 4
    end
    canvas:glyph(x + 2, g.y + 47, 22, 'rb')
end

-- Draws a line below the title, such as the warning that taking from a container is stealing. `caption` is escaped.
function panel.drawSubtitle(canvas, g, caption, textColor)
    canvas:text(g.x + 20, g.y + 44, g.w - 40, 24, caption, size.small, textColor or color.worse)
end

-- Rows for a list of entries, grouped by the kind of item with the most valuable first in each group. Each entry
-- has an `item`. Group headers only show when there is more than one group, and gold never gets one.
function panel.groupedRows(entries)
    local keys = {}
    for _, entry in ipairs(entries) do
        keys[entry] = { group = info.group(entry.item), value = info.value(entry.item), name = info.name(entry.item) }
    end
    local sorted = {}
    for i, entry in ipairs(entries) do
        sorted[i] = entry
    end
    table.sort(sorted, function(a, b)
        local ka, kb = keys[a], keys[b]
        if ka.group ~= kb.group then
            return ka.group < kb.group
        end
        if ka.value ~= kb.value then
            return ka.value > kb.value
        end
        return ka.name < kb.name
    end)
    local groups = {}
    local groupCount = 0
    for _, entry in ipairs(sorted) do
        local group = keys[entry].group
        if group ~= info.GROUP.Gold and not groups[group] then
            groups[group] = true
            groupCount = groupCount + 1
        end
    end
    local rows = {}
    local current = nil
    for _, entry in ipairs(sorted) do
        local group = keys[entry].group
        if group ~= current then
            current = group
            if groupCount > 1 and group ~= info.GROUP.Gold then
                rows[#rows + 1] = { header = text.escape(info.groupName(group)) }
            end
        end
        rows[#rows + 1] = { entry = entry }
    end
    return rows
end

-- Draws a list of rows (see ui/list.lua) in the list column. `drawEntry(canvas, x, y, w, h, row, focused)` draws an
-- entry. Without entries, the list shows `empty`, an escaped caption.
function panel.drawList(canvas, g, rows, listState, drawEntry, empty)
    local x, y, w = g.listX, g.contentTop, g.listW
    local bottom = g.bottom - 8
    if listState:itemCount(rows) == 0 then
        canvas:text(x + 12, y + 4, w - 24, 30, empty, size.normal, color.dim)
        return
    end
    local placed = listState:place(rows, y, bottom, function(row)
        return row.header and panel.HEADER_HEIGHT or panel.ENTRY_HEIGHT
    end)
    for _, place in ipairs(placed) do
        local row = place.row
        if row.header then
            canvas:text(x + 8, place.y, w - 16, 24, row.header, size.small, color.bright)
            canvas:rect(x + 8, place.y + 23, w - 16, 1, color.accent, 0.25)
        else
            local h = panel.ENTRY_HEIGHT - 2
            if row.tint then
                canvas:rect(x, place.y, w, h, color.new, 0.08)
            end
            if place.focused then
                canvas:rect(x, place.y, w, h, color.accent, 0.16)
                canvas:rect(x, place.y, 3, h, color.accent)
            end
            drawEntry(canvas, x, place.y, w, h, row, place.focused)
        end
    end
end

-- Draws a fat entry: a picture, the name, a second line, and two short texts at the right. `options` has `name` and
-- `second` (escaped captions), and optionally `item` or `icon` (a texture path) for the picture, `equipped`,
-- `topRight` and `bottomRight` (escaped captions) with `topRightColor` and `bottomRightColor`, `rightW`, and `dim`
-- for an entry the player can't pick.
function panel.drawEntry(canvas, x, y, w, h, focused, options)
    local iconSize = h - 12
    local textX = x + 14
    if options.item then
        canvas:icon(x + 8, y + 6, iconSize, options.item, options.equipped)
        textX = x + 8 + iconSize + 10
    elseif options.icon then
        canvas:picture(x + 8, y + 6, iconSize, options.icon)
        textX = x + 8 + iconSize + 10
    end
    local rightW = options.rightW or 110
    local textW = x + w - textX - rightW - 4
    local nameColor = focused and color.active or (options.dim and color.dim or color.bright)
    canvas:text(textX, y + 3, textW, 26, text.fit(options.name, textW, size.normal), size.normal, nameColor)
    if options.second and options.second ~= '' then
        canvas:text(textX, y + 27, textW, 20, options.second, size.small, color.dim)
    end
    if options.topRight then
        canvas:text(x + w - rightW - 8, y + 3, rightW, 26, options.topRight, size.normal,
            options.topRightColor or color.text, ui.ALIGNMENT.End)
    end
    if options.bottomRight then
        canvas:text(x + w - rightW - 8, y + 27, rightW, 20, options.bottomRight, size.small,
            options.bottomRightColor or color.dim, ui.ALIGNMENT.End)
    end
end

-- The name of an item for a list entry, with the count when there is more than one. Plain text.
function panel.itemName(item, count)
    count = count or item.count
    local name = info.name(item)
    if count > 1 then
        name = name .. ' (' .. count .. ')'
    end
    return name
end

-- A label and a value on one line of the details column, such as the price. Both are escaped. Returns the y below it.
function panel.fact(canvas, x, y, w, label, value, valueColor)
    canvas:text(x + 16, y, w - 32, 28, label, size.normal, color.text)
    canvas:text(x + 16, y, w - 32, 28, value, size.normal, valueColor or color.bright, ui.ALIGNMENT.End)
    return y + 30
end

return panel
