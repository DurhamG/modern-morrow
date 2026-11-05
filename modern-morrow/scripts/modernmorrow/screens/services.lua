-- The merchant services that a conversation opens: training, travel, buying spells and repairs. Each replaces the
-- engine's window for the service, and all of them are the fork's service panel: a list of what the merchant offers with
-- its price, and the highlighted entry's details beside it. Entries the player can't pick say why in red.
--
-- The prices and rules come from the engine's Lua API, and the global script buys the service. Training and travel end
-- the conversation, as the engine's windows do. Spells and repairs stay open for the next purchase.

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
local list = require('scripts.modernmorrow.ui.list')
local panel = require('scripts.modernmorrow.ui.panel')
local info = require('scripts.modernmorrow.items.info')
local castables = require('scripts.modernmorrow.magic.castables')
local router = require('scripts.modernmorrow.input.router')

local l10n = core.l10n('ModernMorrow')

local services = {}

local color = style.color
local size = style.textSize

local function L(key)
    return text.escape(l10n(key))
end

local function gmst(name, fallback)
    return core.getGMST(name) or fallback
end

local function playerGold()
    return types.Actor.inventory(self):countOf('gold_001')
end

local function actorRecord(actor)
    if types.NPC.objectIsInstance(actor) then
        return types.NPC.record(actor)
    end
    return types.Creature.record(actor)
end

-- Ends the conversation after a service that takes the player away from it, as the engine's windows do.
local function leaveConversation(mode)
    I.UI.removeMode(mode)
    if types.Player.getDialogueActor(self) ~= nil then
        pcall(types.Player.sayGoodbye, self)
    end
end

