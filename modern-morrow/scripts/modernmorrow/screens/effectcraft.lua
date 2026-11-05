-- Enchanting and spellmaking. It replaces the engine's EnchantingDialog and SpellCreationDialog windows with the
-- fork's effect crafting screen (ui-rework/design/crafting.md in the fork). Both share it, and spellmaking leaves out
-- the item, the soul gem and the cast type.
--
-- The right side lists what is being made: the item, the soul gem, the cast type, the effects and a line to add an
-- effect, with the numbers at the bottom. A on a line fills the left side with what can go there: the items that can
-- be enchanted, the soul gems with souls, or the effects the player knows. Choosing one returns to the right side, on
-- the next thing still missing. An effect is edited on the left side, in place of the list: each of its settings has a
-- value and a bar, and left and right change it. The numbers on the right follow every change.
--
-- Pressing the left stick names the enchanted item or the spell with the on-screen keyboard. Until then, an enchanted
-- item keeps the item's name, and a spell takes the name of its first effect.
--
-- The engine's Lua API works out the numbers and checks the effects, and the global script makes the enchantment or
-- the spell. The API shows no message and plays no sound, so this screen does both, as the engine's windows do.

local core = require('openmw.core')
local self = require('openmw.self')
local types = require('openmw.types')
local ui = require('openmw.ui')
local ambient = require('openmw.ambient')
local I = require('openmw.interfaces')

local style = require('scripts.modernmorrow.ui.style')
local layer = require('scripts.modernmorrow.ui.layer')
local draw = require('scripts.modernmorrow.ui.draw')
local text = require('scripts.modernmorrow.ui.text')
local hints = require('scripts.modernmorrow.ui.hints')
local keyboard = require('scripts.modernmorrow.ui.keyboard')
local list = require('scripts.modernmorrow.ui.list')
local screen = require('scripts.modernmorrow.ui.screen')
local info = require('scripts.modernmorrow.items.info')
local castables = require('scripts.modernmorrow.magic.castables')
local router = require('scripts.modernmorrow.input.router')

local l10n = core.l10n('ModernMorrow')

local effectcraft = {}

local color = style.color
local size = style.textSize

local ENTRY_HEIGHT = 52
local LINE_HEIGHT = 44
local EFFECT_LINE_HEIGHT = 32
local SETTING_HEIGHT = 54

-- The spellmaking window's limits, which enchanting shares.
local LIMITS = core.magic.CUSTOM_EFFECT_LIMITS

local RANGE = core.magic.RANGE
local ENCHANTMENT_TYPE = core.magic.ENCHANTMENT_TYPE

local function L(key)
    return text.escape(l10n(key))
end

local function gmstRaw(name, fallback)
    return core.getGMST(name) or fallback
end

local function gmst(name, fallback)
    return text.escape(gmstRaw(name, fallback))
end

local function playerGold()
    return types.Actor.inventory(self):countOf('gold_001')
end

local function click()
    ambient.playSound('menu click')
end

local RANGE_NAMES = {
    [RANGE.Self] = { 'sRangeSelf', 'Self' },
    [RANGE.Touch] = { 'sRangeTouch', 'Touch' },
    [RANGE.Target] = { 'sRangeTarget', 'Target' },
}

local CAST_TYPE_NAMES = {
    [ENCHANTMENT_TYPE.CastOnce] = { 'sItemCastOnce', 'Cast Once' },
    [ENCHANTMENT_TYPE.CastOnStrike] = { 'sItemCastWhenStrikes', 'Cast When Strikes' },
    [ENCHANTMENT_TYPE.CastOnUse] = { 'sItemCastWhenUsed', 'Cast When Used' },
    [ENCHANTMENT_TYPE.ConstantEffect] = { 'sItemCastConstant', 'Constant Effect' },
}

local function rangeName(range)
    local names = RANGE_NAMES[range] or RANGE_NAMES[RANGE.Self]
    return gmst(names[1], names[2])
end

local function castTypeName(castType)
    local names = CAST_TYPE_NAMES[castType] or CAST_TYPE_NAMES[ENCHANTMENT_TYPE.CastOnce]
    return gmst(names[1], names[2])
end

-- ---------------------------------------------------------------------------------------------------------------
-- The effect rules, as the engine's effect editor applies them

local function isRangeAllowed(record, range, constant)
    if range == RANGE.Self then
        return record.onSelf or constant
    elseif range == RANGE.Touch then
        return record.onTouch and not constant
    end
    return record.onTarget and not constant
end

