-- Replaces the Magic window: the player's powers, spells and magic items, with a detail view.

local core = require('openmw.core')
local self = require('openmw.self')
local types = require('openmw.types')
local ui = require('openmw.ui')
local util = require('openmw.util')
local I = require('openmw.interfaces')

local style = require('scripts.uiproof.style')
local list = require('scripts.uiproof.list')

local magic = {}

local WIDTH = 720
local LIST_WIDTH = WIDTH - 40
local ROWS = 16
local COLUMNS = { { width = 110 }, { width = 110 } }

local element = nil
local rows = list.new(ROWS, 28)
local detail = nil

local function gmst(name, fallback)
    return core.getGMST(name) or fallback
end

local function byName(a, b)
    return a.name < b.name
end

local function enchantedItems()
    local found = {}
    local inventory = types.Actor.inventory(self)
    for _, itemType in ipairs({ types.Weapon, types.Armor, types.Clothing, types.Book }) do
        for _, item in ipairs(inventory:getAll(itemType)) do
            local record = itemType.record(item)
            local enchantment = record.enchant and core.magic.enchantments.records[record.enchant]
            if enchantment and (enchantment.type == core.magic.ENCHANTMENT_TYPE.CastOnUse
                    or enchantment.type == core.magic.ENCHANTMENT_TYPE.CastOnce) then
                found[#found + 1] = { item = item, record = record, enchantment = enchantment, name = record.name }
            end
        end
    end
    table.sort(found, byName)
    return found
end

local function gatherItems()
    local selectedSpell = types.Actor.getSelectedSpell(self)
    local selectedItem = types.Actor.getSelectedEnchantedItem(self)
    local actorSpells = types.Actor.spells(self)
    local powers, spells = {}, {}
    for _, spell in ipairs(actorSpells) do
        if spell.type == core.magic.SPELL_TYPE.Power then
            powers[#powers + 1] = spell
        elseif spell.type == core.magic.SPELL_TYPE.Spell then
            spells[#spells + 1] = spell
        end
    end
    table.sort(powers, byName)
    table.sort(spells, byName)

    local items = {}
    local function mark(isSelected, name)
        return isSelected and ('* ' .. name) or name
    end
    if #powers > 0 then
        items[#items + 1] = { header = true, text = gmst('sPowers', 'Powers') }
        for _, spell in ipairs(powers) do
            items[#items + 1] = {
                key = 'spell:' .. spell.id,
                kind = 'spell',
                spell = spell,
                text = mark(selectedSpell ~= nil and selectedSpell.id == spell.id, spell.name),
                columns = { nil, actorSpells:canUsePower(spell) and '' or 'Used' },
            }
        end
    end
    if #spells > 0 then
        items[#items + 1] = { header = true, text = gmst('sSpells', 'Spells'), columns = { 'Cost', 'Chance' } }
        for _, spell in ipairs(spells) do
            items[#items + 1] = {
                key = 'spell:' .. spell.id,
                kind = 'spell',
                spell = spell,
                text = mark(selectedSpell ~= nil and selectedSpell.id == spell.id, spell.name),
                columns = {
                    tostring(core.magic.spells.getCost(spell)),
                    string.format('%d%%', math.floor(actorSpells:getSuccessChance(spell))),
                },
            }
        end
    end
    local magicItems = enchantedItems()
    if #magicItems > 0 then
        items[#items + 1] = { header = true, text = gmst('sMagicItem', 'Magic Items'), columns = { 'Cost', 'Charge' } }
        for _, entry in ipairs(magicItems) do
            local charge
            if entry.enchantment.type == core.magic.ENCHANTMENT_TYPE.CastOnce then
                charge = string.format('x%d', entry.item.count)
            else
                local current = types.Item.itemData(entry.item).enchantmentCharge or entry.enchantment.charge
                charge = string.format('%d/%d', math.floor(current), entry.enchantment.charge)
            end
            entry.kind = 'item'
            entry.key = 'item:' .. entry.item.id
            entry.text = mark(selectedItem ~= nil and selectedItem == entry.item, entry.name)
            entry.columns = {
                tostring(core.magic.enchantments.getEffectiveCastCost(entry.enchantment, self)),
                charge,
            }
            items[#items + 1] = entry
        end
    end
    return items
end

local function detailLayout(entry)
    local lines = {}
    local effects
    if entry.kind == 'spell' then
        effects = entry.spell.effects
        if entry.spell.type == core.magic.SPELL_TYPE.Spell then
            lines[#lines + 1] = string.format('%s: %d    %s: %d%%', gmst('sCastCost', 'Cast Cost'),
                core.magic.spells.getCost(entry.spell), 'Chance',
                math.floor(types.Actor.spells(self):getSuccessChance(entry.spell)))
        else
            lines[#lines + 1] = 'Power: can be used once a day'
        end
    else
        effects = entry.enchantment.effects
        lines[#lines + 1] = string.format('%s: %d    %s: %s', gmst('sCastCost', 'Cast Cost'),
            core.magic.enchantments.getEffectiveCastCost(entry.enchantment, self), gmst('sCharges', 'Charges'),
            entry.columns[2])
    end
    local content = {
        style.text(entry.spell and entry.spell.name or entry.name, {
            position = util.vector2(20, 60),
            textColor = style.header,
        }),
        style.text(lines[1], { position = util.vector2(20, 92), textColor = style.dim }),
    }
    for i, effect in ipairs(effects) do
        local record = core.magic.effects.records[effect.id]
        content[#content + 1] = style.text(core.magic.effects.getDescription(effect), {
            position = util.vector2(20, 120 + i * 30),
        })
        local school = record and record.school and core.stats.Skill.records[record.school]
        if school then
            content[#content + 1] = style.text(school.name, {
                position = util.vector2(WIDTH - 20, 120 + i * 30),
                anchor = util.vector2(1, 0),
                textColor = style.dim,
                textSize = style.smallTextSize,
            })
        end
    end
    return content
end

local function layout()
    local content = {
        style.text(gmst('sMagicMenu', 'Magic'), {
            position = util.vector2(WIDTH / 2, 14),
            anchor = util.vector2(0.5, 0),
            textSize = style.titleSize,
            textColor = style.header,
        }),
    }
    if detail then
        for _, child in ipairs(detailLayout(detail)) do
            content[#content + 1] = child
        end
    else
        local body = rows:layout(LIST_WIDTH, COLUMNS)
        body.props.position = util.vector2(20, 56)
        content[#content + 1] = body
        content[#content + 1] = style.text('X  Details', {
            position = util.vector2(WIDTH - 20, 56 + ROWS * 28 + 14),
            anchor = util.vector2(1, 0),
            textColor = style.dim,
            textSize = style.smallTextSize,
        })
    end
    return {
        layer = style.menuLayer,
        type = ui.TYPE.Widget,
        props = {
            relativePosition = util.vector2(0.5, 0.5),
            anchor = util.vector2(0.5, 0.5),
            size = util.vector2(WIDTH, 56 + ROWS * 28 + 50),
        },
        content = ui.content({ style.panel({ relativeSize = util.vector2(1, 1) }, content) }),
    }
end

local function redraw()
    if element then
        element.layout = layout()
        element:update()
    end
end

local function refresh()
    rows:setItems(gatherItems())
    redraw()
end

function magic.show()
    detail = nil
    rows:setItems(gatherItems())
    if element then
        element:destroy()
    end
    element = ui.create(layout())
end

function magic.hide()
    detail = nil
    if element then
        element:destroy()
        element = nil
    end
end

function magic.isVisible()
    return element ~= nil
end

local pendingRefresh = 0

local function choose(entry)
    if entry.kind == 'spell' then
        types.Actor.setSelectedSpell(self, entry.spell)
    else
        types.Actor.setSelectedEnchantedItem(self, entry.item)
    end
    -- The engine may make the selection at the end of the frame, so read it again a little later.
    pendingRefresh = 2
end

function magic.onAction(action)
    if detail then
        if action == 'back' then
            detail = nil
            redraw()
        elseif action == 'accept' then
            choose(detail)
            detail = nil
            redraw()
        end
        return
    end
    if action == 'up' then
        if rows:move(-1) then redraw() end
    elseif action == 'down' then
        if rows:move(1) then redraw() end
    elseif action == 'accept' then
        local entry = rows:current()
        if entry then
            choose(entry)
        end
    elseif action == 'detail' then
        detail = rows:current()
        if detail then redraw() end
    elseif action == 'back' then
        I.UI.removeMode(I.UI.MODE.Interface)
    end
end

function magic.onFrame()
    if pendingRefresh > 0 then
        pendingRefresh = pendingRefresh - 1
        if pendingRefresh == 0 and element then
            refresh()
        end
    end
end

return magic
