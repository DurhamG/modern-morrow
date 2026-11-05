-- What the magic screen lists: everything the player can ready to cast, and the effects on the player.
-- The numbers come from the engine's Lua API. The rules here pick what to list, as the engine's spell window does.

local core = require('openmw.core')
local self = require('openmw.self')
local types = require('openmw.types')
local vfs = require('openmw.vfs')

local castables = {}

local SPELL_TYPE = core.magic.SPELL_TYPE
local ENCHANTMENT_TYPE = core.magic.ENCHANTMENT_TYPE

-- The kinds of item that can carry an enchantment.
local ENCHANTABLE = { types.Weapon, types.Armor, types.Clothing, types.Book }

local bigIcons = {}

-- The large version of a magic effect's icon, which has "b_" before the file name, or the small one if there is no
-- large one.
function castables.bigIcon(effect)
    local path = effect and effect.icon
    if path == nil or path == '' then
        return nil
    end
    local big = bigIcons[path]
    if big == nil then
        local candidate = path:gsub('([^/\\]+)$', 'b_%1')
        big = vfs.fileExists(candidate) and candidate or path
        bigIcons[path] = big
    end
    return big
end

-- The effects of a spell or an enchantment, each written out in full, such as "Fire Damage 2 to 20 pts for 1 sec in
-- 5 ft on Target". Plain text.
function castables.describe(effects)
    local lines = {}
    for i, effect in ipairs(effects or {}) do
        lines[i] = core.magic.effects.getDescription(effect)
    end
    return lines
end

-- The spells and powers the player got from their race or birthsign, which can't be deleted.
local function inherentSpells()
    local result = {}
    local race = types.NPC.races.records[types.NPC.record(self).race]
    if race then
        for _, id in ipairs(race.spells) do
            result[id] = true
        end
    end
    local signId = types.Player.getBirthSign(self)
    local sign = signId and signId ~= '' and types.Player.birthSigns.records[signId]
    if sign then
        for _, id in ipairs(sign.spells) do
            result[id] = true
        end
    end
    return result
end

-- Everything the player can ready to cast: spells, powers, magic items with a cast when used enchantment, and
-- scrolls. Each entry is a table with:
--   kind         'spell', 'power', 'item' or 'scroll'
--   key          the entry's key in the log of recent spells, such as "spell:fireball"
--   name         its name, as plain text
--   spell        the spell record, for spells and powers
--   item         the item, for magic items and scrolls
--   enchantment  the item's enchantment
--   effects      the effects of the spell or the enchantment
--   icon         the first effect's large icon, for spells and powers
--   school       the skill ID of the spell's school
--   cost         the magicka for a spell, or the charge per cast for an item
--   chance       the chance to cast a spell, from 0 to 100
--   charge, maxCharge, count
--   ready        whether it is ready to cast
--   deletable    whether the player may delete it: spells other than the race's and the birthsign's
--   usedToday    whether a power was used in the last day
function castables.read()
    local result = {}
    local actorSpells = types.Actor.spells(self)
    local readySpell = types.Actor.getSelectedSpell(self)
    local readyItem = types.Actor.getSelectedEnchantedItem(self)
    local inherent = inherentSpells()

    for _, spell in ipairs(actorSpells) do
        if spell.type == SPELL_TYPE.Spell or spell.type == SPELL_TYPE.Power then
            local power = spell.type == SPELL_TYPE.Power
            local first = spell.effects[1]
            result[#result + 1] = {
                kind = power and 'power' or 'spell',
                key = 'spell:' .. spell.id,
                name = spell.name,
                spell = spell,
                effects = spell.effects,
                icon = first and castables.bigIcon(first.effect),
                school = core.magic.spells.getSchool(spell, self),
                cost = core.magic.spells.getCost(spell),
                chance = math.floor(actorSpells:getSuccessChance(spell)),
                ready = readyItem == nil and readySpell ~= nil and readySpell.id == spell.id,
                deletable = not power and not inherent[spell.id],
                usedToday = power and not actorSpells:canUsePower(spell),
            }
        end
    end

    local inventory = types.Actor.inventory(self)
    for _, itemType in ipairs(ENCHANTABLE) do
        for _, item in ipairs(inventory:getAll(itemType)) do
            local record = itemType.record(item)
            local enchantment = record.enchant and record.enchant ~= ''
                and core.magic.enchantments.records[record.enchant]
            if enchantment and (enchantment.type == ENCHANTMENT_TYPE.CastOnUse
                    or enchantment.type == ENCHANTMENT_TYPE.CastOnce) then
                local data = types.Item.itemData(item)
                local charge = data and data.enchantmentCharge
                local maxCharge = core.magic.enchantments.getMaxCharge(enchantment)
                result[#result + 1] = {
                    kind = enchantment.type == ENCHANTMENT_TYPE.CastOnce and 'scroll' or 'item',
                    key = 'item:' .. item.recordId,
                    name = record.name,
                    item = item,
                    enchantment = enchantment,
                    effects = enchantment.effects,
                    cost = core.magic.enchantments.getEffectiveCastCost(enchantment, self),
                    -- An item whose charge was never used has a full charge.
                    charge = math.floor(charge or maxCharge),
                    maxCharge = math.floor(maxCharge),
                    count = item.count,
                    ready = readyItem == item,
                }
            end
        end
    end
    return result
