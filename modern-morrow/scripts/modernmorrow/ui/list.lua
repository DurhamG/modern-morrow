-- A scrolling list of rows, moved through with the d-pad. Rows are tables. A row with a `header` field is a group
-- header, and a row with a `note` field is a line of text, such as one that says a group is empty. The selection skips
-- both. The list draws only the rows that fit, from the first one scrolled into view, so whole rows appear and
-- disappear as it scrolls.

local List = {}
List.__index = List

local list = {}

function list.new()
    return setmetatable({ focus = 1, scroll = 1 }, List)
end

local function isItem(row)
    return row.header == nil and row.note == nil
end

-- Starts again at the top.
function List:reset()
    self.focus = 1
    self.scroll = 1
end

function List:itemCount(rows)
    local count = 0
    for _, row in ipairs(rows) do
        if isItem(row) then
            count = count + 1
        end
    end
    return count
end

-- Moves the selection by `delta` items, stopping at the ends. Returns whether it moved.
function List:move(rows, delta)
    local count = self:itemCount(rows)
    if count == 0 then
        return false
    end
    local focus = math.max(1, math.min(count, self.focus + delta))
    local moved = focus ~= self.focus
    self.focus = focus
    return moved
end

-- The selected row, or nil when the list has no items.
function List:focused(rows)
    local index = 0
    for _, row in ipairs(rows) do
        if isItem(row) then
            index = index + 1
            if index == self.focus then
                return row
            end
        end
    end
    return nil
end

-- The first item of each group that has items, as the item's number and the row of its header.
local function groupStarts(rows)
    local starts = {}
    local index = 0
    for i, row in ipairs(rows) do
        if isItem(row) then
            index = index + 1
        elseif row.header ~= nil and rows[i + 1] and isItem(rows[i + 1]) then
            starts[#starts + 1] = { focus = index + 1, row = i }
        end
    end
    return starts
end

function List:groupCount(rows)
    return #groupStarts(rows)
end

-- Moves the selection to the first item of the next group, or of the first group after the last one. The group's
-- header goes to the top of the list, so the whole group shows. Returns whether it moved.
function List:nextGroup(rows)
    local starts = groupStarts(rows)
    if #starts < 2 then
        return false
    end
    local target = starts[1]
    for _, start in ipairs(starts) do
        if start.focus > self.focus then
            target = start
            break
        end
    end
    self.focus = target.focus
    self.scroll = target.row
    return true
end

-- Works out which rows show between `top` and `bottom`, scrolling so that the selected row is in view.
-- `heightOf(row)` gives each row's height. Returns a list of `{ row, y, height, focused }`, and whether rows are
-- hidden above and below.
function List:place(rows, top, bottom, heightOf)
    local count = self:itemCount(rows)
    if count == 0 then
        return {}, false, false
    end
    self.focus = math.max(1, math.min(count, self.focus))

    local focusRow = 1
    local index = 0
    for i, row in ipairs(rows) do
        if isItem(row) then
            index = index + 1
            if index == self.focus then
                focusRow = i
                break
            end
        end
    end
    self.scroll = math.max(1, math.min(self.scroll, #rows))
    if focusRow < self.scroll then
        -- Keep the header of the selected row's group in view with it.
        if focusRow > 1 and not isItem(rows[focusRow - 1]) then
            self.scroll = focusRow - 1
        else
            self.scroll = focusRow
        end
    end
    while self.scroll < focusRow do
        local bottomOfFocus = top
        for i = self.scroll, focusRow do
            bottomOfFocus = bottomOfFocus + heightOf(rows[i])
        end
        if bottomOfFocus <= bottom then
            break
        end
        self.scroll = self.scroll + 1
    end
    -- The list doesn't scroll past its end, which would leave empty space below the last row.
    local used = 0
    for i = #rows, 1, -1 do
        used = used + heightOf(rows[i])
        if top + used > bottom then
            break
        end
        self.scroll = math.min(self.scroll, i)
    end

    local placed = {}
    local y = top
    index = 0
    for i = 1, self.scroll - 1 do
        if isItem(rows[i]) then
            index = index + 1
        end
    end
    local last = self.scroll - 1
    for i = self.scroll, #rows do
        local row = rows[i]
        local height = heightOf(row)
        if y + height > bottom then
            break
        end
        local focused = false
        if isItem(row) then
            index = index + 1
            focused = index == self.focus
        end
        placed[#placed + 1] = { row = row, y = y, height = height, focused = focused }
        y = y + height
        last = i
    end
    -- A group header at the bottom without any of its rows below it is left out.
    if last < #rows and #placed > 1 and placed[#placed].row.header ~= nil then
        placed[#placed] = nil
        last = last - 1
    end
    return placed, self.scroll > 1, last < #rows
end

return list
