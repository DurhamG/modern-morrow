-- What the screens say about an item: what kind it is, where it is worn, and its stats.
-- Game rules come from the engine's Lua API. The tables here only map record types to names and slots, as the
-- engine's item classes do.

local core = require('openmw.core')
local types = require('openmw.types')
local self = require('openmw.self')
local I = require('openmw.interfaces')

local text = require('scripts.modernmorrow.ui.text')

local l10n = core.l10n('ModernMorrow')

local function gmst(name, fallback)
    return core.getGMST(name) or fallback
end

local info = {}

local WEAPON = types.Weapon.TYPE

-- How each weapon type attacks.
local WEAPON_CLASSES = {
    [WEAPON.ShortBladeOneHand] = 'melee',
    [WEAPON.LongBladeOneHand] = 'melee',
    [WEAPON.LongBladeTwoHand] = 'melee',
    [WEAPON.BluntOneHand] = 'melee',
    [WEAPON.BluntTwoClose] = 'melee',
    [WEAPON.BluntTwoWide] = 'melee',
    [WEAPON.SpearTwoWide] = 'melee',
    [WEAPON.AxeOneHand] = 'melee',
    [WEAPON.AxeTwoHand] = 'melee',
    [WEAPON.MarksmanBow] = 'ranged',
    [WEAPON.MarksmanCrossbow] = 'ranged',
    [WEAPON.MarksmanThrown] = 'thrown',
    [WEAPON.Arrow] = 'ammo',
    [WEAPON.Bolt] = 'ammo',
}

function info.record(item)
    return item.type.record(item)
end

function info.name(item)
    return info.record(item).name
end

function info.isGold(item)
    return types.Miscellaneous.isGold(item)
end

function info.isSoulGem(item)
    return types.Miscellaneous.isSoulGem(item)
end

function info.isKey(item)
    return item.type == types.Miscellaneous and info.record(item).isKey == true
end

-- Whether the item is worn or held: a weapon, armour, clothing or a light.
function info.isEquipment(item)
    local t = item.type
    return t == types.Weapon or t == types.Armor or t == types.Clothing or t == types.Light
end

function info.isTool(item)
    local t = item.type
    return t == types.Lockpick or t == types.Probe or t == types.Repair or t == types.Apparatus
end

function info.enchantment(item)
    local id = info.record(item).enchant
    if id == nil or id == '' then
        return nil
    end
    return core.magic.enchantments.records[id]
end

-- A book with an enchantment is a scroll the player can cast.
function info.isMagicScroll(item)
    return item.type == types.Book and info.enchantment(item) ~= nil
end

-- A weapon's skill and how it attacks, as `{ skill, class }`, where the class is 'melee', 'ranged', 'thrown' or
-- 'ammo'. Nil for other items.
function info.weaponType(item)
    if item.type ~= types.Weapon then
        return nil
    end
    local record = info.record(item)
    return { skill = record.skill, class = WEAPON_CLASSES[record.type] or 'melee' }
end

function info.isAmmunition(item)
    local weaponType = info.weaponType(item)
    return weaponType ~= nil and weaponType.class == 'ammo'
end

-- The slots the item can go in, in the order the engine tries them.
function info.slots(item)
    return types.Item.getEquipmentSlots(item)
end

-- The weapon's strongest attack, as the lowest and highest damage.
function info.bestAttack(record)
    local low, high = record.chopMinDamage, record.chopMaxDamage
    if record.slashMaxDamage > high then
        low, high = record.slashMinDamage, record.slashMaxDamage
    end
    if record.thrustMaxDamage > high then
        low, high = record.thrustMinDamage, record.thrustMaxDamage
    end
    return low, high
end

function info.rangeText(low, high)
    return string.format('%d-%d', low, high)
end

-- The armor rating of a piece of armor for the player, adjusted for their skill in its weight class, rounded down as
-- the engine's tooltip shows it.
function info.armorRating(item)
    return math.floor(I.Combat.getSkillAdjustedArmorRating(item, self))
end

-- The weight class of a piece of armor, as the skill ID of 'lightarmor', 'mediumarmor' or 'heavyarmor'.
function info.armorSkill(item)
    return I.Combat.getArmorSkill(item)
