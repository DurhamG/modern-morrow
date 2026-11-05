-- The companion screen, which replaces the engine's Companion window, where the player shares items with a companion.
-- It has the loot screen's look, with tabs for the companion's items and the player's. A companion who works for pay
-- keeps count of the profit, as the engine's window shows, and warns when the player leaves with the count below zero.

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
local confirm = require('scripts.modernmorrow.ui.confirm')
local info = require('scripts.modernmorrow.items.info')
local details = require('scripts.modernmorrow.items.details')
local router = require('scripts.modernmorrow.input.router')

local l10n = core.l10n('ModernMorrow')

local companion = { window = 'Companion', mode = 'Companion' }

local color = style.color

local function L(key)
    return text.escape(l10n(key))
end

local function gmst(name, fallback)
    return core.getGMST(name) or fallback
end

local element = nil
local state = nil

local function isOpen()
    local i = 1
    while I.UI.modes[i] do
        if I.UI.modes[i] == companion.mode then
            return true
        end
        i = i + 1
    end
    return false
end

local function owner()
    return state.yours and self.object or state.actor
end

local function buildRows()
    local equipped = {}
    for _, item in pairs(types.Actor.getEquipment(owner())) do
        equipped[item.id] = true
    end
    local entries = {}
    for _, item in ipairs(types.Actor.inventory(owner()):getAll()) do
        if item.count > 0 then
            entries[#entries + 1] = { item = item, equipped = equipped[item.id] == true }
        end
    end
    return panel.groupedRows(entries)
end

local function currentList()
    return state.yours and state.yourList or state.theirList
end

local function focusedEntry(rows)
    local row = currentList():focused(rows or buildRows())
    return row and row.entry
end

local function drawEntry(canvas, x, y, w, h, row, focused)
    local entry = row.entry
    local item = entry.item
    panel.drawEntry(canvas, x, y, w, h, focused, {
        item = item,
        equipped = entry.equipped,
        name = text.escape(panel.itemName(item)),
        second = not info.isGold(item) and info.summary(item) or nil,
        topRight = entry.equipped and L('Equipped') or nil,
        topRightColor = color.dim,
    })
end

local function layout()
    local canvas = draw.canvas()
    local actor = state.actor
    -- What the companion carries, and the profit when they keep count.
    local status = string.format('%s  %s / %s', text.escape(gmst('sEncumbrance', 'Encumbrance')),
        text.number(math.ceil(types.Actor.getEncumbrance(actor))), text.number(math.floor(types.Actor.getCapacity(actor))))
    if state.profit then
        status = text.escape(gmst('sProfitValue', 'Profit Value')) .. ' ' .. state.profit .. '      ' .. status
    end
    local g = panel.drawFrame(canvas, text.escape(info.name(actor)), status)
    panel.drawTabs(canvas, g, { L('CompanionTheirItems'), L('CompanionYourItems') }, state.yours and 2 or 1)
    local rows = buildRows()
    panel.drawList(canvas, g, rows, currentList(), drawEntry, L('NoItems'))
    local entry = focusedEntry(rows)
    local list_ = {}
    if entry then
        list_[#list_ + 1] = { 'a', L(state.yours and 'CompanionGive' or 'LootTake') }
    end
    if currentList():groupCount(rows) > 1 then
        list_[#list_ + 1] = { 'y', L('MagicNextGroup') }
    end
    local hintsTop = hints.draw(canvas, g.detailsX + 16, g.detailsW - 32, g.bottom - 4, list_)
    if entry then
        details.drawHeader(canvas, g.detailsX, g.detailsY, g.detailsW, entry, state.yours)
        details.drawStats(canvas, g.detailsX, g.detailsY + 104, g.detailsW, hintsTop - 12, entry.item, {})
    end
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

local function click()
    ambient.playSound('menu click')
end

local function move()
    local entry = focusedEntry()
    if entry == nil then
        return
    end
    local item = entry.item
    -- Summoned items can't change hands.
    if types.Item.isBound(item) then
        ui.showMessage(gmst('sBarterDialog12', "You can't give away summoned items."))
        return
    end
    ambient.playSound(state.yours and types.Item.getDownSound(item) or types.Item.getUpSound(item))
    core.sendGlobalEvent('ModernMorrowCompanionMove', {
        player = self.object,
        companion = state.actor,
        item = item,
        give = state.yours,
    })
end

local function close()
    -- A companion who works for pay warns the player who leaves owing them. Leaving anyway ends the conversation, which
    -- the companion's script needs, as in the engine's window.
    if state.profit and state.profit < 0 then
        confirm.open({
            question = gmst('sCompanionWarningMessage', 'Your companion is unhappy with the share.'),
            yes = gmst('sCompanionWarningButtonOne', 'Leave'),
            no = gmst('sCompanionWarningButtonTwo', 'Stay'),
            onYes = function()
                I.UI.removeMode(companion.mode)
                if types.Player.getDialogueActor(self) ~= nil then
                    types.Player.sayGoodbye(self)
                end
            end,
        })
        return
    end
    I.UI.removeMode(companion.mode)
end

companion.handler = {
    onButton = function(name)
        if state == nil then
            return
        end
        if name == 'a' then
            move()
        elseif name == 'y' then
            if currentList():nextGroup(buildRows()) then
                click()
                redraw()
            end
        elseif name == 'lb' or name == 'rb' then
            state.yours = not state.yours
            click()
            redraw()
        elseif name == 'b' then
            close()
        end
    end,
    onMove = function(_, dy)
        if state == nil or dy == 0 then
            return
        end
        if currentList():move(buildRows(), dy) then
            redraw()
        end
    end,
}

local function show(actor)
    if actor == nil then
        return
    end
    if state == nil or state.actor ~= actor then
        state = { actor = actor, yours = false, theirList = list.new(), yourList = list.new() }
        core.sendGlobalEvent('ModernMorrowCompanionProfit', { player = self.object, companion = actor })
    end
    router.push(companion.handler)
    if element then
        element:destroy()
    end
    element = ui.create(layout())
end

local function hide()
    router.remove(companion.handler)
    confirm.close()
    if element then
        element:destroy()
        element = nil
    end
    if state and not isOpen() then
        state = nil
    end
end

-- From the global script: the profit that the companion counts, or nil if they don't keep count.
function companion.onProfit(profit)
    if state == nil then
        return
    end
    state.profit = profit
    -- Event handlers may run on the Lua worker thread, which can't measure text, so the next frame redraws.
    state.dirty = true
end

-- Redraws when the items change.
function companion.onFrame()
    if element == nil or state == nil then
        return
    end
    local parts = {}
    for _, actor in ipairs({ self.object, state.actor }) do
        for _, item in ipairs(types.Actor.inventory(actor):getAll()) do
            parts[#parts + 1] = item.id .. ':' .. item.count
        end
    end
    local signature = table.concat(parts, ',')
    if signature ~= state.signature or state.dirty then
        state.signature = signature
        state.dirty = false
        redraw()
    end
end

function companion.register()
    I.UI.registerWindow(companion.window, function(actor)
        show(actor)
    end, hide, { capturesInput = true })
end

return companion
