-- The log of items that entered the player's inventory, newest first, which the inventory's Recent tab lists and
-- which marks items as new. Entries are by record id. Each one says whether the player has seen it, which happens
-- once it was highlighted in a list. The player script's onItemAdded handler adds the entries, and the log is kept in
-- the save with the player script's data.

local info = require('scripts.modernmorrow.items.info')

local recent = {}

-- The Recent tab lists this many entries.
recent.SHOWN = 40
-- Entries older than that stay only while unseen, up to this many in all, so the first items of a big haul still
-- show as new elsewhere in the inventory.
local KEPT = 200

local log = {}

local function add(recordId)
    for i = #log, 1, -1 do
        if log[i].id == recordId then
            table.remove(log, i)
        end
    end
    table.insert(log, 1, { id = recordId, seen = false })
    local kept = {}
    for i, entry in ipairs(log) do
        if #kept >= KEPT then
            break
        end
        if i <= recent.SHOWN or not entry.seen then
            kept[#kept + 1] = entry
        end
    end
    log = kept
end

-- From the player script's onItemAdded handler. Gold isn't an item the player looks for, so it isn't logged.
function recent.onItemAdded(item)
    if item:isValid() and not info.isGold(item) then
        add(item.recordId)
    end
end

function recent.isNew(recordId)
    for _, entry in ipairs(log) do
        if entry.id == recordId then
            return not entry.seen
        end
    end
    return false
end

function recent.markSeen(recordId)
    for _, entry in ipairs(log) do
        if entry.id == recordId then
            entry.seen = true
        end
    end
end

-- The record ids the Recent tab lists, newest first.
function recent.shown()
    local ids = {}
    for i = 1, math.min(#log, recent.SHOWN) do
        ids[i] = log[i].id
    end
    return ids
end

function recent.save()
    return { log = log }
end

function recent.load(data)
    log = {}
    if data and data.log then
        for _, entry in ipairs(data.log) do
            log[#log + 1] = { id = entry.id, seen = entry.seen }
        end
    end
end

return recent
