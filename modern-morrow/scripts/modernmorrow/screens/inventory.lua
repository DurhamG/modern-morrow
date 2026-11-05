-- The inventory screen. Items are sorted into tabs and listed with the stats that matter for them, and a details
-- panel compares the highlighted item with the equipped one, so the player never needs a tooltip.
-- It follows the fork's design in ui-rework/design/inventory.md.

local core = require('openmw.core')
local self = require('openmw.self')
local types = require('openmw.types')
local ui = require('openmw.ui')
local util = require('openmw.util')
local ambient = require('openmw.ambient')
local I = require('openmw.interfaces')

local style = require('scripts.modernmorrow.ui.style')
local layer = require('scripts.modernmorrow.ui.layer')
local draw = require('scripts.modernmorrow.ui.draw')
local text = require('scripts.modernmorrow.ui.text')
local hints = require('scripts.modernmorrow.ui.hints')
local list = require('scripts.modernmorrow.ui.list')
local tabs = require('scripts.modernmorrow.ui.tabs')
local countPicker = require('scripts.modernmorrow.ui.countpicker')
local info = require('scripts.modernmorrow.items.info')
local details = require('scripts.modernmorrow.items.details')
local recent = require('scripts.modernmorrow.items.recent')
local books = require('scripts.modernmorrow.items.books')
local hub = require('scripts.modernmorrow.screens.hub')

local l10n = core.l10n('ModernMorrow')

local inventory = { window = 'Inventory' }

local color = style.color
local size = style.textSize
local space = style.space

local ROW_HEIGHT = 42
local HEADER_ROW_HEIGHT = 28
-- How far a book scrolls per second with the right stick pushed all the way, in layer units.
local BOOK_SCROLL_SPEED = 600

local TAB = info.INVENTORY_TAB
local TAB_KEYS = info.INVENTORY_TAB_KEYS

local SLOT = types.Actor.EQUIPMENT_SLOT

local SLOT_KEYS = {}
for name, slot in pairs(SLOT) do
    SLOT_KEYS[slot] = 'Slot' .. name
end

-- The body areas of the Equipment tab. The left column of the overview holds the first six, the right column the
-- rest. `top` lists the slots from the outermost layer in, for the item the overview shows. `grouped` areas list their
-- items under a header for each slot.
local AREAS = {
    { key = 'AreaHead', slots = { SLOT.Helmet } },
    { key = 'AreaShoulders', slots = { SLOT.LeftPauldron, SLOT.RightPauldron }, grouped = true },
    { key = 'AreaChest', slots = { SLOT.Cuirass, SLOT.Shirt, SLOT.Robe }, top = { SLOT.Cuirass, SLOT.Robe, SLOT.Shirt },
        grouped = true },
    { key = 'AreaArms', slots = { SLOT.LeftGauntlet, SLOT.RightGauntlet }, grouped = true },
    { key = 'AreaLegs', slots = { SLOT.Greaves, SLOT.Pants, SLOT.Skirt }, top = { SLOT.Greaves, SLOT.Skirt, SLOT.Pants },
        grouped = true },
    { key = 'AreaFeet', slots = { SLOT.Boots } },
    { key = 'AreaWeapon', slots = { SLOT.CarriedRight } },
    { key = 'AreaOffHand', slots = { SLOT.CarriedLeft } },
    { key = 'AreaAmmunition', slots = { SLOT.Ammunition } },
    { key = 'AreaAmulet', slots = { SLOT.Amulet } },
    { key = 'AreaRings', slots = { SLOT.LeftRing, SLOT.RightRing } },
    { key = 'AreaBelt', slots = { SLOT.Belt } },
}
local AREAS_PER_COLUMN = 6
local WEAPON_AREA = 7
local AMMUNITION_AREA = 9
local FIRST_JEWELLERY_AREA = 10

local function L(key)
    return text.escape(l10n(key))
end

local function gmst(name, fallback)
    return text.escape(core.getGMST(name) or fallback)
end

-- ---------------------------------------------------------------------------------------------------------------
-- State

local element = nil
local entries = {}
local signature = ''

local state = {}

local function resetState()
    state.tab = TAB.Equipment
    state.lists = {}
    for i = 1, #TAB_KEYS do
        state.lists[i] = list.new()
    end
    -- Each list remembers its own sort order, as a position in the orders the sort button cycles through.
    state.tabSort = {}
    state.areaSort = {}
    -- Which part of the Equipment tab has the selection: 'areas', 'slots' or 'items'.
    state.focus = 'areas'
    state.area = 1
    state.slotFocus = 1
    state.slotFilter = nil
    -- An item waiting for the player to pick which ring hand it goes on.
    state.pendingRing = nil
    state.areaList = list.new()
    -- The item last highlighted, which is marked as seen once the selection moves off it.
    state.highlighted = nil
    state.book = { id = nil, scroll = 0, max = 0 }
end
resetState()

-- ---------------------------------------------------------------------------------------------------------------
-- Items

local function equippedSlots()
    local slots = {}
    for slot, item in pairs(types.Actor.getEquipment(self)) do
        slots[item.id] = slot
    end
    return slots
end

