-- The loot screen, which replaces the engine's Container window. It is the fork's loot panel
-- (ui-rework/design/loot.md in the fork): it shows what a chest, a body or an NPC's pockets hold, with the stats that
-- matter and a comparison with what the player wears. Taking from an owned container is spelled out as stealing, and
-- pickpocketing rolls for each item the player tries to take.
--
-- The global script moves the items, with types.Player.takeItem and types.Player.pickpocket, which apply the crime
-- rules as the engine's window does. On a body, View disposes of the corpse after taking everything, as the engine's
-- Dispose of Corpse button does.

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
local recent = require('scripts.modernmorrow.items.recent')
local router = require('scripts.modernmorrow.input.router')

local l10n = core.l10n('ModernMorrow')

local loot = { window = 'Container', mode = 'Container' }

local color = style.color

local TAB = info.INVENTORY_TAB

local function L(key)
    return text.escape(l10n(key))
end

local function gmst(name, fallback)
    return core.getGMST(name) or fallback
end

local element = nil
local state = nil
-- The record of an item that X took to equip, until it arrives in the inventory.
local pendingEquip = nil

local function inventoryOf(object)
    if types.Actor.objectIsInstance(object) then
        return types.Actor.inventory(object)
    end
    return types.Container.inventory(object)
end

local function isOpen()
    local i = 1
    while I.UI.modes[i] do
        if I.UI.modes[i] == loot.mode then
            return true
        end
        i = i + 1
    end
    return false
end

-- Who owns a container, for the warning that taking from it is stealing. Plain text, or nil.
local function ownerName(container)
    local owner = container.owner
    if owner == nil then
        return nil
    end
    if owner.recordId then
        local record = types.NPC.records[owner.recordId] or types.Creature.records[owner.recordId]
        return record and record.name
    end
    if owner.factionId then
        local faction = core.factions.records[owner.factionId]
        return faction and faction.name
    end
    return nil
end

-- ---------------------------------------------------------------------------------------------------------------
-- The lists