end

-- The name of the weight class of a piece of armor, such as "Light".
function info.armorClass(item)
    local skill = info.armorSkill(item)
    if skill == 'lightarmor' then
        return gmst('sLight', 'Light')
    elseif skill == 'mediumarmor' then
        return gmst('sMedium', 'Medium')
    end
    return gmst('sHeavy', 'Heavy')
end

-- The quality of a lockpick, probe, repair tool or alchemy apparatus, or nil for other items.
function info.toolQuality(item)
    if info.isTool(item) then
        return info.record(item).quality
    end
    return nil
end

-- Whether the item wears down: weapons other than thrown weapons and ammunition, armour, and the tools with uses.
function info.hasCondition(item)
    local t = item.type
    if t == types.Weapon then
        local weaponType = info.weaponType(item)
        return weaponType ~= nil and weaponType.class ~= 'ammo' and weaponType.class ~= 'thrown'
    end
    return t == types.Armor or t == types.Lockpick or t == types.Probe or t == types.Repair
end

function info.condition(item)
    local data = types.Item.itemData(item)
    return data and data.condition
end

function info.weight(item)
    return info.record(item).weight
end

-- The value as the engine counts it, including the soul in a soul gem.
function info.value(item)
    return types.Item.getValue(item)
end

function info.isStolen(item)
    return #types.Item.getStolenFrom(item) > 0
end

-- The name of a magic effect without its numbers, such as "Fortify Strength".
local function effectName(effect)
    return core.magic.effects.getDescription(effect, { isConstant = true, noMagnitude = true })
end