-- Each service has a window and its mode, a title, what A does, a caption for an empty list, and:
--   entries(merchant)       the entries, each with `name` (plain text), `price`, and optionally `subtitle` (escaped),
--                           `icon` or `item`, `facts` ({ label, value } pairs, escaped), `paragraphs` (plain text) and
--                           `unavailable` (an escaped reason why the player can't pick it)
--   choose(merchant, entry) buys it
--   signature(merchant)     a string that changes when the entries may have changed
local SERVICES = {}

SERVICES.Training = {
    window = 'Training',
    title = 'ServiceTraining',
    action = 'ServiceTrain',
    empty = 'ServiceNothingToLearn',
    entries = function(trainer)
        local gold = playerGold()
        local result = {}
        -- The trainer's values are the ones that training compares with the player's skill.
        local skills, trainerValues = types.NPC.getTrainingSkills(trainer)
        for i, skillId in ipairs(skills) do
            local skill = core.stats.Skill.records[skillId]
            local value = math.floor(types.NPC.stats.skills[skillId](self).base)
            local trainerValue = math.floor(trainerValues[i])
            local attribute = core.stats.Attribute.records[skill.attribute]
            local attributeValue = math.floor(types.Actor.stats.attributes[skill.attribute](self).modified)
            local entry = {
                name = skill.name,
                icon = skill.icon,
                price = types.NPC.getTrainingPrice(trainer, skillId),
                subtitle = value .. ' -> ' .. (value + 1),
                facts = {
                    { L('ServiceYourSkill'), tostring(value) },
                    { L('ServiceTrainerSkill'), tostring(trainerValue) },
                    { text.escape(attribute.name), tostring(attributeValue) },
                },
                paragraphs = { skill.description },
                skill = skillId,
            }
            local ok, reason = types.NPC.canTrain(trainer, skillId)
            if not ok then
                entry.unavailable = L(reason == 'attributeTooLow' and 'ServiceAttributeTooLow'
                    or 'ServiceTrainerNotBetter')
            elseif entry.price > gold then
                entry.unavailable = L('BarterNotEnoughGold')
            end
            result[#result + 1] = entry
        end
        return result
    end,
    choose = function(trainer, entry)
        -- The screen fades out and in while the two hours pass, as the engine's training window does.
        ui.fadeScreenOut(0.2)
        ui.fadeScreenIn(0.2, 0.2)
        leaveConversation('Training')
        core.sendGlobalEvent('ModernMorrowBuyService',
            { player = self.object, merchant = trainer, service = 'training', skill = entry.skill })
    end,
}

SERVICES.Travel = {
    window = 'Travel',
    title = 'ServiceTravel',
    action = 'Travel',
    empty = 'ServiceNoDestinations',
    entries = function(merchant)
        local gold = playerGold()
        local result = {}
        for i, destination in ipairs(actorRecord(merchant).travelDestinations) do
            local hours = types.Actor.getTravelHours(merchant, i)
            local time = L('ServiceInstant')
            if hours > 0 then
                time = hours .. ' ' .. L(hours == 1 and 'ServiceHour' or 'ServiceHours')
            end
            local entry = {
                name = destination.cellName,
                price = types.Actor.getTravelPrice(merchant, i),
                subtitle = time,
                facts = { { L('ServiceTravelTime'), time } },
                destination = i,
            }
            if entry.price > gold then
                entry.unavailable = L('BarterNotEnoughGold')
            end
            result[#result + 1] = entry
        end
        return result
    end,
    choose = function(merchant, entry)
        -- A guild guide, who works from an interior, teleports the player.
        if merchant.cell and not merchant.cell.isExterior then
            ambient.playSound('mysticism cast')
        end
        -- The screen goes black during the trip, and fades back in at the destination, as with the engine's travel
        -- window.
        ui.fadeScreenOut(0)
        ui.fadeScreenIn(1)
        leaveConversation('Travel')
        core.sendGlobalEvent('ModernMorrowBuyService',
            { player = self.object, merchant = merchant, service = 'travel', destination = entry.destination })
    end,
}

SERVICES.SpellBuying = {
    window = 'SpellBuying',
    title = 'ServiceSpells',
    action = 'Buy',
    empty = 'ServiceNoSpells',
    entries = function(merchant)
        local gold = playerGold()
        local spells = types.Actor.getSpellsForSale(merchant)
        local sorted = {}
        for _, spell in ipairs(spells) do
            sorted[#sorted + 1] = spell
        end
        table.sort(sorted, function(a, b)
            return a.name:lower() < b.name:lower()
        end)
        local actorSpells = types.Actor.spells(self)
        local result = {}
        for _, spell in ipairs(sorted) do
            local school = core.magic.spells.getSchool(spell, self)
            local schoolRecord = school and core.stats.Skill.records[school]
            local cost = core.magic.spells.getCost(spell)
            local first = spell.effects[1]
            local entry = {
                name = spell.name,
                icon = first and castables.bigIcon(first.effect),
                price = types.Actor.getSpellPrice(merchant, spell),
                subtitle = (schoolRecord and (text.escape(schoolRecord.name) .. ', ') or '') .. cost .. ' '
                    .. L('ServiceMagicka'),
                facts = {
                    { L('ServiceMagickaCost'), tostring(cost) },
                    { L('ServiceChance'), math.floor(actorSpells:getSuccessChance(spell)) .. '%' },
                },
                paragraphs = castables.describe(spell.effects),
                spell = spell.id,
            }
            if entry.price > gold then
                entry.unavailable = L('BarterNotEnoughGold')
            end
            result[#result + 1] = entry
        end
        return result
    end,
    choose = function(merchant, entry)
        ambient.playSound('Item Gold Up')
        core.sendGlobalEvent('ModernMorrowBuyService',
            { player = self.object, merchant = merchant, service = 'spell', spell = entry.spell })
    end,
    signature = function()
        local parts = {}
        for _, spell in ipairs(types.Actor.spells(self)) do
            parts[#parts + 1] = spell.id
        end
        return table.concat(parts, ',')
    end,
}

SERVICES.MerchantRepair = {
    window = 'MerchantRepair',
    title = 'ServiceRepair',
    action = 'Repair',
    empty = 'ServiceNothingToRepair',
    entries = function(merchant)
        local gold = playerGold()
        local result = {}
        local inventory = types.Actor.inventory(self)
        for _, itemType in ipairs({ types.Weapon, types.Armor }) do
            for _, item in ipairs(inventory:getAll(itemType)) do
                local health = itemType.record(item).health
                local condition = info.condition(item)
                -- The merchant repairs weapons and armor that have lost some condition.
                if health and health > 0 and condition and condition < health then
                    local ok, price = pcall(types.Actor.getRepairPrice, merchant, item)
                    if ok then
                        local conditionText = string.format('%d / %d', condition, health)
                        local entry = {
                            name = info.name(item),
                            item = item,
                            price = price,
                            subtitle = text.escape(gmst('sCondition', 'Condition')) .. ' ' .. conditionText,
                            facts = { { text.escape(gmst('sCondition', 'Condition')), conditionText } },
                        }
                        if price > gold then
                            entry.unavailable = L('BarterNotEnoughGold')
                        end
                        result[#result + 1] = entry
                    end
                end
            end
        end
        table.sort(result, function(a, b)
            return a.name < b.name
        end)
        return result
    end,
    choose = function(merchant, entry)
        ambient.playSound('Repair')
        core.sendGlobalEvent('ModernMorrowBuyService',
            { player = self.object, merchant = merchant, service = 'repair', item = entry.item })
    end,
}

-- ---------------------------------------------------------------------------------------------------------------
-- The panel

local element = nil
local state = nil

local function readEntries()
    state.entries = state.service.entries(state.merchant)
end

local function rows()
    local result = {}
    for i, entry in ipairs(state.entries) do
        result[i] = { entry = entry }
    end
    return result
end

local function focused()
    local row = state.list:focused(rows())
    return row and row.entry
end

local function drawEntry(canvas, x, y, w, h, row, isFocused)
    local entry = row.entry
    panel.drawEntry(canvas, x, y, w, h, isFocused, {
        item = entry.item,
        icon = entry.icon,
        name = text.escape(entry.name),
        second = entry.subtitle,
        topRight = entry.price .. ' ' .. L('BarterGoldShort'),
        topRightColor = entry.unavailable and color.worse or color.text,
        rightW = 90,
        dim = entry.unavailable ~= nil,
    })
end

local function drawDetails(canvas, g, entry)
    local x, y, w = g.detailsX, g.detailsY, g.detailsW
    local list_ = {}
    if entry and entry.unavailable == nil then
        list_[#list_ + 1] = { 'a', L(state.service.action) }
    end
    local hintsTop = hints.draw(canvas, x + 16, w - 32, g.bottom - 4, list_)
    if entry == nil then
        return
    end
    local innerX = x + 16
    local innerW = w - 32
    local ty = y + 14
    local titleX = innerX
    if entry.item then
        canvas:icon(innerX, ty + 2, 56, entry.item, false)
        titleX = innerX + 68
    elseif entry.icon then
        canvas:picture(innerX, ty + 2, 56, entry.icon)
        titleX = innerX + 68
    end
    local titleW = x + w - 16 - titleX
    for _, line in ipairs(text.wrap(entry.name, titleW, size.large, 2)) do
        canvas:text(titleX, ty, titleW, 30, line, size.large, color.bright)
        ty = ty + 28
    end
    if entry.subtitle then
        canvas:text(titleX, ty, titleW, 24, entry.subtitle, size.small, color.dim)
    end
    ty = math.max(ty + 34, y + 84)
    ty = panel.fact(canvas, x, ty, w, L('BarterBuyPrice'), tostring(entry.price),
        entry.price <= playerGold() and color.bright or color.worse)
    for _, fact in ipairs(entry.facts or {}) do
        ty = panel.fact(canvas, x, ty, w, fact[1], fact[2])
    end
    ty = ty + 6
    -- Why the entry can't be picked goes just above the hints.
    local bottom = entry.unavailable and hintsTop - 40 or hintsTop - 8
    for _, paragraph in ipairs(entry.paragraphs or {}) do
        ty = canvas:paragraph(innerX, ty, innerW, paragraph, size.normal, color.text, bottom) + 8
    end
    if entry.unavailable then
        canvas:text(innerX, hintsTop - 36, innerW, 28, entry.unavailable, size.normal, color.worse)
    end
end

local function layout()
    local canvas = draw.canvas()
    local g = panel.drawFrame(canvas, text.escape(info.name(state.merchant)),
        L('BarterYourGold') .. '  ' .. playerGold())
    panel.drawSubtitle(canvas, g, L(state.service.title), color.text)
    panel.drawList(canvas, g, rows(), state.list, drawEntry, L(state.service.empty))
    drawDetails(canvas, g, focused())
    local root = canvas:layout({ size = layer.size(layer.menu) })
    root.layer = layer.menu
    return root
end

local function redraw()
    if element == nil then
        return
    end
    readEntries()
    element.layout = layout()
    element:update()
end

local function signature()
    local parts = { tostring(playerGold()) }
    for _, item in ipairs(types.Actor.inventory(self):getAll()) do
        local condition = info.condition(item)
        parts[#parts + 1] = item.id .. ':' .. item.count .. ':' .. tostring(condition)
    end
    if state.service.signature then
        parts[#parts + 1] = state.service.signature(state.merchant)
    end
    return table.concat(parts, ',')
end

services.handler = {
    onButton = function(name)
        if state == nil then
            return
        end
        if name == 'a' then
            local entry = focused()
            if entry and entry.unavailable == nil then
                state.service.choose(state.merchant, entry)
            end
        elseif name == 'b' then
            I.UI.removeMode(state.service.window)
        end
    end,
    onMove = function(_, dy)
        if state == nil or dy == 0 then
            return
        end
        if state.list:move(rows(), dy) then
            redraw()
        end
    end,
}

local function show(service, merchant)
    if merchant == nil then
        return
    end
    state = { service = service, merchant = merchant, list = list.new(), entries = {} }
    readEntries()
    state.signature = signature()
    router.push(services.handler)
    if element then
        element:destroy()
    end
    element = ui.create(layout())
end

local function hide()
    router.remove(services.handler)
    if element then
        element:destroy()
        element = nil
    end
    state = nil
end

-- Redraws after a purchase, which changes the player's gold, spells or the condition of their items.
function services.onFrame()
    if element == nil or state == nil then
        return
    end
    local current = signature()
    if current ~= state.signature then
        state.signature = current
        redraw()
    end
end

function services.register()
    for _, service in pairs(SERVICES) do
        I.UI.registerWindow(service.window, function(merchant)
            show(service, merchant)
        end, hide, { capturesInput = true })
    end
end

return services
