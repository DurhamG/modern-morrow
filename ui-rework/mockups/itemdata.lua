-- Prints one JSON object per line for each record whose name is in NAMES, so the inventory mock-up can show
-- real names, icons and stats. The mock-up generator fills in NAMES before sending this to the game.
local types = require('openmw.types')
local core = require('openmw.core')

local NAMES = {}

-- The game's console mangles backslashes in Lua code, so this builds them with string.char.
local BACKSLASH = string.char(92)

local function quote(s)
    local escaped = tostring(s):gsub(BACKSLASH, BACKSLASH .. BACKSLASH):gsub('"', BACKSLASH .. '"')
    return '"' .. escaped .. '"'
end

local function encode(t)
    local parts = {}
    for k, v in pairs(t) do
        local value
        if type(v) == 'table' then
            local items = {}
            for _, item in ipairs(v) do
                table.insert(items, quote(item))
            end
            value = '[' .. table.concat(items, ',') .. ']'
        elseif type(v) == 'number' then
            value = tostring(v)
        else
            value = quote(v)
        end
        table.insert(parts, quote(k) .. ':' .. value)
    end
    return '{' .. table.concat(parts, ',') .. '}'
end

local function effectNames(enchantId)
    local names = {}
    if enchantId == nil or enchantId == '' then
        return names
    end
    local enchantment = core.magic.enchantments.records[enchantId]
    if enchantment == nil then
        return names
    end
    for _, effect in ipairs(enchantment.effects) do
        local ok, name = pcall(function() return effect.effect.name end)
        table.insert(names, ok and name or tostring(effect.id))
    end
    return names
end

local wanted = {}
for _, name in ipairs(NAMES) do
    wanted[name] = true
end

local function dump(kind, records, fields)
    for i = 1, #records do
        local r = records[i]
        if wanted[r.name] then
            wanted[r.name] = nil
            local t = { kind = kind, id = r.id, name = r.name, icon = r.icon, weight = r.weight, value = r.value,
                effects = effectNames(r.enchant) }
            for _, field in ipairs(fields) do
                t[field] = r[field]
            end
            print(encode(t))
        end
    end
end

dump('weapon', types.Weapon.records, { 'type', 'chopMinDamage', 'chopMaxDamage', 'slashMinDamage', 'slashMaxDamage',
    'thrustMinDamage', 'thrustMaxDamage', 'speed', 'reach', 'health' })
dump('armor', types.Armor.records, { 'type', 'baseArmor', 'health' })
dump('clothing', types.Clothing.records, { 'type' })
dump('potion', types.Potion.records, {})
dump('light', types.Light.records, {})