local function containerEntries()
    local entries = {}
    if state.pickpocket then
        -- The items the player noticed when the pickpocketing started, that are still there.
        for _, item in ipairs(state.pickpocketItems) do
            if item:isValid() and item.count > 0 and item.parentContainer == state.container then
                entries[#entries + 1] = { item = item }
            end
        end
        return entries
    end
    for _, item in ipairs(inventoryOf(state.container):getAll()) do
        if item.count > 0 then
            entries[#entries + 1] = { item = item }
        end
    end
    return entries
end

local function playerEntries()
    local equipped = {}
    for _, item in pairs(types.Actor.getEquipment(self)) do
        equipped[item.id] = true
    end
    local all = {}
    for _, item in ipairs(types.Actor.inventory(self):getAll()) do
        if item.count > 0 and not info.isGold(item) then
            all[#all + 1] = { item = item, equipped = equipped[item.id] == true }
        end
    end
    local entries = {}
    if state.yourTab == TAB.Recent then
        -- The Recent tab lists what the player picked up last, which is often what a quest asks them to leave.
        for _, recordId in ipairs(recent.shown()) do
            for _, entry in ipairs(all) do
                if entry.item.recordId == recordId then
                    entries[#entries + 1] = entry
                end
            end
        end
        return entries
    end
    for _, entry in ipairs(all) do
        if info.inventoryTab(entry.item) == state.yourTab then
            entries[#entries + 1] = entry
        end
    end
    return entries
end

local function currentList()
    if state.yourItems then
        return state.yourLists[state.yourTab]
    end
    return state.list
end

local function buildRows()
    if not state.yourItems then
        return panel.groupedRows(containerEntries())
    end
    local entries = playerEntries()
    if state.yourTab ~= TAB.Recent then
        table.sort(entries, function(a, b)
            local va, vb = info.value(a.item), info.value(b.item)
            if va ~= vb then
                return va > vb
            end
            return info.name(a.item) < info.name(b.item)
        end)
    end
    local rows = {}
    for i, entry in ipairs(entries) do
        rows[i] = { entry = entry }
    end
    return rows
end

local function focusedEntry(rows)
    local row = currentList():focused(rows or buildRows())
    return row and row.entry
end

-- Whether it is clear where the item goes when taken to equip: it fits one slot, or one of its slots is free.
local function canTakeAndEquip(item)
    if state.pickpocket or not info.isEquipment(item) then
        return false
    end
    local slots = info.slots(item)
    if #slots == 0 then
        return false
    elseif #slots == 1 then
        return true
    end
    for _, slot in ipairs(slots) do
        if types.Actor.getEquipment(self, slot) == nil then
            return true
        end
    end
    return false
end

-- Whether the container is a body, which the player can dispose of.
local function isCorpse()
    return not state.pickpocket and types.Actor.objectIsInstance(state.container) and types.Actor.isDead(state.container)
end

local function canPutIn()
    -- The game doesn't let the player put items into an NPC's pockets.
    return not state.pickpocket
end

-- ---------------------------------------------------------------------------------------------------------------
-- Drawing

local function drawEntry(canvas, x, y, w, h, row, focused)
    local entry = row.entry
    local item = entry.item
    local marker, markerColor = nil, color.better
    if state.yourItems then
        if entry.equipped then
            marker, markerColor = L('Equipped'), color.dim
        end
    elseif state.stealing and not state.pickpocket then
        marker, markerColor = L('LootStealing'), color.worse
    elseif info.isUpgrade(item) then
        marker = L('LootBetter')
    end
    -- The weight is red when taking the item would go over what the player can carry.
    local weight = info.weight(item) * item.count
    local weightText, weightColor = nil, color.dim
    if weight > 0 then
        weightText = text.escape(gmst('sWeight', 'Weight')) .. ' ' .. text.number(weight)
        local spare = types.Actor.getCapacity(self) - types.Actor.getEncumbrance(self)
        if not state.yourItems and weight > spare then
            weightColor = color.worse
        end
    end
    panel.drawEntry(canvas, x, y, w, h, focused, {
        item = item,
        equipped = state.yourItems and entry.equipped,
        name = text.escape(panel.itemName(item)),
        second = not info.isGold(item) and info.summary(item) or nil,
        topRight = marker,
        topRightColor = markerColor,
        bottomRight = weightText,
        bottomRightColor = weightColor,
    })
end

local function drawDetails(canvas, g, entry)
    local x, y, w = g.detailsX, g.detailsY, g.detailsW
    local list_ = {}
    -- Taking from an owned container, or from an NPC's pockets, is stealing, and the hints say so.
    local stealing = state.stealing or state.pickpocket
    if state.yourItems then
        if entry then
            list_[#list_ + 1] = { 'a', L('LootPutInAction') }
        end
    else
        if entry then
            list_[#list_ + 1] = { 'a', L(stealing and 'LootSteal' or 'LootTake') }
        end
        if entry and not state.pickpocket then
            list_[#list_ + 1] = { 'y', L(stealing and 'LootStealAll' or 'LootTakeAll') }
        end
        if entry and canTakeAndEquip(entry.item) then
            list_[#list_ + 1] = { 'x', L(stealing and 'LootStealAndEquip' or 'LootTakeAndEquip') }
        end
        if canPutIn() then
            list_[#list_ + 1] = { 'ls', L('LootYourItems') }
        end
        if isCorpse() then
            list_[#list_ + 1] = { 'back', text.escape(gmst('sDisposeofCorpse', 'Dispose of Corpse')) }
        end
    end
    local hintsTop = hints.draw(canvas, x + 16, w - 32, g.bottom - 4, list_)
    if entry == nil then
        return
    end
    details.drawHeader(canvas, x, y, w, { item = entry.item, equipped = entry.equipped }, state.yourItems)
    -- The player's own items aren't compared, since they're already theirs.
    local compared = state.yourItems and {} or details.comparedWith(entry.item)
    details.drawStats(canvas, x, y + 104, w, hintsTop - 12, entry.item, compared)
end

local function layout()
    local canvas = draw.canvas()
    local name = info.name(state.container)
    if state.yourItems then
        name = l10n('LootPutIn') .. ' ' .. name
    end
    -- How much the player carries, since taking too much stops them moving.
    local status = string.format('%s  %s / %s', text.escape(gmst('sEncumbrance', 'Encumbrance')),
        text.number(math.floor(types.Actor.getEncumbrance(self))), text.number(math.floor(types.Actor.getCapacity(self))))
    local g = panel.drawFrame(canvas, text.escape(name), status)
    if state.yourItems then
        local captions = {}
        for i, key in ipairs(info.INVENTORY_TAB_KEYS) do
            captions[i] = L(key)
        end
        panel.drawTabs(canvas, g, captions, state.yourTab, style.textSize.small)
    elseif state.pickpocket then
        panel.drawSubtitle(canvas, g, L('LootPickpocketing'))
    elseif state.stealing then
        local owner = ownerName(state.container)
        if owner then
            panel.drawSubtitle(canvas, g, L('LootOwnedBy') .. ' ' .. text.escape(owner) .. '.  ' ..
                L('LootStealingWarning'))
        else
            panel.drawSubtitle(canvas, g, L('LootOwned'))
        end
    end
    local rows = buildRows()
    panel.drawList(canvas, g, rows, currentList(), drawEntry, L(state.yourItems and 'NoItems' or 'LootEmpty'))
    drawDetails(canvas, g, focusedEntry(rows))
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

local function click()
    ambient.playSound('menu click')
end

local function close()
    I.UI.removeMode(loot.mode)
end

local function take(equip)
    local entry = focusedEntry()
    if entry == nil or state.waiting then
        return
    end
    local item = entry.item
    -- Summoned items can't change hands.
    if types.Item.isBound(item) then
        ui.showMessage(gmst('sContentsMessage1', "You can't take that."))
        return
    end
    ambient.playSound(types.Item.getUpSound(item))
    if state.pickpocket then
        -- The NPC may notice, which ends the pickpocketing. The input waits for the roll.
        state.waiting = true
        core.sendGlobalEvent('ModernMorrowPickpocket', { player = self.object, victim = state.container, item = item })
    else
        if equip then
            pendingEquip = item.recordId
        end
        core.sendGlobalEvent('ModernMorrowTake', { player = self.object, container = state.container, item = item })
    end
    -- The window closes once the last item is taken.
    state.closeWhenEmpty = true
end

local function takeAll()
    local function takeEverything()
        ambient.playSound('Item Misc Up')
        core.sendGlobalEvent('ModernMorrowTakeAll', { player = self.object, container = state.container })
        close()
    end
    -- Taking everything from an owned container is an easy way to a bounty, so it asks first.
    if state.stealing then
        confirm.open({ question = l10n('LootStealAllConfirmation'), onYes = takeEverything })
        return
    end
    takeEverything()
end

-- Takes everything from the body, then disposes of it. The global script says so when the corpse has to stay, as a
-- quest character's does.
local function disposeCorpse()
    ambient.playSound('Item Misc Up')
    core.sendGlobalEvent('ModernMorrowDisposeCorpse', { player = self.object, actor = state.container })
    close()
end

local function putIn()
    local entry = focusedEntry()
    if entry == nil then
        return
    end
    local item = entry.item
    if types.Item.isBound(item) then
        return
    end
    local container = state.container
    -- A container can refuse, such as when it is full, and says why, as in the engine's window.
    if types.Container.objectIsInstance(container) then
        local record = types.Container.record(container)
        if record.isOrganic then
            ui.showMessage(gmst('sContentsMessage2', "You can't put anything in there."))
            return
        end
        local capacity = types.Container.getCapacity(container)
        local weight = info.weight(item) * item.count
        if capacity <= 0 or types.Container.getEncumbrance(container) + weight > capacity + 0.0001 then
            ui.showMessage(gmst('sContentsMessage3', 'The container is full.'))
            return
        end
    end
    ambient.playSound(types.Item.getDownSound(item))
    core.sendGlobalEvent('ModernMorrowPutIn', { player = self.object, container = container, item = item })
end

local function switchView()
    if not state.yourItems and not canPutIn() then
        return
    end
    state.yourItems = not state.yourItems
    click()
    redraw()
end

loot.handler = {
    onButton = function(name)
        if state == nil or state.waiting then
            return
        end
        if state.yourItems then
            if name == 'a' then
                putIn()
            elseif name == 'b' or name == 'ls' then
                switchView()
            elseif name == 'lb' or name == 'rb' then
                state.yourTab = (state.yourTab - 1 + (name == 'lb' and -1 or 1)) % #info.INVENTORY_TAB_KEYS + 1
                click()
                redraw()
            end
            return
        end
        if name == 'a' then
            take(false)
        elseif name == 'x' then
            local entry = focusedEntry()
            if entry and canTakeAndEquip(entry.item) then
                take(true)
            end
        elseif name == 'y' then
            if not state.pickpocket and #containerEntries() > 0 then
                takeAll()
            end
        elseif name == 'ls' then
            switchView()
        elseif name == 'back' then
            if isCorpse() then
                disposeCorpse()
            end
        elseif name == 'b' then
            close()
        end
    end,
    onMove = function(_, dy)
        if state == nil or state.waiting or dy == 0 then
            return
        end
        if currentList():move(buildRows(), dy) then
            redraw()
        end
    end,
}

-- ---------------------------------------------------------------------------------------------------------------
-- The screen

local function show(container)
    if container == nil then
        return
    end
    -- The mode can be covered for a moment, such as by a question, and the screen then comes back as it was.
    if state == nil or state.container ~= container then
        local pickpocket = types.NPC.objectIsInstance(container) and not types.Actor.isDead(container)
        state = {
            container = container,
            pickpocket = pickpocket,
            stealing = not types.Player.isAllowedToUse(self, container),
            list = list.new(),
            yourLists = {},
            yourItems = false,
            yourTab = TAB.Recent,
            waiting = false,
            detected = false,
        }
        for i = 1, #info.INVENTORY_TAB_KEYS do
            state.yourLists[i] = list.new()
        end
        if pickpocket then
            -- The engine rolls for each item whether the player notices it, so this is asked once.
            state.pickpocketItems = types.Player.getPickpocketItems(self, container)
        end
    end
    router.push(loot.handler)
    if element then
        element:destroy()
    end
    element = ui.create(layout())
end

local function hide()
    router.remove(loot.handler)
    confirm.close()
    if element then
        element:destroy()
        element = nil
    end
    if state and not isOpen() then
        -- Closing the window ends the pickpocketing, and the NPC may notice then, unless they already did.
        if state.pickpocket and not state.detected then
            core.sendGlobalEvent('ModernMorrowFinishPickpocketing', { player = self.object, victim = state.container })
        end
        state = nil
    end
end

-- From the global script, after a try to steal from the NPC's pockets. The NPC noticed if `detected` is true, which
-- ends the pickpocketing. It is nil if nothing could be taken.
function loot.onPickpocketResult(detected)
    if state == nil then
        return
    end
    state.waiting = false
    if detected then
        state.detected = true
        close()
        return
    end
    -- Event handlers may run on the Lua worker thread, which can't measure text, so the next frame redraws.
    state.dirty = true
end

-- From the player script's onItemAdded handler: equips the item that X took.
function loot.onItemAdded(item)
    if pendingEquip and item:isValid() and item.recordId == pendingEquip then
        pendingEquip = nil
        ambient.playSound(types.Item.getDownSound(item))
        core.sendGlobalEvent('UseItem', { object = item, actor = self.object })
    end
end

-- Redraws when the items change, and closes the window once the last item is taken.
function loot.onFrame()
    if element == nil or state == nil then
        return
    end
    local parts = {}
    for _, owner in ipairs({ self.object, state.container }) do
        if owner:isValid() then
            for _, item in ipairs(inventoryOf(owner):getAll()) do
                parts[#parts + 1] = item.id .. ':' .. item.count
            end
        end
    end
    local signature = table.concat(parts, ',')
    if signature == state.signature and not state.dirty then
        return
    end
    state.signature = signature
    state.dirty = false
    if state.closeWhenEmpty and not state.yourItems and #containerEntries() == 0 then
        close()
        return
    end
    redraw()
end

function loot.register()
    I.UI.registerWindow(loot.window, function(container)
        show(container)
    end, hide, { capturesInput = true })
end

return loot