-- The effects the player can see, as plain names. An ingredient's effects that the player's Alchemy skill doesn't
-- reveal yet are "?".
function info.effects(item)
    local names = {}
    if item.type == types.Potion then
        for _, effect in ipairs(info.record(item).effects) do
            names[#names + 1] = effectName(effect)
        end
    elseif item.type == types.Ingredient then
        local known = types.Player.getKnownIngredientEffects(self, item)
        for _, effect in ipairs(known) do
            names[#names + 1] = effectName(effect)
        end
        for _ = #known + 1, #info.record(item).effects do
            names[#names + 1] = '?'
        end
    else
        local enchantment = info.enchantment(item)
        if enchantment then
            for _, effect in ipairs(enchantment.effects) do
                names[#names + 1] = effectName(effect)
            end
        end
    end
    return names
end

-- The skill a book teaches, or nil.
function info.bookSkill(item)
    if item.type ~= types.Book then
        return nil
    end
    local skill = info.record(item).skill
    if skill == nil or skill == '' then
        return nil
    end
    return core.stats.Skill.records[skill]
end

-- Whether the item is a book that teaches a skill and that the player hasn't learned from yet.
function info.isUnreadSkillBook(item)
    return info.bookSkill(item) ~= nil and not types.Player.hasReadSkillBook(self, item)
end

-- A short description of the item, such as "Long Blade" or "Potion". Plain text.
function info.describe(item)
    local t = item.type
    if t == types.Weapon then
        local weaponType = info.weaponType(item)
        local skill = weaponType and core.stats.Skill.records[weaponType.skill]
        return skill and skill.name or gmst('sWeapon', 'Weapon')
    elseif t == types.Armor then
        return info.armorClass(item)
    elseif t == types.Clothing then
        return l10n('KindClothing')
    elseif t == types.Potion then
        return l10n('KindPotion')
    elseif t == types.Ingredient then
        return gmst('sIngredients', 'Ingredient')
    elseif t == types.Book then
        if info.enchantment(item) then
            return l10n('KindScroll')
        end
        local skill = info.bookSkill(item)
        if skill then
            return l10n('KindSkillBook') .. ': ' .. skill.name
        end
        return l10n('KindBook')
    elseif t == types.Lockpick then
        return l10n('KindLockpick')
    elseif t == types.Probe then
        return l10n('KindProbe')
    elseif t == types.Repair then
        return l10n('KindRepair')
    elseif t == types.Apparatus then
        return l10n('KindApparatus')
    elseif t == types.Light then
        return l10n('KindLight')
    elseif info.isSoulGem(item) then
        return l10n('KindSoulGem')
    elseif info.isKey(item) then
        return l10n('KindKey')
    end
    return l10n('KindMisc')
end

-- The number that decides whether an item is an upgrade: the armor rating or the highest damage. Nil for other items.
function info.upgradeNumber(item)
    if item.type == types.Armor then
        return info.armorRating(item)
    elseif item.type == types.Weapon then
        local _, high = info.bestAttack(info.record(item))
        return high
    end
    return nil
end

-- The second line of an entry: what the item is, and its main stat, such as "Long Blade, 1-20". Escaped.
function info.summary(item)
    if item.type == types.Armor then
        return text.escape(info.armorClass(item)) .. ',  ' .. text.escape(gmst('sArmor', 'Armor')) .. ' '
            .. text.number(info.armorRating(item))
    end
    local s = text.escape(info.describe(item))
    if item.type == types.Weapon then
        s = s .. ',  ' .. info.rangeText(info.bestAttack(info.record(item)))
    else
        local effects = info.effects(item)
        if #effects > 0 then
            s = s .. ',  ' .. text.escape(effects[1])
        end
    end
    return s
end

-- The groups that the loot, barter and companion screens sort items into, in their order. Gold comes first.
info.GROUP = { Gold = 1, Weapons = 2, Armor = 3, Clothing = 4, Consumables = 5, Books = 6, Tools = 7, Misc = 8 }
local GROUP_KEYS = { 'GroupGold', 'GroupWeapons', 'GroupArmor', 'GroupClothing', 'GroupConsumables', 'GroupBooks',
    'GroupTools', 'GroupMisc' }

function info.group(item)
    local t = item.type
    if info.isGold(item) then
        return info.GROUP.Gold
    elseif t == types.Weapon then
        return info.GROUP.Weapons
    elseif t == types.Armor then
        return info.GROUP.Armor
    elseif t == types.Clothing then
        return info.GROUP.Clothing
    elseif t == types.Potion or t == types.Ingredient or info.isMagicScroll(item) then
        return info.GROUP.Consumables
    elseif t == types.Book then
        return info.GROUP.Books
    elseif info.isTool(item) then
        return info.GROUP.Tools
    end
    return info.GROUP.Misc
end

-- The name of a group, as plain text.
function info.groupName(group)
    return l10n(GROUP_KEYS[group])
end

-- The inventory screen's tabs, which the loot screen also shows for the player's own items.
info.INVENTORY_TAB = { Recent = 1, Equipment = 2, Consumables = 3, Notes = 4, Tools = 5, Misc = 6 }
info.INVENTORY_TAB_KEYS = { 'TabRecent', 'TabEquipment', 'TabConsumables', 'TabNotes', 'TabTools', 'TabMisc' }

-- The inventory tab an item is listed on, other than Recent.
function info.inventoryTab(item)
    local tab = info.INVENTORY_TAB
    if info.isEquipment(item) then
        return tab.Equipment
    elseif item.type == types.Potion or item.type == types.Ingredient then
        return tab.Consumables
    elseif item.type == types.Book then
        return info.isMagicScroll(item) and tab.Consumables or tab.Notes
    elseif info.isTool(item) then
        return tab.Tools
    end
    return tab.Misc
end

-- Whether a piece of armor or a weapon beats what the player wears in one of the slots it fits, or would fill an empty
-- one.
function info.isUpgrade(item)
    local number = info.upgradeNumber(item)
    if number == nil then
        return false
    end
    for _, slot in ipairs(info.slots(item)) do
        local equipped = types.Actor.getEquipment(self, slot)
        if equipped == nil then
            return true
        end
        local other = info.upgradeNumber(equipped)
        if other == nil or other < number then
            return true
        end
    end
    return false
end

-- The name of the creature whose soul is in a soul gem, or nil.
function info.soulName(item)
    local data = types.Item.itemData(item)
    local soul = data and data.soul
    if soul == nil or soul == '' then
        return nil
    end
    local creature = types.Creature.records[soul]
    return creature and creature.name or soul
end

return info