end

-- A summary of what read() returned, to tell when the screen needs to redraw.
function castables.signature(list)
    local parts = {}
    for _, castable in ipairs(list) do
        parts[#parts + 1] = string.format('%s%s%d/%d/%d/%d%s', castable.key, castable.ready and '*' or '',
            castable.charge or 0, castable.count or 1, castable.chance or 0, castable.cost or 0,
            castable.usedToday and 'u' or '')
    end
    return table.concat(parts, ';')
end

-- What kind of spell an active effect came from, as a key of the mod's strings, or nil.
local SOURCE_KEYS = {
    [SPELL_TYPE.Spell] = 'MagicSourceSpell',
    [SPELL_TYPE.Ability] = 'MagicSourceAbility',
    [SPELL_TYPE.Blight] = 'MagicSourceBlight',
    [SPELL_TYPE.Disease] = 'MagicSourceDisease',
    [SPELL_TYPE.Curse] = 'MagicSourceCurse',
    [SPELL_TYPE.Power] = 'MagicSourcePower',
}

-- The effects on the player. Each is a table with `icon`, `text` (such as "Shield 5 pts"), `name` (where it came
-- from, such as "Shield"), `sourceKey` (such as 'MagicSourceSpell', or nil) and `timeLeft` (seconds, or nil for
-- effects that last until something removes them). Effects that run out come first, soonest first.
function castables.activeEffects()
    local result = {}
    for _, active in pairs(types.Actor.activeSpells(self)) do
        local sourceKey = nil
        local spell = core.magic.spells.records[active.id]
        if spell then
            sourceKey = SOURCE_KEYS[spell.type]
        elseif active.fromEquipment then
            sourceKey = 'MagicSourceWorn'
        end
        for _, effect in ipairs(active.effects) do
            if effect.durationLeft == nil or effect.durationLeft > 0 then
                local record = core.magic.effects.records[effect.id]
                local magnitude = math.floor(effect.magnitudeThisFrame or 0)
                local description = core.magic.effects.getDescription({
                    id = effect.id,
                    affectedSkill = effect.affectedSkill,
                    affectedAttribute = effect.affectedAttribute,
                    magnitudeMin = magnitude,
                    magnitudeMax = magnitude,
                }, { isConstant = true })
                result[#result + 1] = {
                    icon = castables.bigIcon(record),
                    text = description,
                    name = active.name,
                    sourceKey = sourceKey,
                    timeLeft = effect.durationLeft,
                    order = #result,
                }
            end
        end
    end
    table.sort(result, function(a, b)
        local left = a.timeLeft or math.huge
        local right = b.timeLeft or math.huge
        if left ~= right then
            return left < right
        end
        return a.order < b.order
    end)
    return result
end

return castables