local function allowedRanges(record, constant)
    local names = {}
    for _, range in ipairs({ RANGE.Self, RANGE.Touch, RANGE.Target }) do
        if isRangeAllowed(record, range, constant) then
            names[#names + 1] = rangeName(range)
        end
    end
    return table.concat(names, ', ')
end

-- A new effect, with the settings the engine's effect editor starts with: the first range that the effect allows, and
-- the smallest magnitude, duration and area.
local function newEffect(record, constant)
    local range = RANGE.Self
    if not isRangeAllowed(record, RANGE.Self, constant) then
        range = RANGE.Touch
    end
    if not isRangeAllowed(record, RANGE.Touch, constant) and range == RANGE.Touch then
        range = RANGE.Target
    end
    return {
        id = record.id,
        range = range,
        magnitudeMin = LIMITS.minMagnitude,
        magnitudeMax = LIMITS.minMagnitude,
        duration = LIMITS.minDuration,
        area = LIMITS.minArea,
    }
end

local function copyEffect(effect)
    local result = {}
    for key, value in pairs(effect) do
        result[key] = value
    end
    return result
end

-- The settings that an effect shows, in order: the range, the magnitude, the duration and the area.
local function settingsOf(effect, constant)
    local record = core.magic.effects.records[effect.id]
    local result = { 'range' }
    if record.hasMagnitude then
        result[#result + 1] = 'magnitudeMin'
        result[#result + 1] = 'magnitudeMax'
    end
    if record.hasDuration and not constant then
        result[#result + 1] = 'duration'
    end
    if effect.range ~= RANGE.Self then
        result[#result + 1] = 'area'
    end
    return result
end

local SETTING_LIMITS = {
    magnitudeMin = { LIMITS.minMagnitude, LIMITS.maxMagnitude },
    magnitudeMax = { LIMITS.minMagnitude, LIMITS.maxMagnitude },
    duration = { LIMITS.minDuration, LIMITS.maxDuration },
    area = { LIMITS.minArea, LIMITS.maxArea },
}

-- Changes a setting by `delta`, as the effect editor's sliders do. Raising the minimum magnitude above the maximum
-- raises the maximum too, and the maximum can't go below the minimum. Returns whether anything changed.
local function adjustSetting(effect, setting, delta, constant)
    if setting == 'range' then
        local record = core.magic.effects.records[effect.id]
        local range = effect.range
        for _ = 1, 3 do
            range = (range + 1) % 3
            if isRangeAllowed(record, range, constant) then
                break
            end
        end
        if range == effect.range then
            return false
        end
        effect.range = range
        if range == RANGE.Self then
            effect.area = LIMITS.minArea
        end
        return true
    end
    local limits = SETTING_LIMITS[setting]
    local low = limits[1]
    if setting == 'magnitudeMax' then
        low = effect.magnitudeMin
    end
    local value = math.max(low, math.min(limits[2], effect[setting] + delta))
    if value == effect[setting] then
        return false
    end
    effect[setting] = value
    if setting == 'magnitudeMin' and effect.magnitudeMax < value then
        effect.magnitudeMax = value
    end
    return true
end

local function targetsOne(record)
    return record.hasSkill or record.hasAttribute
end

local function effectRecord(effect)
    return core.magic.effects.records[effect.id]
end

-- An effect written out, such as "Fire Damage 5 to 10 pts for 3 secs on Target". Plain text.
local function describe(effect, constant)
    return core.magic.effects.getDescription(effect, { isConstant = constant })
end

-- The name a new spell gets from its first effect, such as "Fortify Strength". Plain text.
local function effectName(effect)
    return core.magic.effects.getDescription(effect, { isConstant = true, noTarget = true, noMagnitude = true })
end

-- ---------------------------------------------------------------------------------------------------------------
-- The two kinds of screen

local element = nil
local state = nil

local KINDS = {}

KINDS.Enchanting = {
    window = 'EnchantingDialog',
    mode = 'Enchanting',
    title = 'EnchantTitle',
    placeholder = 'EnchantNewItem',
    effectsHelp = { 'sEnchantmentHelp10', 'Current effects on the item' },
}

KINDS.Spellmaking = {
    window = 'SpellCreationDialog',
    mode = 'SpellCreation',
    title = 'SpellmakingTitle',
    placeholder = 'SpellmakingNewSpell',
    effectsHelp = { 'sSpellmakingHelp6', 'Current effects on the spell' },
}

local function enchanting()
    return state.kind == KINDS.Enchanting
end

local function selfEnchanting()
    return enchanting() and state.merchant == nil
end

-- The effects that the numbers count: the ones added, with the one being edited in its place or at the end.
local function countedEffects()
    local effects = {}
    for i, effect in ipairs(state.effects) do
        effects[i] = effect
    end
    if state.editing then
        effects[state.editing.index or (#effects + 1)] = state.editing.effect
    end
    return effects
end

local function enchantingSetup(effects, castType)
    return {
        item = state.item,
        soulGem = state.soulGem,
        effects = effects,
        type = castType,
        enchanter = state.merchant,
        name = state.typedName,
    }
end

local newName

-- Works out the numbers, and the cast type of an enchantment. A constant effect works on self only, so switching to
-- one turns every effect to self, as the engine's window does.
local function readInfo()
    state.info = nil
    if not enchanting() then
        local ok, result = pcall(types.Player.getSpellmakingInfo, self,
            { merchant = state.merchant, effects = countedEffects(), name = newName() })
        if ok then
            state.info = result
        else
            print('Modern Morrow: ' .. tostring(result))
        end
        return
    end
    local ok, base = pcall(types.Player.getEnchantingInfo, self, enchantingSetup({}, nil))
    if not ok then
        print('Modern Morrow: ' .. tostring(base))
        return
    end
    local allowed = false
    for _, castType in ipairs(base.types) do
        if castType == state.castType then
            allowed = true
        end
    end
    if not allowed then
        state.castType = #base.types > 0 and base.type or nil
    end
    state.constant = state.castType == ENCHANTMENT_TYPE.ConstantEffect
    if state.constant then
        for _, effect in ipairs(countedEffects()) do
            effect.range = RANGE.Self
            effect.area = LIMITS.minArea
        end
    end
    local result
    ok, result = pcall(types.Player.getEnchantingInfo, self, enchantingSetup(countedEffects(), state.castType))
    if ok then
        state.info = result
    else
        print('Modern Morrow: ' .. tostring(result))
        state.info = base
    end
end

-- The name of the new item or spell, or nil before there is one. Plain text.
function newName()
    if state.typedName then
        return state.typedName
    end
    if enchanting() then
        return state.item and info.name(state.item)
    end
    if state.name == nil and #state.effects > 0 then
        -- A new spell is named after its first effect, so it can be bought without typing a name.
        state.name = effectName(state.effects[1])
    end
    return state.name
end

-- ---------------------------------------------------------------------------------------------------------------
-- The lines on the right

local function lines()
    local result = {}
    if enchanting() then
        result = { { kind = 'item' }, { kind = 'soulGem' }, { kind = 'castType' } }
    end
    for i = 1, #state.effects do
        result[#result + 1] = { kind = 'effect', index = i }
    end
    if #state.effects < LIMITS.maxEffects then
        result[#result + 1] = { kind = 'add' }
    end
    return result
end

-- The first of the item and the soul gem that is still missing, or else adding an effect.
local function focusNextLine()
    local all = lines()
    for i, line in ipairs(all) do
        if (line.kind == 'item' and state.item == nil) or (line.kind == 'soulGem' and state.soulGem == nil)
                or line.kind == 'add' then
            state.lineFocus = i
            return
        end
    end
    state.lineFocus = #all
end

-- ---------------------------------------------------------------------------------------------------------------
-- The list on the left

local function isUsable(item)
    return item ~= nil and item:isValid() and item.count > 0 and item.parentContainer == self.object
end

local function canEnchant(item)
    if info.enchantment(item) ~= nil then
        return false
    end
    if item.type == types.Book then
        return types.Book.record(item).isScroll
    end
    return item.type == types.Weapon or item.type == types.Armor or item.type == types.Clothing
end

local function soulOf(item)
    local data = types.Item.itemData(item)
    local soul = data and data.soul
    return soul and soul ~= '' and types.Creature.records[soul] or nil
end

local function pickerRows()
    local rows = {}
    local picker = state.picker
    if picker == 'effects' then
        -- The effects the player knows from their spells, grouped by school.
        local known = enchanting() and types.Player.getEnchantingEffects(self) or types.Player.getSpellmakingEffects(self)
        local groups = {}
        local schools = {}
        for _, record in ipairs(known) do
            local school = record.school and core.stats.Skill.records[record.school]
            local name = school and school.name or ''
            if groups[name] == nil then
                groups[name] = {}
                schools[#schools + 1] = name
            end
            local added = false
            if not targetsOne(record) then
                for _, effect in ipairs(state.effects) do
                    if effect.id == record.id then
                        added = true
                    end
                end
            end
            table.insert(groups[name], {
                record = record,
                icon = castables.bigIcon(record),
                name = record.name,
                detail = allowedRanges(record, state.constant),
                added = added,
            })
        end
        table.sort(schools)
        for _, school in ipairs(schools) do
            rows[#rows + 1] = { header = text.escape(school) }
            table.sort(groups[school], function(a, b)
                return a.name < b.name
            end)
            for _, row in ipairs(groups[school]) do
                rows[#rows + 1] = row
            end
        end
    elseif picker == 'items' then
        for _, item in ipairs(types.Actor.inventory(self):getAll()) do
            if item.count > 0 and canEnchant(item) then
                -- How much enchantment the item holds, as the engine counts it.
                local ok, itemInfo = pcall(types.Player.getEnchantingInfo, self, { item = item })
                rows[#rows + 1] = {
                    item = item,
                    name = info.name(item),
                    detail = L('EnchantCapacity') .. ' ' .. (ok and math.floor(itemInfo.maxPoints) or 0),
                    added = item == state.item,
                }
            end
        end
        table.sort(rows, function(a, b)
            return a.name < b.name
        end)
    elseif picker == 'soulGems' then
        for _, item in ipairs(types.Actor.inventory(self):getAll(types.Miscellaneous)) do
            local soul = info.isSoulGem(item) and soulOf(item)
            if soul then
                rows[#rows + 1] = {
                    item = item,
                    name = info.name(item),
                    detail = text.escape(soul.name) .. ',  ' .. L('EnchantSoulStrength') .. ' ' .. soul.soulValue,
                    added = item == state.soulGem,
                }
            end
        end
    elseif picker == 'skills' then
        for _, skill in ipairs(core.stats.Skill.records) do
            rows[#rows + 1] = { id = skill.id, icon = skill.icon, name = skill.name }
        end
    elseif picker == 'attributes' then
        for _, attribute in ipairs(core.stats.Attribute.records) do
            rows[#rows + 1] = { id = attribute.id, icon = attribute.icon, name = attribute.name }
        end
    end
    return rows
end

local function setPicker(picker)
    state.picker = picker
    state.list:reset()
end

-- ---------------------------------------------------------------------------------------------------------------
-- Drawing

local function drawPickerEntry(canvas, x, y, w, h, row, isFocused)
    local iconSize = ENTRY_HEIGHT - 16
    if row.item then
        canvas:icon(x + 10, y + 7, iconSize, row.item, false)
    elseif row.icon then
        canvas:picture(x + 10, y + 7, iconSize, row.icon)
    end
    local textX = x + 10 + iconSize + 12
    local textW = x + w - 12 - textX
    local nameColor = isFocused and color.active or (row.added and color.new or color.bright)
    if row.detail then
        canvas:text(textX, y + 2, textW, 26, text.fit(row.name, textW, size.normal), size.normal, nameColor)
        canvas:text(textX, y + 26, textW, 22, row.detail, size.small, color.dim)
    else
        canvas:text(textX, y, textW, ENTRY_HEIGHT - 2, text.fit(row.name, textW, size.normal), size.normal,
            nameColor)
    end
end

local PICKER_TEXT = {
    effects = { title = function() return gmst('sMagicEffects', 'Magic Effects') end, empty = 'EnchantNoEffects' },
    items = { title = function() return gmst('sItem', 'Item') end, empty = 'EnchantNoItems' },
    soulGems = { title = function() return gmst('sSoulGem', 'Soul Gem') end, empty = 'EnchantNoSoulGems' },
    skills = { title = function() return L('EnchantWhichSkill') end },
    attributes = { title = function() return L('EnchantWhichAttribute') end },
}

local function drawList(canvas, x, y, w, h)
    screen.box(canvas, x, y, w, h)
    local picker = PICKER_TEXT[state.picker]
    canvas:text(x + 16, y + 8, w - 32, 30, picker.title(), size.large, color.bright)
    local rows = state.rows
    if state.list:itemCount(rows) == 0 then
        local empty = picker.empty or 'EnchantNoEffects'
        if state.picker == 'effects' and not enchanting() then
            empty = 'SpellmakingNoEffects'
        end
        screen.lines(canvas, x + 16, y + 44, w - 32, l10n(empty), size.normal, color.dim, 3, 26)
        return
    end
    screen.drawRows(canvas, x + 4, y + 44, w - 8, y + h - 6, rows, state.list, ENTRY_HEIGHT, drawPickerEntry,
        state.side == 'list')
end

local SETTING_LABELS = {
    range = function() return gmst('sRange', 'Range') end,
    magnitudeMin = function() return gmst('sMagnitude', 'Magnitude') end,
    magnitudeMax = function() return gmst('sMagnitude', 'Magnitude') .. ' ' .. gmst('sTo', 'to') end,
    duration = function() return gmst('sDuration', 'Duration') end,
    area = function() return gmst('sArea', 'Area') end,
}

local SETTING_HELP = {
    range = { 'sRangeDes', '' },
    magnitudeMin = { 'sMagnitudeDes', '' },
    magnitudeMax = { 'sMagnitudeDes', '' },
    duration = { 'sDurationDes', '' },
    area = { 'sAreaDes', '' },
}

local function drawEditor(canvas, x, y, w, h)
    local effect = state.editing.effect
    local record = effectRecord(effect)
    screen.box(canvas, x, y, w, h, true)

    -- The effect, and the skill or attribute it works on.
    local icon = castables.bigIcon(record)
    if icon then
        canvas:picture(x + 16, y + 14, 48, icon)
    end
    local name = record.name
    if effect.affectedSkill then
        name = name .. ': ' .. core.stats.Skill.records[effect.affectedSkill].name
    elseif effect.affectedAttribute then
        name = name .. ': ' .. core.stats.Attribute.records[effect.affectedAttribute].name
    end
    canvas:text(x + 76, y + 14, w - 92, 48, text.fit(name, w - 92, size.large), size.large, color.bright)

    -- Each setting, with its value and a bar for where the value sits in its range.
    local all = settingsOf(effect, state.constant)
    state.settingFocus = math.max(1, math.min(#all, state.settingFocus))
    local ry = y + 80
    for i, setting in ipairs(all) do
        local isFocused = i == state.settingFocus
        if isFocused then
            canvas:rect(x + 4, ry, w - 8, SETTING_HEIGHT - 4, color.accent, 0.16)
            canvas:rect(x + 4, ry, 3, SETTING_HEIGHT - 4, color.accent)
        end
        local value = setting == 'range' and rangeName(effect.range) or tostring(effect[setting])
        canvas:text(x + 20, ry + 2, w - 40, 28, SETTING_LABELS[setting](), size.normal,
            isFocused and color.active or color.text)
        canvas:text(x + 20, ry + 2, w - 40, 28, value, size.large, color.bright, ui.ALIGNMENT.End)
        local limits = SETTING_LIMITS[setting]
        if limits then
            local fraction = (effect[setting] - limits[1]) / (limits[2] - limits[1])
            canvas:rect(x + 20, ry + 36, w - 40, 6, color.black, 0.6)
            canvas:rect(x + 20, ry + 36, math.max(2, math.floor((w - 40) * fraction)), 6, color.accent, 0.8)
        end
        ry = ry + SETTING_HEIGHT
    end

    local list_ = { { 'dpad', L('EnchantChange') }, { 'a', L('OK') } }
    if state.editing.index then
        list_[#list_ + 1] = { 'x', gmst('sDelete', 'Delete') }
    end
    local hintsTop = hints.draw(canvas, x + 16, w - 32, y + h, list_)

    -- What the highlighted setting does, in the game's own words, and how to change it faster.
    local setting = all[state.settingFocus]
    local help = SETTING_HELP[setting]
    local paragraphs = { gmstRaw(help[1], help[2]) }
    if setting ~= 'range' then
        paragraphs[2] = l10n('EnchantFaster')
    end
    local lineCount = 0
    for _, paragraph in ipairs(paragraphs) do
        lineCount = lineCount + screen.lineCount(paragraph, w - 32, size.small, 3) + 1
    end
    local hy = hintsTop - 12 - (lineCount - 1) * 21
    for _, paragraph in ipairs(paragraphs) do
        hy = screen.lines(canvas, x + 16, hy, w - 32, paragraph, size.small, color.dim, 3, 21) + 21
    end
end

-- The help for the highlighted line on the right, in the game's words where it has them.
local function lineHelp(line)
    if line.kind == 'item' then
        return gmstRaw('sEnchantmentHelp1', '')
    elseif line.kind == 'soulGem' then
        return gmstRaw('sEnchantmentHelp2', '')
    elseif line.kind == 'castType' then
        return l10n('EnchantCastTypeHelp')
    end
    return gmstRaw(state.kind.effectsHelp[1], state.kind.effectsHelp[2])
end

local function drawSummary(canvas, x, y, w, h)
    screen.box(canvas, x, y, w, h, true)

    -- The name of the new item or spell.
    local name = newName()
    canvas:text(x + 16, y + 10, w - 32, 34, name and text.fit(name, w - 32, size.large) or L(state.kind.placeholder),
        size.large, name and color.bright or color.dim)

    local all = lines()
    state.lineFocus = math.max(1, math.min(#all, state.lineFocus))
    local rightFocused = state.side == 'lines' and state.editing == nil
    local ry = y + 52
    local function highlight(i, height)
        local isFocused = rightFocused and i == state.lineFocus
        if isFocused then
            canvas:rect(x + 4, ry, w - 8, height - 2, color.accent, 0.16)
            canvas:rect(x + 4, ry, 3, height - 2, color.accent)
        end
        return isFocused
    end
    local labelW = 130
    local valueX = x + 16 + labelW
    local valueW = x + w - 16 - valueX
    local effectsHeading = false
    local shownEffects = countedEffects()
    for i, line in ipairs(all) do
        if line.kind == 'effect' or line.kind == 'add' then
            if not effectsHeading then
                effectsHeading = true
                ry = ry + 6
                canvas:text(x + 16, ry, w - 32, 22, gmst('sEffects', 'Effects'), size.small, color.dim)
                ry = ry + 24
            end
            if ry + EFFECT_LINE_HEIGHT > y + h - 190 then
                break
            end
            local isFocused = highlight(i, EFFECT_LINE_HEIGHT)
            if line.kind == 'add' then
                canvas:text(x + 20, ry, w - 40, EFFECT_LINE_HEIGHT - 2, '+  ' .. L('EnchantAddEffect'), size.normal,
                    isFocused and color.active or color.text)
            else
                local effect = shownEffects[line.index]
                local icon = castables.bigIcon(effectRecord(effect))
                if icon then
                    canvas:picture(x + 20, ry + 3, EFFECT_LINE_HEIGHT - 6, icon)
                end
                local textW = w - 44 - EFFECT_LINE_HEIGHT
                canvas:text(x + 20 + EFFECT_LINE_HEIGHT + 4, ry, textW, EFFECT_LINE_HEIGHT - 2,
                    text.fit(describe(effect, state.constant), textW, size.normal), size.normal,
                    isFocused and color.active or color.new)
            end
            ry = ry + EFFECT_LINE_HEIGHT
        else
            local isFocused = highlight(i, LINE_HEIGHT)
            local labelColor = isFocused and color.active or color.text
            if line.kind == 'item' or line.kind == 'soulGem' then
                local isItem = line.kind == 'item'
                local object = state.soulGem
                if isItem then
                    object = state.item
                end
                canvas:text(x + 20, ry, labelW, LINE_HEIGHT - 2,
                    isItem and gmst('sItem', 'Item') or gmst('sSoulGem', 'Soul Gem'), size.normal, labelColor)
                if object == nil then
                    canvas:text(valueX, ry, valueW, LINE_HEIGHT - 2,
                        L(isItem and 'EnchantChooseItem' or 'EnchantChooseSoulGem'), size.normal, color.dim)
                else
                    canvas:icon(valueX, ry + 4, LINE_HEIGHT - 10, object, false)
                    local caption = info.name(object)
                    local soul = not isItem and soulOf(object)
                    if soul then
                        caption = caption .. ' (' .. soul.name .. ')'
                    end
                    local textW = valueW - LINE_HEIGHT + 4
                    canvas:text(valueX + LINE_HEIGHT - 4, ry, textW, LINE_HEIGHT - 2,
                        text.fit(caption, textW, size.normal), size.normal, color.bright)
                end
            else
                canvas:text(x + 20, ry, labelW, LINE_HEIGHT - 2, L('EnchantType'), size.normal, labelColor)
                canvas:text(valueX, ry, valueW, LINE_HEIGHT - 2,
                    state.castType and castTypeName(state.castType) or '-', size.normal,
                    state.castType and color.bright or color.dim)
            end
            ry = ry + LINE_HEIGHT
        end
    end

    -- From the bottom up: the button hints, what the highlighted line is for, and the numbers.
    local list_ = {}
    local line = all[state.lineFocus]
    if rightFocused and line then
        if line.kind == 'item' or line.kind == 'soulGem' or line.kind == 'add' then
            list_[#list_ + 1] = { 'a', L('EnchantChoose') }
        elseif line.kind == 'castType' then
            if state.info and #state.info.types > 1 then
                list_[#list_ + 1] = { 'a', L('EnchantChange') }
            end
        else
            list_[#list_ + 1] = { 'a', L('EnchantEdit') }
        end
    elseif state.side == 'list' and state.editing == nil and state.list:focused(state.rows) then
        list_[#list_ + 1] = { 'a', L(state.picker == 'effects' and 'EnchantAdd' or 'EnchantChoose') }
    end
    if state.editing == nil then
        list_[#list_ + 1] = { 'x', L(selfEnchanting() and 'Create' or 'Buy') }
        if newName() then
            list_[#list_ + 1] = { 'ls', L('Rename') }
        end
    end
    local by = hints.draw(canvas, x + 16, w - 32, y + h, list_) - 10

    if rightFocused and line then
        local help = lineHelp(line)
        by = by - screen.lineCount(help, w - 32, size.small, 3) * 21
        screen.lines(canvas, x + 16, by, w - 32, help, size.small, color.dim, 3, 21)
        by = by - 10
    end

    -- The numbers, in two columns.
    local facts = {}
    local numbers = state.info
    local gold = playerGold()
    if not enchanting() then
        local price = numbers and numbers.price or 0
        facts[#facts + 1] = { gmst('sEnchantmentMenu4', 'Point Cost'), tostring(numbers and numbers.cost or 0) }
        facts[#facts + 1] = { gmst('sSpellmakingMenu1', 'Spell Chance'),
            math.floor(numbers and numbers.chance or 0) .. '%' }
        facts[#facts + 1] = { gmst('sBarterDialog7', 'Price'), price .. ' ' .. L('BarterGoldShort'),
            price > gold and color.worse or nil }
    else
        local points = numbers and math.floor(numbers.points) or 0
        local maxPoints = numbers and math.floor(numbers.maxPoints) or 0
        facts[#facts + 1] = { L('EnchantPoints'), points .. ' / ' .. maxPoints,
            points > maxPoints and color.worse or nil }
        facts[#facts + 1] = { gmst('sCastCost', 'Cast Cost'), tostring(numbers and math.floor(numbers.castCost) or 0) }
        facts[#facts + 1] = { L('MagicCharge'), tostring(numbers and math.floor(numbers.charge) or 0) }
        if not selfEnchanting() then
            local price = numbers and numbers.price or 0
            facts[#facts + 1] = { gmst('sBarterDialog7', 'Price'), price .. ' ' .. L('BarterGoldShort'),
                price > gold and color.worse or nil }
        elseif numbers and numbers.showChance then
            -- The player's own chance shows when the game's "show enchant chance" setting is on.
            facts[#facts + 1] = { L('MagicChance'), math.floor(numbers.chance) .. '%' }
        end
    end
    local rows = math.ceil(#facts / 2)
    by = by - rows * 28
    local columnW = math.floor((w - 32 - 24) / 2)
    for i, fact in ipairs(facts) do
        local fx = x + 16 + ((i - 1) % 2) * (columnW + 24)
        local fy = by + math.floor((i - 1) / 2) * 28
        canvas:text(fx, fy, columnW, 26, fact[1], size.normal, color.text)
        canvas:text(fx, fy, columnW, 26, fact[2], size.normal, fact[3] or color.bright, ui.ALIGNMENT.End)
    end
    canvas:rect(x + 16, by - 8, w - 32, 1, color.accent, 0.2)
end

local function layout()
    -- Items and soul gems that left the inventory, such as a gem used up by a failed enchantment, leave their lines.
    if state.item and not isUsable(state.item) then
        state.item = nil
    end
    if state.soulGem and not isUsable(state.soulGem) then
        state.soulGem = nil
    end
    readInfo()
    state.rows = pickerRows()
    local canvas = draw.canvas()
    local status
    if selfEnchanting() then
        status = text.escape(core.stats.Skill.records.enchant.name) .. '  '
            .. math.floor(types.NPC.stats.skills.enchant(self).modified)
    elseif state.merchant then
        status = text.escape(info.name(state.merchant)) .. '      ' .. L('BarterYourGold') .. '  ' .. playerGold()
    end
    local g = screen.drawFrame(canvas, L(state.kind.title), status)
    local columns = screen.columns(g, 0.42)
    if state.editing then
        drawEditor(canvas, columns.leftX, g.top, columns.leftW, g.bottom - g.top)
    else
        drawList(canvas, columns.leftX, g.top, columns.leftW, g.bottom - g.top)
    end
    drawSummary(canvas, columns.rightX, g.top, columns.rightW, g.bottom - g.top)
    local root = canvas:layout({ size = layer.size(layer.menu) })
    root.layer = layer.menu
    return root
end

local function redraw()
    if element == nil then
        return
    end
    element.layout = layout()
    element:update()
end

-- ---------------------------------------------------------------------------------------------------------------
-- Actions

local function startEditing(effect, index)
    state.editing = { effect = effect, index = index, old = index and copyEffect(state.effects[index]) }
    state.settingFocus = 1
    click()
end

-- The same checks as the engine's windows make when an effect is picked.
local function addEffect(record)
    if #state.effects >= LIMITS.maxEffects then
        ui.showMessage(gmstRaw('sNotifyMessage28', 'You can only add eight effects to a spell.'))
        return
    end
    if not (isRangeAllowed(record, RANGE.Self, state.constant) or isRangeAllowed(record, RANGE.Touch, state.constant)
            or isRangeAllowed(record, RANGE.Target, state.constant)) then
        return
    end
    -- An effect on a skill or an attribute asks which one first, in the list.
    if targetsOne(record) then
        state.pending = record
        setPicker(record.hasSkill and 'skills' or 'attributes')
        click()
        return
    end
    for _, effect in ipairs(state.effects) do
        if effect.id == record.id then
            ui.showMessage(gmstRaw('sOnetypeEffectMessage', 'This effect has already been added.'))
            return
        end
    end
    startEditing(newEffect(record, state.constant), nil)
end

local function backToLines()
    setPicker('effects')
    state.side = 'lines'
    state.pending = nil
end

local function chooseEntry()
    local row = state.list:focused(state.rows)
    if row == nil then
        return
    end
    if state.picker == 'effects' then
        addEffect(row.record)
    elseif state.picker == 'items' then
        state.item = row.item
        state.typedName = nil
        state.castType = nil
        ambient.playSound(types.Item.getDownSound(row.item))
        backToLines()
        focusNextLine()
    elseif state.picker == 'soulGems' then
        state.soulGem = row.item
        ambient.playSound(types.Item.getDownSound(row.item))
        backToLines()
        focusNextLine()
    elseif state.pending then
        local effect = newEffect(state.pending, state.constant)
        if state.picker == 'skills' then
            effect.affectedSkill = row.id
        else
            effect.affectedAttribute = row.id
        end
        setPicker('effects')
        state.pending = nil
        startEditing(effect, nil)
    end
end

local function chooseLine()
    local line = lines()[state.lineFocus]
    if line == nil then
        return
    end
    if line.kind == 'item' then
        setPicker('items')
        state.side = 'list'
    elseif line.kind == 'soulGem' then
        setPicker('soulGems')
        state.side = 'list'
    elseif line.kind == 'castType' then
        local castTypes = state.info and state.info.types or {}
        if #castTypes < 2 then
            return
        end
        local index = 1
        for i, castType in ipairs(castTypes) do
            if castType == state.castType then
                index = i
            end
        end
        state.castType = castTypes[index % #castTypes + 1]
    elseif line.kind == 'effect' then
        startEditing(copyEffect(state.effects[line.index]), line.index)
        return
    elseif line.kind == 'add' then
        setPicker('effects')
        state.side = 'list'
    end
    click()
end

-- Back on the right side after an edit, on the effect just added or edited.
local function finishEditing(keep)
    local editing = state.editing
    state.editing = nil
    if keep then
        if editing.index then
            state.effects[editing.index] = editing.effect
        else
            state.effects[#state.effects + 1] = editing.effect
            editing.index = #state.effects
        end
    end
    state.side = 'lines'
    setPicker('effects')
    if keep then
        for i, line in ipairs(lines()) do
            if line.kind == 'effect' and line.index == editing.index then
                state.lineFocus = i
            end
        end
    end
end

local function deleteEdited()
    local index = state.editing.index
    state.editing = nil
    table.remove(state.effects, index)
    if index == 1 and not enchanting() then
        state.name = nil
    end
    state.side = 'lines'
    setPicker('effects')
    click()
end

-- A setting changes faster the longer left or right is held.
local function adjustStep(dx)
    local now = core.getRealTime()
    if state.heldDirection ~= dx or now - state.lastAdjust > 0.4 then
        state.heldDirection = dx
        state.heldSince = now
    end
    state.lastAdjust = now
    local held = now - state.heldSince
    if held > 3 then
        return 10
    elseif held > 1.2 then
        return 5
    end
    return 1
end

local function adjust(delta)
    local all = settingsOf(state.editing.effect, state.constant)
    local setting = all[state.settingFocus]
    if setting and adjustSetting(state.editing.effect, setting, delta, state.constant) then
        click()
        redraw()
    end
end

-- The messages of the enchanting window when it refuses, by the problem that the engine names.
local ENCHANTING_PROBLEMS = {
    noEffects = { 'sEnchantmentMenu11', 'You must add at least one effect to an enchantment' },
    noName = { 'sNotifyMessage10', 'You must give the item a name.' },
    noSoulGem = { 'sNotifyMessage52', 'You must have a soul gem with a soul to make an Enchanted Item' },
    noItem = { 'sNotifyMessage11', 'You have to have an item to enchant.' },
    tooManyPoints = { 'sNotifyMessage29', 'You cannot add more enchantment to an object then it can hold.' },
    notEnoughGold = { 'sNotifyMessage18', "You don't have enough gold to buy this spell." },
}

-- The messages of the spellmaking window when it refuses.
local SPELLMAKING_PROBLEMS = {
    noEffects = { 'sNotifyMessage30', 'You have to add at least one effect to a spell.' },
    noName = { 'sNotifyMessage10', 'You must give the spell a name.' },
    noCost = { 'sEnchantmentMenu8', 'You cannot buy a spell that has a zero point cost.' },
    notEnoughGold = { 'sNotifyMessage18', "You don't have enough gold to buy this spell." },
}

-- Shows the window's message when the engine would refuse. Returns whether it refuses.
local function refuses(messages)
    local problem = state.info and state.info.problem
    if problem == nil then
        return state.info == nil
    end
    -- The new item takes the old item's name, and the keyboard keeps a name from being empty, so a missing name means
    -- that the item is missing. The engine's window names the missing name, which would only confuse here.
    if problem == 'noName' and enchanting() and state.item == nil then
        problem = 'noItem'
    end
    local message = messages[problem]
    if message then
        ui.showMessage(gmstRaw(message[1], message[2]))
    end
    return true
end

local function enchant()
    if refuses(ENCHANTING_PROBLEMS) then
        return
    end
    local setup = enchantingSetup(state.effects, state.castType)
    -- The enchanter takes back what the player stole from them, and the conversation ends, as in the engine's window.
    -- The engine shows the message.
    if state.info.stolenItem then
        core.sendGlobalEvent('ModernMorrowEnchant', { player = self.object, setup = setup })
        I.UI.removeMode(state.kind.mode)
        if types.Player.getDialogueActor(self) ~= nil then
            pcall(types.Player.sayGoodbye, self)
        end
        return
    end
    state.waiting = true
    core.sendGlobalEvent('ModernMorrowEnchant', { player = self.object, setup = setup })
end

-- The engine plays the sound when the spell is made.
local function makeSpell()
    if refuses(SPELLMAKING_PROBLEMS) then
        return
    end
    core.sendGlobalEvent('ModernMorrowMakeSpell', {
        player = self.object,
        setup = { merchant = state.merchant, effects = state.effects, name = newName() },
    })
    I.UI.removeMode(state.kind.mode)
end

local function close()
    I.UI.removeMode(state.kind.mode)
end

-- Names the item or the spell with the on-screen keyboard. An empty name keeps the one before.
local function rename()
    local current = newName()
    if current == nil then
        return
    end
    click()
    keyboard.open({
        label = gmstRaw('sName', 'Name'),
        text = current,
        onDone = function(typed)
            if state then
                state.typedName = typed
                redraw()
            end
        end,
    })
end

local function editorButton(name)
    if name == 'a' then
        click()
        finishEditing(true)
    elseif name == 'b' then
        finishEditing(false)
    elseif name == 'x' then
        if state.editing.index then
            deleteEdited()
        end
    elseif name == 'lb' then
        adjust(-10)
        return
    elseif name == 'rb' then
        adjust(10)
        return
    else
        return
    end
    redraw()
end

effectcraft.handler = {
    onButton = function(name)
        if state == nil or state.waiting then
            return
        end
        if state.editing then
            editorButton(name)
            return
        end
        if name == 'a' then
            if state.side == 'list' then
                chooseEntry()
            else
                chooseLine()
            end
        elseif name == 'b' then
            if state.side == 'list' and state.picker ~= 'effects' then
                backToLines()
            elseif state.side == 'list' then
                state.side = 'lines'
            else
                close()
                return
            end
        elseif name == 'x' then
            if enchanting() then
                enchant()
            else
                makeSpell()
            end
            if state == nil then
                return
            end
        elseif name == 'ls' then
            rename()
            return
        else
            return
        end
        redraw()
    end,
    onMove = function(dx, dy)
        if state == nil or state.waiting then
            return
        end
        if state.editing then
            if dy ~= 0 then
                local count = #settingsOf(state.editing.effect, state.constant)
                state.settingFocus = math.max(1, math.min(count, state.settingFocus + dy))
                state.heldDirection = nil
            elseif dx ~= 0 then
                adjust(dx * adjustStep(dx))
                return
            end
        elseif dx < 0 and state.side == 'lines' then
            state.side = 'list'
        elseif dx > 0 and state.side == 'list' then
            state.side = 'lines'
        elseif dy ~= 0 and state.side == 'list' then
            state.list:move(state.rows, dy)
        elseif dy ~= 0 then
            state.lineFocus = math.max(1, math.min(#lines(), state.lineFocus + dy))
        else
            return
        end
        redraw()
    end,
}

-- From the global script, after enchanting. The engine played the sound and showed the message.
function effectcraft.onEnchantResult(enchanted)
    if state == nil or not enchanting() then
        return
    end
    state.waiting = false
    if enchanted then
        close()
        return
    end
    -- Event handlers may run on the Lua worker thread, which can't measure text, so the next frame redraws.
    state.dirty = true
end

-- ---------------------------------------------------------------------------------------------------------------
-- The screen

local function show(kind, target)
    state = {
        kind = kind,
        effects = {},
        side = 'lines',
        picker = 'effects',
        list = list.new(),
        rows = {},
        lineFocus = 1,
        settingFocus = 1,
        lastAdjust = 0,
        heldSince = 0,
        constant = false,
    }
    -- Enchanting opens with the soul gem the player used, or with the enchanter the player talks to.
    if target and types.Actor.objectIsInstance(target) then
        state.merchant = target
    elseif target and kind == KINDS.Enchanting then
        state.soulGem = target
    end
    focusNextLine()
    router.push(effectcraft.handler)
    if element then
        element:destroy()
    end
    element = ui.create(layout())
end

local function hide()
    keyboard.close()
    router.remove(effectcraft.handler)
    if element then
        element:destroy()
        element = nil
    end
    state = nil
end

local function signature()
    local parts = { tostring(playerGold()) }
    for _, item in ipairs(types.Actor.inventory(self):getAll(types.Miscellaneous)) do
        parts[#parts + 1] = item.id .. ':' .. item.count
    end
    return table.concat(parts, ',')
end

function effectcraft.onFrame()
    if element == nil or state == nil then
        return
    end
    local current = signature()
    if current ~= state.signature or state.dirty then
        state.signature = current
        state.dirty = false
        redraw()
    end
end

function effectcraft.register()
    for _, kind in pairs(KINDS) do
        I.UI.registerWindow(kind.window, function(target)
            show(kind, target)
        end, hide, { capturesInput = true })
    end
end

return effectcraft
