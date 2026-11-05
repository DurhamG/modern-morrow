-- The details panel of an item: its icon, name and markers, its stats beside those of the equipped items it would
-- replace, and its effects. The inventory uses it, and the loot and barter screens can too.

local core = require('openmw.core')
local self = require('openmw.self')
local types = require('openmw.types')
local ui = require('openmw.ui')

local style = require('scripts.modernmorrow.ui.style')
local text = require('scripts.modernmorrow.ui.text')
local info = require('scripts.modernmorrow.items.info')
local recent = require('scripts.modernmorrow.items.recent')

local l10n = core.l10n('ModernMorrow')

local details = {}

local function gmst(name, fallback)
    return text.escape(core.getGMST(name) or fallback)
end

-- Markers for an entry, such as "New" or "Stolen", as `{ label, color }`. `entry` has `item` and `equipped`.
-- "New" only means something for the player's own items, so `showNew` can leave it out.
function details.markers(entry, showNew)
    local result = {}
    if entry.equipped then
        result[#result + 1] = { text.escape(l10n('Equipped')), style.color.dim }
    end
    if showNew ~= false and recent.isNew(entry.item.recordId) then
        result[#result + 1] = { text.escape(l10n('New')), style.color.new }
    end
    if info.isStolen(entry.item) then
        result[#result + 1] = { text.escape(l10n('Stolen')), style.color.worse }
    end
    if info.isUnreadSkillBook(entry.item) then
        result[#result + 1] = { text.escape(l10n('Unread')), style.color.new }
    end
    return result
end

-- The equipped items that an item the player doesn't own would replace, as the `compared` list of details.drawStats:
-- the item in each slot it fits. Items that fit two slots, such as rings, are compared with both, labelled by hand.
-- When one of the slots is empty, the item replaces nothing, and the list is empty.
function details.comparedWith(item)
    if not info.isEquipment(item) then
        return {}
    end
    local SLOT = types.Actor.EQUIPMENT_SLOT
    local compared = {}
    for _, slot in ipairs(info.slots(item)) do
        local equipped = types.Actor.getEquipment(self, slot)
        if equipped == nil then
            return {}
        end
        local label = l10n('Equipped')
        if slot == SLOT.LeftRing or slot == SLOT.LeftGauntlet then
            label = l10n('Left')
        elseif slot == SLOT.RightRing or slot == SLOT.RightGauntlet then
            label = l10n('Right')
        end
        compared[#compared + 1] = { label = text.escape(label), item = equipped }
    end
    return compared
end

-- Draws the icon, name, description and markers in the top 100 units of a panel at x and y.
function details.drawHeader(canvas, x, y, w, entry, showNew)
    local size = style.textSize
    canvas:icon(x + 16, y + 16, 64, entry.item, entry.equipped)
    canvas:text(x + 92, y + 14, w - 108, 34, text.fit(info.name(entry.item) ..
        (entry.item.count > 1 and (' (' .. entry.item.count .. ')') or ''), w - 108, size.large), size.large,
        style.color.bright)
    canvas:text(x + 92, y + 46, w - 108, 26, text.escape(info.describe(entry.item)), size.small, style.color.dim)
    local tx = x + 92
    for _, marker in ipairs(details.markers(entry, showNew)) do
        canvas:text(tx, y + 70, 120, 20, marker[1], size.small, marker[2])
        tx = tx + text.width(marker[1], size.small) + 12
    end
end

-- A row of the stats table. Numbers are compared with the equipped item's, where there is one.
local function numberRow(label, value, lowerIsBetter)
    return { label = label, value = text.number(value), number = value, comparable = true,
        higherIsBetter = not lowerIsBetter }
end

local function rangeRow(label, low, high)
    return { label = label, value = info.rangeText(low, high), number = high, comparable = true,
        higherIsBetter = true }
end

local function statRows(item)
    local rows = {}
    local record = info.record(item)
    local t = item.type
    if t == types.Weapon then
        local weaponType = info.weaponType(item)
        local melee = weaponType == nil or weaponType.class == 'melee'
        if melee then
            rows[#rows + 1] = rangeRow(gmst('sChop', 'Chop'), record.chopMinDamage, record.chopMaxDamage)
            rows[#rows + 1] = rangeRow(gmst('sSlash', 'Slash'), record.slashMinDamage, record.slashMaxDamage)
            rows[#rows + 1] = rangeRow(gmst('sThrust', 'Thrust'), record.thrustMinDamage, record.thrustMaxDamage)
        else
            rows[#rows + 1] = rangeRow(gmst('sAttack', 'Attack'), record.chopMinDamage, record.chopMaxDamage)
        end
        if not info.isAmmunition(item) then
            rows[#rows + 1] = numberRow(text.escape(l10n('ColumnSpeed')), record.speed)
            if melee then
                rows[#rows + 1] = numberRow(gmst('sRange', 'Range'), record.reach)
            end
        end
    end
    if t == types.Armor then
        rows[#rows + 1] = numberRow(gmst('sArmorRating', 'Armor Rating'), info.armorRating(item))
    end
    local quality = info.toolQuality(item)
    if quality then
        rows[#rows + 1] = numberRow(gmst('sQuality', 'Quality'), quality)
    end
    if info.hasCondition(item) then
        local tool = t == types.Lockpick or t == types.Probe or t == types.Repair
        local condition = info.condition(item)
        if condition then
            rows[#rows + 1] = numberRow(tool and gmst('sUses', 'Uses') or gmst('sCondition', 'Condition'), condition)
        end
    end
    if info.isSoulGem(item) then
        local soul = info.soulName(item)
        rows[#rows + 1] = { label = text.escape(l10n('Soul')), value = text.escape(soul or l10n('None')) }
    end
    rows[#rows + 1] = numberRow(gmst('sWeight', 'Weight'), info.weight(item), true)
    rows[#rows + 1] = numberRow(gmst('sValue', 'Value'), info.value(item))
    return rows
end

-- Draws the item's stats from y down, beside the stats of the equipped items it would replace, then its effects.
-- `compared` is a list of `{ label, item }`. A stat is green where it beats what is equipped, and red where it falls
-- short. Nothing is drawn below `bottom`. Returns where the drawing ended.
function details.drawStats(canvas, x, y, w, bottom, item, compared)
    local size = style.textSize
    local color = style.color
    local rows = statRows(item)
    local otherRows = {}
    for i, other in ipairs(compared) do
        otherRows[i] = statRows(other.item)
    end

    local ry = y
    local valueW = 80
    local valuesX = x + w - 16 - (1 + #compared) * valueW
    if #compared > 0 then
        canvas:text(valuesX, ry, valueW, 24, text.escape(l10n('ColumnThis')), size.small, color.dim, ui.ALIGNMENT.End)
        for i, other in ipairs(compared) do
            canvas:text(valuesX + i * valueW, ry, valueW, 24, other.label, size.small, color.dim, ui.ALIGNMENT.End)
        end
        ry = ry + 26
    end
    for _, row in ipairs(rows) do
        if ry + 30 > bottom then
            break
        end
        canvas:text(x + 16, ry, w - 32, 28, row.label, size.normal, color.text)
        local others = {}
        for i, other in ipairs(otherRows) do
            for _, candidate in ipairs(other) do
                if candidate.label == row.label then
                    others[i] = candidate
                end
            end
        end
        -- The value is compared with the weakest of the equipped items, which is the one worth replacing.
        local valueColor = color.bright
        if row.comparable then
            local weakest = nil
            for i = 1, #compared do
                local other = others[i]
                if other and other.comparable then
                    if weakest == nil or (row.higherIsBetter and other.number < weakest)
                            or (not row.higherIsBetter and other.number > weakest) then
                        weakest = other.number
                    end
                end
            end
            if weakest and row.number ~= weakest then
                valueColor = ((row.number > weakest) == row.higherIsBetter) and color.better or color.worse
            end
        end
        canvas:text(valuesX, ry, valueW, 28, row.value, size.normal, valueColor, ui.ALIGNMENT.End)
        for i = 1, #compared do
            canvas:text(valuesX + i * valueW, ry, valueW, 28, others[i] and others[i].value or '-', size.normal,
                color.dim, ui.ALIGNMENT.End)
        end
        ry = ry + 30
    end

    local effects = info.effects(item)
    if #effects > 0 and ry + 60 <= bottom then
        ry = ry + 8
        local consumable = item.type == types.Potion or item.type == types.Ingredient or info.isMagicScroll(item)
        canvas:text(x + 16, ry, w - 32, 24, text.escape(l10n(consumable and 'MagicEffects' or 'Enchantment')),
            size.small, color.dim)
        ry = ry + 24
        for _, effect in ipairs(effects) do
            if ry + 26 > bottom then
                break
            end
            canvas:text(x + 16, ry, w - 32, 26, text.fit(effect, w - 32, size.normal), size.normal, color.new)
            ry = ry + 26
        end
    end
    if #compared > 0 and ry + 36 <= bottom then
        local names = {}
        for i, other in ipairs(compared) do
            names[i] = info.name(other.item)
        end
        canvas:text(x + 16, ry + 12, w - 32, 24,
            text.fit(l10n('Replaces') .. ' ' .. table.concat(names, ', '), w - 32, size.small), size.small, color.dim)
        ry = ry + 36
    end
    return ry
end

return details
