-- A list of rows that the d-pad moves through. Rows that are headers can't be selected.
-- The list draws all rows inside a parent of fixed height and moves them up to scroll,
-- relying on the parent to clip the rows outside it.

local ui = require('openmw.ui')
local util = require('openmw.util')

local style = require('scripts.uiproof.style')

local list = {}
list.__index = list

-- `rows` is how many rows fit on screen.
function list.new(rows, rowHeight)
    return setmetatable({ items = {}, selected = nil, top = 1, rows = rows, rowHeight = rowHeight or 28 }, list)
end

local function selectable(item)
    return item ~= nil and not item.header
end

function list:ensureVisible()
    if not self.selected then
        return
    end
    if self.selected < self.top then
        self.top = self.selected
        -- Keep a header that sits right above the first visible row on screen.
        if self.top > 1 and self.items[self.top - 1].header then
            self.top = self.top - 1
        end
    elseif self.selected > self.top + self.rows - 1 then
        self.top = self.selected - self.rows + 1
    end
    self.top = math.max(1, math.min(self.top, math.max(1, #self.items - self.rows + 1)))
end

-- Replaces the rows. The selection stays on the row with the same key, if there is one.
function list:setItems(items)
    local oldKey = self.selected and self.items[self.selected] and self.items[self.selected].key
    self.items = items
    self.selected = nil
    if oldKey ~= nil then
        for i, item in ipairs(items) do
            if item.key == oldKey and selectable(item) then
                self.selected = i
            end
        end
    end
    if not self.selected then
        for i, item in ipairs(items) do
            if selectable(item) then
                self.selected = i
                break
            end
        end
        self.top = 1
    end
    self:ensureVisible()
end

function list:move(delta)
    if not self.selected then
        return false
    end
    local i = self.selected + delta
    while i >= 1 and i <= #self.items do
        if selectable(self.items[i]) then
            self.selected = i
            self:ensureVisible()
            return true
        end
        i = i + delta
    end
    return false
end

function list:current()
    return self.selected and self.items[self.selected]
end

-- `columns` is a list of { width = number } for the values on the right of each row, from left to right.
function list:layout(width, columns)
    columns = columns or {}
    local rowHeight = self.rowHeight
    local rows = {}
    for i, item in ipairs(self.items) do
        local isSelected = i == self.selected
        local children = {}
        if isSelected then
            children[#children + 1] = style.fill(style.selection, 0.9)
        end
        local color = item.color or style.normal
        if item.header then
            color = style.header
        elseif isSelected then
            color = style.over
        end
        children[#children + 1] = style.text(item.text, {
            position = util.vector2(item.header and 4 or 14, rowHeight / 2),
            anchor = util.vector2(0, 0.5),
            textColor = color,
            textSize = item.header and style.smallTextSize or style.textSize,
        })
        local right = width - 10
        for c = #columns, 1, -1 do
            local value = item.columns and item.columns[c]
            if value then
                children[#children + 1] = style.text(value, {
                    position = util.vector2(right, rowHeight / 2),
                    anchor = util.vector2(1, 0.5),
                    textColor = item.header and style.header or (isSelected and style.over or style.normal),
                    textSize = item.header and style.smallTextSize or style.textSize,
                })
            end
            right = right - columns[c].width
        end
        rows[#rows + 1] = {
            type = ui.TYPE.Widget,
            props = {
                position = util.vector2(0, (i - 1) * rowHeight),
                size = util.vector2(width, rowHeight),
            },
            content = ui.content(children),
        }
    end
    local children = {
        {
            type = ui.TYPE.Widget,
            props = {
                position = util.vector2(0, -(self.top - 1) * rowHeight),
                size = util.vector2(width, math.max(1, #self.items) * rowHeight),
            },
            content = ui.content(rows),
        },
    }
    -- Small marks at the right edge when there are more rows above or below.
    if self.top > 1 then
        children[#children + 1] = style.fill(style.frame, 1, {
            relativeSize = util.vector2(0, 0),
            position = util.vector2(width - 4, 0),
            size = util.vector2(4, rowHeight),
        })
    end
    if self.top + self.rows - 1 < #self.items then
        children[#children + 1] = style.fill(style.frame, 1, {
            relativeSize = util.vector2(0, 0),
            position = util.vector2(width - 4, (self.rows - 1) * rowHeight),
            size = util.vector2(4, rowHeight),
        })
    end
    return {
        type = ui.TYPE.Widget,
        props = { size = util.vector2(width, self.rows * rowHeight) },
        content = ui.content(children),
    }
end

return list