local function readItems()
    local equipped = equippedSlots()
    local result = {}
    local parts = {}
    for _, item in ipairs(types.Actor.inventory(self):getAll()) do
        if not info.isGold(item) and item.count > 0 then
            local entry = { item = item, equipped = equipped[item.id] ~= nil, index = #result + 1 }
            result[#result + 1] = entry
            parts[#parts + 1] = item.id .. ':' .. item.count .. (entry.equipped and 'e' or '')
        end
    end
    -- The engine updates the carried weight a frame after the items change, and the header shows it.
    parts[#parts + 1] = tostring(math.floor(types.Actor.getEncumbrance(self)))
    return result, table.concat(parts, ',')
end

local function updateItems()
    entries, signature = readItems()
end

local function equippedIn(slot)
    return types.Actor.getEquipment(self, slot)
end

local function entryFor(item)
    if item == nil then
        return nil
    end
    for _, entry in ipairs(entries) do
        if entry.item == item then
            return entry
        end
    end
    return nil
end

local function contains(values, wanted)
    for _, value in ipairs(values) do
        if value == wanted then
            return true
        end
    end
    return false
end

local function areaItems(area)
    local result = {}
    local def = AREAS[area]
    for _, entry in ipairs(entries) do
        local item = entry.item
        if info.isEquipment(item) and (area ~= WEAPON_AREA or item.type == types.Weapon) then
            for _, slot in ipairs(info.slots(item)) do
                if contains(def.slots, slot) then
                    result[#result + 1] = entry
                    break
                end
            end
        end
    end
    return result
end

local function areaTopEntry(area)
    local def = AREAS[area]
    for _, slot in ipairs(def.top or def.slots) do
        local entry = entryFor(equippedIn(slot))
        if entry then
            return entry
        end
    end
    return nil
end

local function areaHasNew(area)
    for _, entry in ipairs(areaItems(area)) do
        if not entry.equipped and recent.isNew(entry.item.recordId) then
            return true
        end
    end
    return false
end

-- ---------------------------------------------------------------------------------------------------------------
-- Columns and sorting

local STAT_KEYS = {
    armor = 'ColumnArmor', damage = 'ColumnDamage', speed = 'ColumnSpeed', weight = 'ColumnWeight',
    value = 'ColumnValue', count = 'ColumnCount', effect = 'ColumnEffect', quality = 'ColumnQuality',
    uses = 'ColumnUses', name = 'ColumnName',
}

local function statLabel(stat)
    return L(STAT_KEYS[stat])
end

-- The text of a list column, as plain text.
local function statText(stat, entry)
    local item = entry.item
    if stat == 'armor' then
        return item.type == types.Armor and text.number(info.armorRating(item)) or '-'
    elseif stat == 'damage' then
        if item.type ~= types.Weapon then
            return '-'
        end
        return info.rangeText(info.bestAttack(info.record(item)))
    elseif stat == 'speed' then
        return item.type == types.Weapon and text.number(info.record(item).speed) or '-'
    elseif stat == 'weight' then
        return text.number(info.weight(item))
    elseif stat == 'value' then
        return tostring(info.value(item))
    elseif stat == 'count' then
        return tostring(item.count)
    elseif stat == 'effect' then
        local effects = info.effects(item)
        return effects[1] or '-'
    elseif stat == 'quality' then
        local quality = info.toolQuality(item)
        return quality and text.number(quality) or '-'
    elseif stat == 'uses' then
        local condition = info.hasCondition(item) and info.condition(item)
        return condition and tostring(condition) or '-'
    end
    return info.name(item)
end

local function areaColumns(area)
    if area == WEAPON_AREA then
        return { { 'damage', 70 }, { 'speed', 58 }, { 'weight', 62 }, { 'value', 62 } }
    elseif area == AMMUNITION_AREA then
        return { { 'damage', 70 }, { 'count', 58 }, { 'value', 62 } }
    elseif area >= FIRST_JEWELLERY_AREA then
        return { { 'weight', 62 }, { 'value', 62 } }
    end
    return { { 'armor', 64 }, { 'weight', 64 }, { 'value', 64 } }
end

local function tabColumns(tab)
    if tab == TAB.Consumables then
        return { { 'effect', 200 }, { 'count', 58 }, { 'value', 62 } }
    elseif tab == TAB.Tools then
        return { { 'quality', 70 }, { 'uses', 58 }, { 'weight', 62 }, { 'value', 62 } }
    elseif tab == TAB.Notes then
        return { { 'weight', 62 }, { 'value', 62 } }
    end
    return { { 'count', 58 }, { 'weight', 62 }, { 'value', 62 } }
end

local function listColumns()
    if state.tab == TAB.Equipment then
        return areaColumns(state.area)
    end
    return tabColumns(state.tab)
end

-- The orders the sort button cycles through. The first is the list's main stat, such as the armor rating for armor
-- and the damage for weapons. The list's other columns follow, then the name.
local function sortOrders()
    local first = 'value'
    if state.tab == TAB.Equipment and (state.area == WEAPON_AREA or state.area == AMMUNITION_AREA) then
        first = 'damage'
    elseif state.tab == TAB.Equipment and state.area < FIRST_JEWELLERY_AREA then
        first = 'armor'
    elseif state.tab == TAB.Tools then
        first = 'quality'
    end
    local orders = { first }
    for _, column in ipairs(listColumns()) do
        if column[1] ~= first and column[1] ~= 'effect' then
            orders[#orders + 1] = column[1]
        end
    end
    orders[#orders + 1] = 'name'
    return orders
end

local function sortIndex()
    if state.tab == TAB.Equipment then
        return state.areaSort[state.area] or 1
    end
    return state.tabSort[state.tab] or 1
end

local function currentSort()
    local orders = sortOrders()
    return orders[math.min(sortIndex(), #orders)]
end

-- The number an item's list stat shows, to compare it with the equipped item. Items without the stat count as 0.
-- Weapons compare by their highest damage.
local function compareNumber(stat, item)
    if item == nil then
        return 0
    end
    if stat == 'armor' and item.type == types.Armor then
        return info.armorRating(item)
    elseif stat == 'damage' and item.type == types.Weapon then
        local _, high = info.bestAttack(info.record(item))
        return high
    end
    return 0
end

-- The number to sort items by, highest first. Items without the stat sort last.
local function sortNumber(stat, entry)
    local item = entry.item
    if stat == 'armor' then
        return item.type == types.Armor and info.armorRating(item) or -1
    elseif stat == 'damage' then
        if item.type ~= types.Weapon then
            return -1
        end
        -- The highest damage decides, and the lowest breaks ties.
        local low, high = info.bestAttack(info.record(item))
        return high + low / 1000
    elseif stat == 'speed' then
        return item.type == types.Weapon and info.record(item).speed or -1
    elseif stat == 'weight' then
        return info.weight(item)
    elseif stat == 'value' then
        return info.value(item)
    elseif stat == 'count' then
        return item.count
    elseif stat == 'quality' then
        return info.toolQuality(item) or -1
    elseif stat == 'uses' then
        return info.hasCondition(item) and info.condition(item) or -1
    end
    return 0
end

local function sortEntries(list_)
    local sort = currentSort()
    local keys = {}
    for _, entry in ipairs(list_) do
        keys[entry] = { number = sort ~= 'name' and sortNumber(sort, entry) or 0, name = info.name(entry.item) }
    end
    table.sort(list_, function(a, b)
        local ka, kb = keys[a], keys[b]
        if ka.number ~= kb.number then
            return ka.number > kb.number
        end
        if ka.name ~= kb.name then
            return ka.name < kb.name
        end
        return a.index < b.index
    end)
end

-- ---------------------------------------------------------------------------------------------------------------
-- Rows

local function grouped(rows, items, headers, groupOf)
    for group, header in ipairs(headers) do
        local hasHeader = false
        for _, entry in ipairs(items) do
            if groupOf(entry.item) == group then
                if not hasHeader then
                    rows[#rows + 1] = { header = header }
                    hasHeader = true
                end
                rows[#rows + 1] = { entry = entry }
            end
        end
    end
    return rows
end

local function buildRows()
    local rows = {}
    if state.tab == TAB.Recent then
        for _, recordId in ipairs(recent.shown()) do
            for _, entry in ipairs(entries) do
                if entry.item.recordId == recordId and #rows < recent.SHOWN then
                    rows[#rows + 1] = { entry = entry }
                end
            end
        end
        return rows
    end

    if state.tab == TAB.Equipment then
        local def = AREAS[state.area]
        local items = areaItems(state.area)
        if state.slotFilter then
            local filtered = {}
            for _, entry in ipairs(items) do
                if contains(info.slots(entry.item), state.slotFilter) then
                    filtered[#filtered + 1] = entry
                end
            end
            items = filtered
        end
        sortEntries(items)
        if not def.grouped or state.slotFilter then
            for _, entry in ipairs(items) do
                rows[#rows + 1] = { entry = entry }
            end
            return rows
        end
        local headers = {}
        for i, slot in ipairs(def.slots) do
            headers[i] = L(SLOT_KEYS[slot])
        end
        return grouped(rows, items, headers, function(item)
            local first = info.slots(item)[1]
            for i, slot in ipairs(def.slots) do
                if slot == first then
                    return i
                end
            end
            return nil
        end)
    end

    local items = {}
    for _, entry in ipairs(entries) do
        if info.inventoryTab(entry.item) == state.tab then
            items[#items + 1] = entry
        end
    end
    sortEntries(items)

    if state.tab == TAB.Consumables then
        -- Consumables are grouped by their main effect, so all the Restore Health potions sit together.
        local groups = {}
        for i, entry in ipairs(items) do
            local effects = info.effects(entry.item)
            local group = effects[1] or ''
            if group == '?' then
                group = l10n('UnknownEffect')
            end
            groups[i] = { group = group, entry = entry, order = i }
        end
        table.sort(groups, function(a, b)
            if a.group ~= b.group then
                return a.group < b.group
            end
            return a.order < b.order
        end)
        local current = nil
        for _, g in ipairs(groups) do
            if g.group ~= current then
                rows[#rows + 1] = { header = g.group == '' and L('None') or text.escape(g.group) }
                current = g.group
            end
            rows[#rows + 1] = { entry = g.entry }
        end
        return rows
    end

    -- Some tabs are split into groups by kind, in a fixed order, each under its own header. The sort order still
    -- applies within each group.
    if state.tab == TAB.Misc then
        return grouped(rows, items, { L('GroupKeys'), L('GroupSoulGems'), L('GroupOther') }, function(item)
            if info.isKey(item) then
                return 1
            elseif info.isSoulGem(item) then
                return 2
            end
            return 3
        end)
    end
    -- Notes are books that teach a skill, other books, then letters and notes. The game calls loose papers scrolls,
    -- even when they hold no spell.
    if state.tab == TAB.Notes then
        return grouped(rows, items, { L('GroupSkillBooks'), L('GroupBooks'), L('GroupLetters') }, function(item)
            if info.record(item).isScroll then
                return 3
            elseif info.bookSkill(item) then
                return 1
            end
            return 2
        end)
    end

    for _, entry in ipairs(items) do
        rows[#rows + 1] = { entry = entry }
    end
    return rows
end

local function currentList()
    if state.tab == TAB.Equipment then
        return state.areaList
    end
    return state.lists[state.tab]
end

local function focusedEntry()
    if state.tab == TAB.Equipment then
        if state.focus == 'areas' then
            return areaTopEntry(state.area)
        elseif state.focus == 'slots' then
            return entryFor(equippedIn(AREAS[state.area].slots[state.slotFocus]))
        end
    end
    local row = currentList():focused(buildRows())
    return row and row.entry
end

-- The slot whose item the given one would replace.
local function compareSlot(entry)
    if entry == nil or not info.isEquipment(entry.item) or entry.equipped then
        return nil
    end
    local slots = info.slots(entry.item)
    if #slots == 0 then
        return nil
    end
    if state.pendingRing == entry and state.focus == 'slots' then
        return AREAS[state.area].slots[state.slotFocus]
    end
    if state.slotFilter and contains(slots, state.slotFilter) then
        return state.slotFilter
    end
    return slots[1]
end

-- The equipped item the given one would replace.
local function compareEntry(entry)
    local slot = compareSlot(entry)
    return slot and entryFor(equippedIn(slot))
end

-- What A does with the item, such as "Equip" or "Drink", or nil.
local function actionFor(entry)
    if entry == nil then
        return nil
    end
    local item = entry.item
    local t = item.type
    if info.isEquipment(item) or t == types.Lockpick or t == types.Probe then
        return entry.equipped and L('ActionUnequip') or L('Equip')
    elseif t == types.Potion then
        return L('ActionDrink')
    elseif t == types.Ingredient then
        return L('ActionEat')
    elseif t == types.Book then
        if info.isMagicScroll(item) then
            return L('Equip')
        end
        return L('ActionRead')
    elseif t == types.Repair or t == types.Apparatus or info.isSoulGem(item) then
        return L('ActionUse')
    end
    return nil
end

local function setHighlighted(recordId)
    if recordId == state.highlighted then
        return
    end
    -- The item stays marked as new while it is highlighted, and stops being new once the selection moves on.
    if state.highlighted then
        recent.markSeen(state.highlighted)
    end
    state.highlighted = recordId
end

-- ---------------------------------------------------------------------------------------------------------------
-- Drawing

local function drawHeader(canvas, width)
    local captions = {}
    for i, key in ipairs(TAB_KEYS) do
        captions[i] = L(key)
    end
    -- The armor rating, the encumbrance and the gold.
    local status = string.format('%s  %d      %s  %d / %d      %s  %d', gmst('sArmor', 'Armor'),
        math.floor(I.Combat.getArmorRating(self)), gmst('sEncumbrance', 'Encumbrance'),
        math.floor(types.Actor.getEncumbrance(self)), math.floor(types.Actor.getCapacity(self)),
        gmst('sGold', 'Gold'), types.Actor.inventory(self):countOf('gold_001'))
    tabs.draw(canvas, width, captions, state.tab, status)
end

local function drawEmptyDetails(canvas, x, y, w, h, caption)
    canvas:rect(x, y, w, h, color.panel, 0.9)
    canvas:text(x + 16, y + 16, w - 32, 30, caption, size.normal, color.dim)
end

local function drawList(canvas, x, y, w, h, rows, listState, focused)
    canvas:rect(x, y, w, h, color.panel, 0.9)
    local columns = listColumns()
    local statsW = 0
    local hasCount = false
    for _, column in ipairs(columns) do
        statsW = statsW + column[2]
        hasCount = hasCount or column[1] == 'count'
    end

    -- The Recent tab always lists the newest items first.
    local sort = nil
    if state.tab ~= TAB.Recent then
        sort = currentSort()
        canvas:text(x + 16, y + 6, 260, 22, L('SortedBy') .. ' ' .. statLabel(sort), size.small, color.dim)
    end
    local cx = x + w - statsW - 12
    for _, column in ipairs(columns) do
        canvas:text(cx, y + 6, column[2], 22, statLabel(column[1]), size.small,
            column[1] == sort and color.bright or color.dim, ui.ALIGNMENT.End)
        cx = cx + column[2]
    end

    if listState:itemCount(rows) == 0 then
        canvas:text(x + 16, y + 40, w - 32, 30, L('NoItems'), size.normal, color.dim)
        return
    end

    local placed = listState:place(rows, y + 32, y + h - 4, function(row)
        return row.header and HEADER_ROW_HEIGHT or ROW_HEIGHT
    end)
    for _, place in ipairs(placed) do
        local row = place.row
        local rowY = place.y
        if row.header then
            canvas:text(x + 12, rowY, w - 24, 24, row.header, size.small, color.bright)
            canvas:rect(x + 12, rowY + 23, w - 24, 1, color.accent, 0.25)
        else
            local entry = row.entry
            local item = entry.item
            local rowFocused = focused and place.focused
            local rx = x + 4
            local rw = w - 8
            if rowFocused then
                canvas:rect(rx, rowY, rw, ROW_HEIGHT - 2, color.accent, 0.16)
                canvas:rect(rx, rowY, 3, ROW_HEIGHT - 2, color.accent)
                setHighlighted(item.recordId)
            end
            canvas:icon(rx + 8, rowY + 3, ROW_HEIGHT - 8, item, entry.equipped)

            local nameColor = rowFocused and color.active or (entry.equipped and color.bright or color.text)
            local nameX = rx + ROW_HEIGHT + 6
            local nameW = rw - (ROW_HEIGHT + 6) - statsW - 12
            local name = info.name(item)
            if item.count > 1 and not hasCount then
                name = name .. ' (' .. item.count .. ')'
            end
            local markers = details.markers(entry)
            if #markers == 0 then
                canvas:text(nameX, rowY, nameW, ROW_HEIGHT - 2, text.fit(name, nameW, size.normal), size.normal,
                    nameColor)
            else
                -- Markers sit under the name, so a long name and its markers never collide.
                canvas:text(nameX, rowY + 1, nameW, 24, text.fit(name, nameW, size.normal), size.normal, nameColor)
                local mx = nameX
                for _, marker in ipairs(markers) do
                    canvas:text(mx, rowY + 22, 120, 16, marker[1], size.small, marker[2])
                    mx = mx + text.width(marker[1], size.small) + 10
                end
            end

            -- Armor ratings and damage are green where they beat what the slot holds now, and red where they don't.
            local slot = compareSlot(entry)
            local sx = rx + rw - statsW - 8
            for _, column in ipairs(columns) do
                local stat = column[1]
                local statColor = color.text
                if (stat == 'armor' or stat == 'damage') and slot and sortNumber(stat, entry) >= 0 then
                    local equippedNumber = compareNumber(stat, equippedIn(slot))
                    local number = compareNumber(stat, item)
                    if number > equippedNumber then
                        statColor = color.better
                    elseif number < equippedNumber then
                        statColor = color.worse
                    end
                end
                canvas:text(sx, rowY, column[2], ROW_HEIGHT - 2, text.fit(statText(stat, entry), column[2] - 4,
                    size.normal), size.normal, statColor, ui.ALIGNMENT.End)
                sx = sx + column[2]
            end
        end
    end
end

local function canShowDrop()
    return state.tab ~= TAB.Equipment or state.focus == 'items'
end

local function drawButtonHints(canvas, x, y, w, h, action, scroll)
    local list_ = {}
    if action then
        list_[#list_ + 1] = { 'a', action }
    end
    if canShowDrop() then
        list_[#list_ + 1] = { 'x', L('Drop') }
    end
    if scroll then
        list_[#list_ + 1] = { 'rstick', L('Scroll') }
    end
    return hints.draw(canvas, x + 16, w - 32, y + h, list_)
end

local function drawDetailsHeader(canvas, x, y, w, h, entry)
    canvas:rect(x, y, w, h, color.panel, 0.9)
    canvas:rect(x, y, w, 1, color.accent, 0.3)
    if state.tab ~= TAB.Equipment or state.focus == 'items' then
        setHighlighted(entry.item.recordId)
    end
    details.drawHeader(canvas, x, y, w, entry)
end

local function drawDetails(canvas, x, y, w, h, entry, action)
    drawDetailsHeader(canvas, x, y, w, h, entry)
    local hintsTop = drawButtonHints(canvas, x, y, w, h, action)
    local compared = {}
    local other = compareEntry(entry)
    if other then
        compared[1] = { label = L('Equipped'), item = other.item }
    end
    details.drawStats(canvas, x, y + 104, w, hintsTop - 16, entry.item, compared)
end

-- The book's text on a page, in place of its stats. The right stick scrolls it.
local function drawBook(canvas, x, y, w, h, entry)
    local item = entry.item
    drawDetailsHeader(canvas, x, y, w, h, entry)
    local book = state.book
    if book.id ~= item.recordId then
        book.id = item.recordId
        book.scroll = 0
    end

    local padding = 20
    local pageX = x + 16
    local pageY = y + 100
    local pageW = w - 32
    local pageH = h - 100 - 56
    local areaW = pageW - 2 * padding
    local areaH = pageH - 2 * padding
    canvas:rect(pageX, pageY, pageW, pageH, color.page)
    local raw = info.record(item).text or ''
    local isBook = types.Book.objectIsInstance(item)
    local textHeight = books.height(raw, areaW, { isBook = isBook })
    book.max = math.max(0, math.ceil(textHeight - areaH))
    book.scroll = math.max(0, math.min(book.scroll, book.max))

    -- The text moves up inside a widget of the page's size, which clips what is outside it.
    local content = books.layout(raw, areaW, { isBook = isBook, offset = math.floor(book.scroll), clipH = areaH })
    canvas:add({
        type = ui.TYPE.Widget,
        props = {
            position = util.vector2(pageX + padding, pageY + padding),
            size = util.vector2(areaW, areaH),
        },
        content = content,
    })

    local scrolls = book.max > 0
    drawButtonHints(canvas, x, y, w, h, actionFor(entry), scrolls)
    if scrolls then
        local barWidth = 4
        local trackX = pageX + pageW - padding / 2 - barWidth / 2
        local trackY = pageY + padding
        local trackH = pageH - 2 * padding
        canvas:rect(trackX, trackY, barWidth, trackH, color.black, 0.12)
        local thumbH = math.max(16, math.floor(trackH * areaH / textHeight))
        local thumbY = trackY + math.floor((trackH - thumbH) * book.scroll / book.max)
        canvas:rect(trackX, thumbY, barWidth, thumbH, color.black, 0.45)
    end
end

local function drawEquipmentOverview(canvas, width, height)
    local top = space.contentTop
    local bottom = height - space.contentBottomMargin
    local panelW = math.max(320, math.floor(width * 0.3))
    local columnsW = width - 2 * space.margin - panelW - 24
    local columnW = math.floor((columnsW - 16) / 2)
    local cardH = math.floor((bottom - top - 5 * 6) / AREAS_PER_COLUMN)

    for area, def in ipairs(AREAS) do
        local column = math.floor((area - 1) / AREAS_PER_COLUMN)
        local row = (area - 1) % AREAS_PER_COLUMN
        local x = space.margin + column * (columnW + 16)
        local y = top + row * (cardH + 6)
        local focused = area == state.area
        canvas:rect(x, y, columnW, cardH, color.panel, 0.9)
        if focused then
            canvas:rect(x, y, columnW, cardH, color.accent, 0.16)
            canvas:rect(x, y, 3, cardH, color.accent)
        end
        canvas:text(x + 84, y + 6, columnW - 100, 22, L(def.key), size.small, color.dim)
        if areaHasNew(area) then
            canvas:text(x + columnW - 70, y + 6, 58, 22, L('New'), size.small, color.new, ui.ALIGNMENT.End)
        end

        local iconSize = math.min(62, cardH - 28)
        local shown = areaTopEntry(area)
        if shown == nil then
            canvas:rect(x + 12, y + math.floor((cardH - iconSize) / 2), iconSize, iconSize, color.iconBack, 0.9)
            canvas:text(x + 84, y + 28, columnW - 96, 28, L('NothingEquipped'), size.normal, color.dim)
        else
            canvas:icon(x + 12, y + math.floor((cardH - iconSize) / 2), iconSize, shown.item, false)
            local name = info.name(shown.item)
            if shown.item.count > 1 then
                name = name .. ' (' .. shown.item.count .. ')'
            end
            canvas:text(x + 84, y + 28, columnW - 96, 28, text.fit(name, columnW - 96, size.normal), size.normal,
                focused and color.active or color.bright)

            -- The second line names the layers under the top item, or the top item's main stat.
            local others = {}
            for _, slot in ipairs(def.slots) do
                local item = equippedIn(slot)
                if item and item ~= shown.item then
                    others[#others + 1] = info.name(item)
                end
            end
            local second
            if #others > 0 then
                second = text.fit('+ ' .. table.concat(others, ', '), columnW - 96, size.small)
            else
                second = info.summary(shown.item)
            end
            canvas:text(x + 84, y + 56, columnW - 96, 24, second, size.small, color.dim)
        end
    end

    -- The highlighted area's details: what each slot holds, and what's new for it.
    local px = width - space.margin - panelW
    canvas:rect(px, top, panelW, bottom - top, color.panel, 0.9)
    canvas:text(px + 16, top + 10, panelW - 32, 32, L(AREAS[state.area].key), size.title, color.bright)
    local y = top + 52
    for _, slot in ipairs(AREAS[state.area].slots) do
        canvas:text(px + 16, y, panelW - 32, 22, L(SLOT_KEYS[slot]), size.small, color.dim)
        y = y + 24
        local item = equippedIn(slot)
        if item then
            canvas:icon(px + 16, y, 40, item, false)
            canvas:text(px + 66, y, panelW - 82, 22, text.fit(info.name(item), panelW - 82, size.normal),
                size.normal, color.bright)
            canvas:text(px + 66, y + 20, panelW - 82, 22, text.escape(info.describe(item)), size.small, color.dim)
        else
            canvas:text(px + 16, y, panelW - 32, 40, L('NothingEquipped'), size.normal, color.dim)
        end
        y = y + 54
    end
    local news = {}
    for _, entry in ipairs(areaItems(state.area)) do
        if not entry.equipped and recent.isNew(entry.item.recordId) then
            news[#news + 1] = info.name(entry.item)
        end
    end
    if #news > 0 then
        canvas:text(px + 16, y + 6, panelW - 32, 24,
            text.fit(l10n('New') .. ': ' .. table.concat(news, ', '), panelW - 32, size.small), size.small, color.new)
    end
    hints.draw(canvas, px + 16, panelW - 32, bottom, { { 'a', L('ActionChange') } })
end

local function drawArea(canvas, width, height)
    local top = space.contentTop
    local bottom = height - space.contentBottomMargin
    local slotsW = math.max(200, math.floor(width * 0.18))
    local panelW = math.max(300, math.floor(width * 0.27))
    local listX = space.margin + slotsW + 16
    local listW = width - space.margin - panelW - 16 - listX
    local def = AREAS[state.area]
    local margin = space.margin

    -- The area's slots.
    canvas:rect(margin, top, slotsW, bottom - top, color.panel, 0.9)
    canvas:text(margin + 16, top + 10, slotsW - 32, 32, L(def.key), size.title, color.bright)
    local y = top + 52
    if state.pendingRing then
        canvas:text(margin + 16, y, slotsW - 32, 22, L('ReplaceWhich'), size.small, color.new)
        y = y + 28
    end
    for i, slot in ipairs(def.slots) do
        local focused = state.focus == 'slots' and i == state.slotFocus
        if focused or slot == state.slotFilter then
            canvas:rect(margin + 4, y - 2, slotsW - 8, 68, color.accent, focused and 0.16 or 0.08)
            canvas:rect(margin + 4, y - 2, 3, 68, color.accent, focused and 1 or 0.5)
        end
        canvas:text(margin + 16, y, slotsW - 32, 22, L(SLOT_KEYS[slot]), size.small, color.dim)
        y = y + 24
        local item = equippedIn(slot)
        if item then
            canvas:icon(margin + 16, y, 40, item, false)
            local lines = text.wrap(info.name(item), slotsW - 80, size.small)
            local lineH = math.floor(40 / #lines)
            for line, caption in ipairs(lines) do
                canvas:text(margin + 64, y + (line - 1) * lineH, slotsW - 80, lineH, caption, size.small,
                    focused and color.active or color.bright)
            end
        else
            canvas:rect(margin + 16, y, 40, 40, color.iconBack, 0.9)
            canvas:text(margin + 64, y, slotsW - 80, 40, L('Empty'), size.small, color.dim)
        end
        y = y + 54
    end
    if #def.slots > 1 and not state.pendingRing then
        canvas:text(margin + 16, bottom - 34, slotsW - 32, 24, L(state.slotFilter and 'ShowAllSlots' or 'PickSlot'),
            size.small, color.dim)
    end

    -- The items that fit the area.
    drawList(canvas, listX, top, listW, bottom - top, buildRows(), state.areaList, state.focus == 'items')

    -- Details of the highlighted item.
    local px = width - margin - panelW
    local entry = state.pendingRing or focusedEntry()
    if entry then
        local action = nil
        if state.focus ~= 'slots' or state.pendingRing then
            action = actionFor(entry)
        end
        drawDetails(canvas, px, top, panelW, bottom - top, entry, action)
    else
        drawEmptyDetails(canvas, px, top, panelW, bottom - top, L('NothingEquipped'))
    end
end

local function layout()
    local layerSize = layer.size(layer.menu)
    local width, height = layerSize.x, layerSize.y
    local canvas = draw.canvas()
    canvas:rect(0, space.topBarHeight, width, height - space.topBarHeight, color.black, 0.97)
    hub.drawTopBar(canvas, width, inventory)
    drawHeader(canvas, width)

    if state.tab == TAB.Equipment then
        if state.focus == 'areas' then
            drawEquipmentOverview(canvas, width, height)
        else
            drawArea(canvas, width, height)
        end
    else
        local top = space.contentTop
        local bottom = height - space.contentBottomMargin
        -- The Notes tab shows the highlighted book's text, so its details panel gets half the width.
        local panelW
        if state.tab == TAB.Notes then
            panelW = math.floor((width - 2 * space.margin - 16) / 2)
        else
            panelW = math.max(300, math.floor(width * 0.3))
        end
        local listW = width - 2 * space.margin - panelW - 16
        drawList(canvas, space.margin, top, listW, bottom - top, buildRows(), currentList(), true)
        local entry = focusedEntry()
        local px = width - space.margin - panelW
        if entry and state.tab == TAB.Notes then
            drawBook(canvas, px, top, panelW, bottom - top, entry)
        elseif entry then
            drawDetails(canvas, px, top, panelW, bottom - top, entry, actionFor(entry))
        else
            drawEmptyDetails(canvas, px, top, panelW, bottom - top,
                L(state.tab == TAB.Recent and 'NoRecentItems' or 'NoItems'))
        end
    end

    local root = canvas:layout({ size = layerSize })
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

local function sendUse(item)
    -- Going through the UseItem event lets scripts react to the item being used, as in the engine's inventory.
    core.sendGlobalEvent('UseItem', { object = item, actor = self.object })
end

local function unequip(item)
    local equipment = types.Actor.getEquipment(self)
    for slot, equipped in pairs(equipment) do
        if equipped == item then
            equipment[slot] = nil
        end
    end
    types.Actor.setEquipment(self, equipment)
end

-- Equipment makes the sound of putting it down when it is put on, and of picking it up when it is taken off.
local function useEntry(entry)
    if entry.equipped then
        ambient.playSound(types.Item.getUpSound(entry.item))
        unequip(entry.item)
        return
    end
    if info.isEquipment(entry.item) then
        ambient.playSound(types.Item.getDownSound(entry.item))
    end
    sendUse(entry.item)
end

local function equipInSlot(entry, slot)
    -- Equipping fills the first empty slot the item fits, so emptying the chosen slot first puts it there.
    local equipment = types.Actor.getEquipment(self)
    if equipment[slot] then
        equipment[slot] = nil
        types.Actor.setEquipment(self, equipment)
    end
    ambient.playSound(types.Item.getDownSound(entry.item))
    sendUse(entry.item)
end

local function dropEntry(entry, count)
    ambient.playSound(types.Item.getDownSound(entry.item))
    core.sendGlobalEvent('ModernMorrowDropItem', { player = self.object, item = entry.item, count = count })
end

local function move(dx, dy)
    if state.tab == TAB.Equipment then
        if state.focus == 'areas' then
            local column = math.floor((state.area - 1) / AREAS_PER_COLUMN)
            local row = math.max(0, math.min(AREAS_PER_COLUMN - 1, (state.area - 1) % AREAS_PER_COLUMN + dy))
            local newColumn = math.max(0, math.min(1, column + dx))
            state.area = newColumn * AREAS_PER_COLUMN + row + 1
        elseif state.focus == 'slots' then
            local slotCount = #AREAS[state.area].slots
            state.slotFocus = math.max(1, math.min(slotCount, state.slotFocus + dy))
            if dx > 0 and not state.pendingRing then
                state.focus = 'items'
            end
        else
            if dx < 0 then
                state.focus = 'slots'
                setHighlighted(nil)
            else
                state.areaList:move(buildRows(), dy)
            end
        end
    elseif dy ~= 0 then
        currentList():move(buildRows(), dy)
    end
    redraw()
end

local function activate()
    if state.tab == TAB.Equipment then
        if state.focus == 'areas' then
            state.focus = 'items'
            state.slotFilter = nil
            state.slotFocus = 1
            state.areaList:reset()
            click()
            redraw()
            return
        end
        if state.focus == 'slots' then
            local slot = AREAS[state.area].slots[state.slotFocus]
            if state.pendingRing then
                equipInSlot(state.pendingRing, slot)
                state.pendingRing = nil
            else
                state.slotFilter = state.slotFilter ~= slot and slot or nil
                state.areaList:reset()
                click()
            end
            state.focus = 'items'
            redraw()
            return
        end
    end

    local entry = focusedEntry()
    if entry == nil then
        return
    end
    if state.tab == TAB.Equipment and not entry.equipped then
        -- A ring needs the player to choose which one it replaces, when both hands already wear one.
        local slots = info.slots(entry.item)
        local allFull = #slots > 1
        for _, slot in ipairs(slots) do
            if equippedIn(slot) == nil then
                allFull = false
            end
        end
        if allFull and not state.slotFilter then
            state.pendingRing = entry
            state.focus = 'slots'
            state.slotFocus = 1
            redraw()
            return
        end
        if state.slotFilter and #slots > 1 and contains(slots, state.slotFilter) then
            equipInSlot(entry, state.slotFilter)
            return
        end
    end
    useEntry(entry)
end

local function back()
    if state.tab == TAB.Equipment and state.focus ~= 'areas' then
        if state.pendingRing then
            state.pendingRing = nil
            state.focus = 'items'
        elseif state.slotFilter then
            state.slotFilter = nil
            state.areaList:reset()
        else
            setHighlighted(nil)
            state.focus = 'areas'
        end
        click()
        redraw()
        return
    end
    hub.close()
end

local function dropFocused()
    if state.tab == TAB.Equipment and state.focus == 'slots' then
        -- Unequips the item in the focused slot.
        local item = equippedIn(AREAS[state.area].slots[state.slotFocus])
        if item then
            ambient.playSound(types.Item.getUpSound(item))
            unequip(item)
        end
        return
    end
    if state.tab == TAB.Equipment and state.focus == 'areas' then
        return
    end
    local entry = focusedEntry()
    if entry == nil then
        return
    end
    if entry.item.count > 1 then
        countPicker.open({
            title = core.getGMST('sDrop') or 'Drop',
            item = info.name(entry.item),
            max = entry.item.count,
            onChosen = function(count)
                if entry.item:isValid() and entry.item.count >= count then
                    dropEntry(entry, count)
                end
            end,
        })
        return
    end
    dropEntry(entry, 1)
end

local function cycleSort()
    if state.tab == TAB.Recent or (state.tab == TAB.Equipment and state.focus == 'areas') then
        return
    end
    local index = sortIndex() % #sortOrders() + 1
    if state.tab == TAB.Equipment then
        state.areaSort[state.area] = index
    else
        state.tabSort[state.tab] = index
    end
    click()
    redraw()
end

local function changeTab(delta)
    setHighlighted(nil)
    state.tab = (state.tab - 1 + delta) % #TAB_KEYS + 1
    state.pendingRing = nil
    click()
    redraw()
end

local function scrollBook(stickY, dt)
    if state.tab ~= TAB.Notes or stickY == 0 or state.book.max <= 0 then
        return
    end
    -- Squaring the deflection gives finer control near the centre of the stick.
    local magnitude = (math.abs(stickY) - style.stickDeadzone) / (1 - style.stickDeadzone)
    local step = magnitude * magnitude * BOOK_SCROLL_SPEED * dt
    if stickY < 0 then
        step = -step
    end
    local before = state.book.scroll
    state.book.scroll = math.max(0, math.min(state.book.max, state.book.scroll + step))
    if math.floor(state.book.scroll) ~= math.floor(before) then
        redraw()
    end
end

inventory.handler = {
    onButton = function(name)
        if name == 'a' then
            activate()
        elseif name == 'b' then
            back()
        elseif name == 'x' then
            dropFocused()
        elseif name == 'y' then
            cycleSort()
        elseif name == 'lb' then
            changeTab(-1)
        elseif name == 'rb' then
            changeTab(1)
        elseif name == 'lt' then
            hub.cycle(-1)
        elseif name == 'rt' then
            hub.cycle(1)
        end
    end,
    onMove = move,
    onStick = function(_, y, dt)
        scrollBook(y, dt)
    end,
}

-- ---------------------------------------------------------------------------------------------------------------
-- The screen

function inventory.title()
    return text.escape(core.getGMST('sInventory') or 'Inventory')
end

function inventory.show()
    state.pendingRing = nil
    updateItems()
    if element then
        element:destroy()
    end
    element = ui.create(layout())
end

function inventory.hide()
    setHighlighted(nil)
    countPicker.close()
    if element then
        element:destroy()
        element = nil
    end
end

-- Redraws when the inventory changed. Equipping and using items happen a frame or more after the request, and
-- scripts can change the inventory too, so the screen compares it every frame while it is shown.
function inventory.onFrame()
    if element == nil then
        return
    end
    local newEntries, newSignature = readItems()
    if newSignature ~= signature then
        entries, signature = newEntries, newSignature
        if state.pendingRing and not entryFor(state.pendingRing.item) then
            state.pendingRing = nil
        end
        redraw()
    end
end

-- Forgets the screen's state, such as when another game is loaded.
function inventory.reset()
    resetState()
end

return inventory
