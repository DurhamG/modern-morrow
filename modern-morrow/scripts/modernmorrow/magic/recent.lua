-- The spells, powers, magic items and scrolls the player readied recently, newest first. Readying one anywhere counts,
-- such as with a quick key during play, because the log watches what is ready every frame. The log is kept in the save.
--
-- Entries are keys from magic/castables.lua, such as "spell:fireball" or "item:<record id>".

local types = require('openmw.types')
local self = require('openmw.self')

local recent = {}

-- How many entries the log keeps.
recent.KEPT = 40

local keys = {}
-- What was ready last frame.
local lastReady = nil

-- The key of the spell or item that is ready to cast, or nil.
function recent.readyKey()
    local item = types.Actor.getSelectedEnchantedItem(self)
    if item then
        return 'item:' .. item.recordId
    end
    local spell = types.Actor.getSelectedSpell(self)
    if spell then
        return 'spell:' .. spell.id
    end
    return nil
end

local function moveToFront(key)
    for i = #keys, 1, -1 do
        if keys[i] == key then
            table.remove(keys, i)
        end
    end
    table.insert(keys, 1, key)
    while #keys > recent.KEPT do
        keys[#keys] = nil
    end
end

-- Called every frame. Adds what the player readied since the last frame.
function recent.update()
    local key = recent.readyKey()
    if key ~= lastReady then
        lastReady = key
        if key then
            moveToFront(key)
        end
    end
end

-- The keys, newest first.
function recent.keys()
    return keys
end

function recent.load(data)
    keys = {}
    if type(data) == 'table' then
        for _, key in ipairs(data) do
            if type(key) == 'string' then
                keys[#keys + 1] = key
            end
        end
    end
    -- What is ready when the game loads goes to the front of the log, as if the player had just readied it.
    lastReady = nil
end

function recent.save()
    return keys
end

return recent
