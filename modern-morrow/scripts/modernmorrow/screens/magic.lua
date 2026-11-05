-- The magic screen. The Active tab shows the effects on the player, the spell or item that is ready to cast, and the
-- ones readied recently, so switching between them takes one press. The other tabs list every spell, power, magic item
-- and scroll with their effects written out, so the player never needs a tooltip.
-- It follows the fork's design in ui-rework/design/magic.md.

local core = require('openmw.core')
local self = require('openmw.self')
local types = require('openmw.types')
local ui = require('openmw.ui')
local ambient = require('openmw.ambient')

local style = require('scripts.modernmorrow.ui.style')
local layer = require('scripts.modernmorrow.ui.layer')
local draw = require('scripts.modernmorrow.ui.draw')
local text = require('scripts.modernmorrow.ui.text')
local hints = require('scripts.modernmorrow.ui.hints')
local list = require('scripts.modernmorrow.ui.list')
local tabs = require('scripts.modernmorrow.ui.tabs')
local confirm = require('scripts.modernmorrow.ui.confirm')
local castables = require('scripts.modernmorrow.magic.castables')
local recent = require('scripts.modernmorrow.magic.recent')
local hub = require('scripts.modernmorrow.screens.hub')

local l10n = core.l10n('ModernMorrow')
local interfaceL10n = core.l10n('Interface')

local magic = { window = 'Magic' }

local color = style.color
local size = style.textSize
local space = style.space

local ENTRY_HEIGHT = 58
local EFFECT_HEIGHT = 50
local HEADER_ROW_HEIGHT = 28
local READY_CARD_HEIGHT = 150
-- How often the screen checks for changes, such as an effect running out, in seconds.
local REFRESH_INTERVAL = 0.2

local TAB = { Recent = 1, Active = 2, Spells = 3, Powers = 4, Items = 5, Scrolls = 6 }
local TAB_KEYS = { 'MagicTabRecent', 'MagicTabActive', 'MagicTabSpells', 'MagicTabPowers', 'MagicTabItems',
    'MagicTabScrolls' }
local TAB_KINDS = { [TAB.Spells] = 'spell', [TAB.Powers] = 'power', [TAB.Items] = 'item', [TAB.Scrolls] = 'scroll' }

local RANGE = core.magic.RANGE
local RANGE_ORDER = { RANGE.Self, RANGE.Touch, RANGE.Target }
local RANGE_GMSTS = {
    [RANGE.Self] = { 'sRangeSelf', 'Self' },
    [RANGE.Touch] = { 'sRangeTouch', 'Touch' },
    [RANGE.Target] = { 'sRangeTarget', 'Target' },
}

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
local effects = {}
local signature = ''
-- When the screen checks for changes next, in real time.
local refreshAt = 0
-- Frames to wait before reading the castables again, after an action that the engine applies at the end of a frame.
local refreshIn = 0

local state = {}

local function resetState()
    state.tab = TAB.Active
    state.lists = {}
    for i = 1, #TAB_KEYS do
        state.lists[i] = list.new()
    end
    -- The Active tab's two columns: the effects on the left, and the recent spells on the right.
    state.effectsFocused = false
    state.effectList = list.new()
    state.recentList = list.new()
end
resetState()

local function magicka()
    local stat = types.Actor.stats.dynamic.magicka(self)
    return stat.current, stat.base + stat.modifier
end

local function currentSignature()
    local parts = { castables.signature(entries) }
    for _, effect in ipairs(effects) do
        parts[#parts + 1] = effect.text .. effect.name .. (effect.timeLeft and math.floor(effect.timeLeft) or '-')
    end
    parts[#parts + 1] = tostring(math.floor(magicka()))
    parts[#parts + 1] = table.concat(recent.keys(), ',')
    return table.concat(parts, ';')
end

local function update()
    entries = castables.read()
    effects = castables.activeEffects()
end

-- ---------------------------------------------------------------------------------------------------------------
-- Rows

-- The castables readied recently, newest first, from the log of recent spells. `skipReady` leaves out the one that
-- is ready now.
local function recentEntries(skipReady)
    local byKey = {}
    for _, entry in ipairs(entries) do
        if byKey[entry.key] == nil then
            byKey[entry.key] = entry
        end
    end
    local result = {}
    for _, key in ipairs(recent.keys()) do
        local entry = byKey[key]
        if entry and not (skipReady and entry.ready) then
            result[#result + 1] = entry
            byKey[key] = nil
        end
    end
    return result
end

local function readyEntry()
    for _, entry in ipairs(entries) do
        if entry.ready then
            return entry
        end
    end
    return nil
end

local function firstRange(entry)
    local first = entry.effects[1]
    return first and first.range or RANGE.Self
end

local function schoolName(entry)
    local school = entry.school and core.stats.Skill.records[entry.school]
    return school and school.name or ''
end

local function buildRows()
    local rows = {}
    if state.tab == TAB.Recent then
        for _, entry in ipairs(recentEntries(false)) do
            rows[#rows + 1] = { entry = entry }
        end
        return rows
    end

    local kind = TAB_KINDS[state.tab]
    local items = {}
    for _, entry in ipairs(entries) do
        if entry.kind == kind then
            items[#items + 1] = entry
        end
    end
    table.sort(items, function(a, b)
        return a.name < b.name
    end)

    if state.tab == TAB.Powers then
        for _, entry in ipairs(items) do
            rows[#rows + 1] = { entry = entry }
        end
        return rows
    end

    -- Spells are grouped by their school, such as Destruction. Items and scrolls are grouped by the range of their
    -- first effect: Self, Touch, then Target.
    if state.tab == TAB.Spells then
        local groups = {}
        local names = {}
        for _, entry in ipairs(items) do
            local name = schoolName(entry)
            if groups[name] == nil then
                groups[name] = {}
                names[#names + 1] = name
            end
            table.insert(groups[name], entry)
        end
        table.sort(names)
        for _, name in ipairs(names) do
            rows[#rows + 1] = { header = text.escape(name) }
            for _, entry in ipairs(groups[name]) do
                rows[#rows + 1] = { entry = entry }
            end
        end
        return rows
    end
    for _, range in ipairs(RANGE_ORDER) do
        local hasHeader = false
        for _, entry in ipairs(items) do
            if firstRange(entry) == range then
                if not hasHeader then
                    rows[#rows + 1] = { header = gmst(RANGE_GMSTS[range][1], RANGE_GMSTS[range][2]) }
                    hasHeader = true
                end
                rows[#rows + 1] = { entry = entry }
            end
        end
    end
    return rows
end

local function recentRows()
    local rows = {}
    for _, entry in ipairs(recentEntries(true)) do
        rows[#rows + 1] = { entry = entry }
    end
    return rows
end

local function effectRows()
    local rows = {}
    for _, effect in ipairs(effects) do
        rows[#rows + 1] = { effect = effect }
    end
    return rows
end

local function focusedEntry()
    if state.tab == TAB.Active then
        if state.effectsFocused then
            return nil
        end
        local row = state.recentList:focused(recentRows())
        return row and row.entry
    end
    local row = state.lists[state.tab]:focused(buildRows())
    return row and row.entry
end

-- ---------------------------------------------------------------------------------------------------------------
-- Drawing

-- The time left on an effect, such as "13 s left", in the engine's units for durations.
local function timeLeftText(seconds)
    local whole = math.floor(seconds)
    local units = {
        { 'DurationDay', 'days', math.floor(whole / 86400) },
        { 'DurationHour', 'hours', math.floor(whole / 3600) % 24 },
        { 'DurationMinute', 'minutes', math.floor(whole / 60) % 60 },
        { 'DurationSecond', 'seconds', whole % 60 },
    }
    -- The two largest units that aren't zero show, as in the engine's tooltips. Each unit's text ends with a space.
    local parts = {}
    for _, unit in ipairs(units) do
        if unit[3] > 0 and #parts < 2 then
            parts[#parts + 1] = interfaceL10n(unit[1], { [unit[2]] = unit[3] })
        end
    end
    if #parts == 0 then
        parts[1] = interfaceL10n('DurationSecond', { seconds = 0 })
    end
    return text.escape(table.concat(parts) .. l10n('MagicLeft'))
end

-- The entry's picture: the first effect's icon for spells and powers, the item's icon for items.
local function drawPicture(canvas, x, y, iconSize, entry)
    if entry.item then
        canvas:icon(x, y, iconSize, entry.item, false)
    elseif entry.icon then
        canvas:picture(x, y, iconSize, entry.icon)
    else
        canvas:rect(x, y, iconSize, iconSize, color.iconBack, 0.9)
    end
end

local function drawHeader(canvas, width)
    local captions = {}
    for i, key in ipairs(TAB_KEYS) do
        captions[i] = L(key)
    end
    local current, max = magicka()
    tabs.draw(canvas, width, captions, state.tab,
        string.format('%s  %d / %d', gmst('sMagic', 'Magicka'), math.floor(current), math.floor(max)))
end

local function drawEntry(canvas, x, y, w, h, entry, focused)
    if focused then
        canvas:rect(x, y, w, h, color.accent, 0.16)
        canvas:rect(x, y, 3, h, color.accent)
    end
    local iconSize = h - 12
    drawPicture(canvas, x + 8, y + 6, iconSize, entry)

    local textX = x + 8 + iconSize + 12
    local numbersW = 130
    local nameW = x + w - textX - numbersW - 8
    local name = entry.name
    if entry.kind == 'scroll' and entry.count > 1 then
        name = name .. ' (' .. entry.count .. ')'
    end
    canvas:text(textX, y + 4, nameW, 26, text.fit(name, nameW, size.normal), size.normal,
        focused and color.active or color.bright)

    -- Spells show their cost and chance, and items their charge, as the engine's magic window does.
    local numbers = ''
    if entry.kind == 'spell' then
        numbers = string.format('%d / %d%%', entry.cost, entry.chance)
    elseif entry.kind == 'item' then
        numbers = string.format('%d / %d', entry.charge, entry.maxCharge)
    end
    canvas:text(x + w - numbersW - 8, y + 4, numbersW, 26, numbers, size.normal, color.text, ui.ALIGNMENT.End)

    local lines = castables.describe(entry.effects)
    local summary = lines[1] or ''
    if #lines > 1 then
        summary = summary .. ',  +' .. (#lines - 1)
    end
    canvas:text(textX, y + 30, nameW, 22, text.fit(summary, nameW, size.small), size.small, color.dim)

    local marker, markerColor = nil, color.new
    if entry.ready then
        marker = L('MagicReady')
    elseif entry.usedToday then
        marker, markerColor = L('MagicUsedToday'), color.dim
    end
    if marker then
        canvas:text(x + w - numbersW - 8, y + 30, numbersW, 22, marker, size.small, markerColor, ui.ALIGNMENT.End)
    end
end

local function drawList(canvas, x, y, w, h, rows, listState, focused, emptyCaption)
    if listState:itemCount(rows) == 0 then
        canvas:text(x + 16, y + 16, w - 32, 30, emptyCaption or L('MagicNoSpells'), size.normal, color.dim)
        return
    end
    local placed = listState:place(rows, y + 8, y + h - 4, function(row)
        return row.header and HEADER_ROW_HEIGHT or ENTRY_HEIGHT
    end)
    for _, place in ipairs(placed) do
        local row = place.row
        if row.header then
            canvas:text(x + 12, place.y, w - 24, 24, row.header, size.small, color.bright)
            canvas:rect(x + 12, place.y + 23, w - 24, 1, color.accent, 0.25)
        else
            drawEntry(canvas, x + 4, place.y, w - 8, ENTRY_HEIGHT - 2, row.entry, focused and place.focused)
        end
    end
end

-- The buttons the highlighted entry answers to. Returns the top of the hints.
local function drawButtonHints(canvas, x, y, w, h, entry)
    if entry == nil then
        return y + h
    end
    local list_ = {}
    if not entry.ready then
        list_[#list_ + 1] = { 'a', L('MagicActionReady') }
    end
    if entry.deletable then
        list_[#list_ + 1] = { 'x', gmst('sDelete', 'Delete') }
    end
    if state.tab ~= TAB.Active and state.lists[state.tab]:groupCount(buildRows()) > 1 then
        list_[#list_ + 1] = { 'y', L('MagicNextGroup') }
    end
    return hints.draw(canvas, x + 16, w - 32, y + h, list_)
end

local function drawDetails(canvas, x, y, w, h, entry)
    canvas:rect(x, y, w, h, color.panel, 0.9)
    canvas:rect(x, y, w, 1, color.accent, 0.3)
    drawPicture(canvas, x + 16, y + 16, 64, entry)
    canvas:text(x + 92, y + 14, w - 108, 34, text.fit(entry.name, w - 108, size.large), size.large, color.bright)

    local kind
    local rows = {}
    if entry.kind == 'spell' then
        kind = gmst('sSchool', 'School') .. ': ' .. text.escape(schoolName(entry))
        rows = {
            { L('MagicCost'), tostring(entry.cost) },
            { L('MagicChance'), entry.chance .. '%' },
        }
    elseif entry.kind == 'power' then
        kind = L('MagicKindPower')
    elseif entry.kind == 'item' then
        kind = gmst('sItemCastWhenUsed', 'Cast When Used')
        rows = {
            { gmst('sCastCost', 'Cast Cost'), tostring(entry.cost) },
            { L('MagicCharge'), string.format('%d / %d', entry.charge, entry.maxCharge) },
            { L('MagicCasts'), tostring(entry.cost > 0 and math.floor(entry.charge / entry.cost) or 0) },
        }
    else
        kind = gmst('sItemCastOnce', 'Cast Once')
        rows = { { L('ColumnCount'), tostring(entry.count) } }
    end
    canvas:text(x + 92, y + 46, w - 108, 26, kind, size.small, color.dim)
    local tx = x + 92
    local function tag(label, tagColor)
        canvas:text(tx, y + 70, 160, 20, label, size.small, tagColor)
        tx = tx + text.width(label, size.small) + 12
    end
    if entry.ready then
        tag(L('MagicReady'), color.new)
    end
    if entry.usedToday then
        tag(L('MagicUsedToday'), color.dim)
    end

    local hintsTop = drawButtonHints(canvas, x, y, w, h, entry)
    local ry = y + 104
    for _, row in ipairs(rows) do
        canvas:text(x + 16, ry, w - 32, 28, row[1], size.normal, color.text)
        canvas:text(x + w - 16 - 120, ry, 120, 28, row[2], size.normal, color.bright, ui.ALIGNMENT.End)
        ry = ry + 30
    end

    -- Each effect, written out in full.
    ry = ry + 8
    canvas:text(x + 16, ry, w - 32, 24, L('MagicEffects'), size.small, color.dim)
    ry = ry + 26
    for i, line in ipairs(castables.describe(entry.effects)) do
        local lines = text.wrap(line, w - 32 - 36, size.normal, 3)
        local effectH = #lines * 24
        if ry + effectH > hintsTop - 16 then
            break
        end
        local icon = castables.bigIcon(entry.effects[i].effect)
        if icon then
            canvas:image(x + 16, ry, 26, 26, draw.texture(icon))
        end
        for n, caption in ipairs(lines) do
            canvas:text(x + 16 + 36, ry + (n - 1) * 24, w - 32 - 36, 24, caption, size.normal, color.new)
        end
        ry = ry + math.max(effectH, 28) + 6
    end
end

local function drawEffects(canvas, x, y, w, h)
    canvas:rect(x, y, w, h, color.panel, 0.9)
    canvas:text(x + 16, y + 8, w - 32, 30, L('MagicActiveEffects'), size.large, color.bright)
    local rows = effectRows()
    if #rows == 0 then
        canvas:text(x + 16, y + 48, w - 32, 30, L('MagicNoEffects'), size.normal, color.dim)
        return
    end
    local placed = state.effectList:place(rows, y + 44, y + h - 8, function()
        return EFFECT_HEIGHT
    end)
    local timeW = 130
    for _, place in ipairs(placed) do
        local effect = place.row.effect
        local rowY = place.y
        local focused = state.effectsFocused and place.focused
        if focused then
            canvas:rect(x + 4, rowY, w - 8, EFFECT_HEIGHT - 2, color.accent, 0.16)
            canvas:rect(x + 4, rowY, 3, EFFECT_HEIGHT - 2, color.accent)
        end
        local iconSize = EFFECT_HEIGHT - 14
        if effect.icon then
            canvas:picture(x + 12, rowY + 6, iconSize, effect.icon)
        end
        local textX = x + 12 + iconSize + 12
        local textW = x + w - textX - timeW - 16
        canvas:text(textX, rowY + 3, textW, 24, text.fit(effect.text, textW, size.normal), size.normal,
            focused and color.active or color.bright)
        -- Where the effect came from, such as "Shield, Spell" or "Ring of Frost, Worn".
        local source = effect.name
        if effect.sourceKey then
            source = source .. ', ' .. l10n(effect.sourceKey)
        end
        canvas:text(textX, rowY + 25, x + w - textX - 16, 20, text.fit(source, x + w - textX - 16, size.small),
            size.small, color.dim)
        canvas:text(x + w - timeW - 16, rowY + 3, timeW, 24,
            effect.timeLeft and timeLeftText(effect.timeLeft) or L('MagicConstant'), size.small, color.dim,
            ui.ALIGNMENT.End)
    end
end

local function drawReadyCard(canvas, x, y, w, h, entry)
    canvas:rect(x, y, w, h, color.accent, 0.08)
    canvas:rect(x, y, 3, h, color.accent, 0.6)
    if entry == nil then
        canvas:text(x + 20, y + 16, w - 40, 30, L('MagicNothingReady'), size.normal, color.dim)
        return
    end
    drawPicture(canvas, x + 16, y + 16, 64, entry)
    canvas:text(x + 96, y + 12, w - 112, 32, text.fit(entry.name, w - 112, size.large), size.large, color.active)

    local numbers
    if entry.kind == 'spell' then
        numbers = string.format('%s %d      %s %d%%', L('MagicCost'), entry.cost, L('MagicChance'), entry.chance)
    elseif entry.kind == 'power' then
        numbers = entry.usedToday and L('MagicUsedToday') or L('MagicKindPower')
    elseif entry.kind == 'item' then
        numbers = string.format('%s %d / %d', L('MagicCharge'), entry.charge, entry.maxCharge)
    else
        numbers = string.format('%s %d', L('ColumnCount'), entry.count)
    end
    canvas:text(x + 96, y + 42, w - 112, 24, numbers, size.small, color.text)

    local ey = y + 90
    for _, line in ipairs(castables.describe(entry.effects)) do
        if ey + 24 > y + h - 4 then
            break
        end
        canvas:text(x + 16, ey, w - 32, 24, text.fit(line, w - 32, size.normal), size.normal, color.new)
        ey = ey + 24
    end
end

local function drawActive(canvas, width, height)
    local top = space.contentTop
    local bottom = height - space.contentBottomMargin
    local columnW = math.floor((width - 2 * space.margin - 16) / 2)

    drawEffects(canvas, space.margin, top, columnW, bottom - top)

    -- The spell or item ready to cast, and the ones readied recently.
    local x = space.margin + columnW + 16
    canvas:rect(x, top, columnW, bottom - top, color.panel, 0.9)
    canvas:text(x + 16, top + 8, columnW - 32, 30, L('MagicReady'), size.large, color.bright)
    drawReadyCard(canvas, x + 12, top + 44, columnW - 24, READY_CARD_HEIGHT, readyEntry())

    local recentTop = top + 44 + READY_CARD_HEIGHT + 12
    local listTop = recentTop + 32
    canvas:text(x + 16, recentTop, columnW - 32, 30, L('MagicRecent'), size.large, color.bright)
    local hintsTop = bottom
    if not state.effectsFocused then
        hintsTop = drawButtonHints(canvas, x, top, columnW, bottom - top, focusedEntry())
    end
    drawList(canvas, x + 4, listTop - 8, columnW - 8, hintsTop - 12 - (listTop - 8), recentRows(), state.recentList,
        not state.effectsFocused, L('MagicNoRecentSpells'))
end

local function layout()
    local layerSize = layer.size(layer.menu)
    local width, height = layerSize.x, layerSize.y
    local canvas = draw.canvas()
    canvas:rect(0, space.topBarHeight, width, height - space.topBarHeight, color.black, 0.97)
    hub.drawTopBar(canvas, width, magic)
    drawHeader(canvas, width)

    if state.tab == TAB.Active then
        drawActive(canvas, width, height)
    else
        local top = space.contentTop
        local bottom = height - space.contentBottomMargin
        local panelW = math.max(320, math.floor(width * 0.34))
        local listW = width - 2 * space.margin - panelW - 16
        canvas:rect(space.margin, top, listW, bottom - top, color.panel, 0.9)
        drawList(canvas, space.margin, top, listW, bottom - top, buildRows(), state.lists[state.tab], true,
            L(state.tab == TAB.Recent and 'MagicNoRecentSpells' or 'MagicNoSpells'))
        local entry = focusedEntry()
        local px = width - space.margin - panelW
        if entry then
            drawDetails(canvas, px, top, panelW, bottom - top, entry)
        else
            canvas:rect(px, top, panelW, bottom - top, color.panel, 0.9)
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

local function ready(entry)
    if entry.ready then
        return
    end
    if entry.item then
        -- An item with a cast when used enchantment is equipped as it is readied, as in the engine's magic window.
        types.Actor.setSelectedEnchantedItem(self, entry.item)
    else
        types.Actor.setSelectedSpell(self, entry.spell)
    end
    click()
    -- The engine readies it at the end of the frame.
    refreshIn = 2
end

local function askDelete(entry)
    if entry.kind ~= 'spell' then
        return
    end
    if not entry.deletable then
        ui.showMessage(core.getGMST('sDeleteSpellError') or "You can't delete this spell.")
        return
    end
    local question = (core.getGMST('sQuestionDeleteSpell') or 'Delete %s?'):gsub('%%s', function()
        return entry.name
    end)
    confirm.open({
        question = question,
        onYes = function()
            local selected = types.Actor.getSelectedSpell(self)
            if selected and selected.id == entry.spell.id then
                types.Actor.clearSelectedCastable(self)
            end
            types.Actor.spells(self):remove(entry.spell)
            refreshIn = 2
        end,
    })
end

local function move(dx, dy)
    if state.tab == TAB.Active then
        if dx < 0 then
            state.effectsFocused = true
        elseif dx > 0 then
            state.effectsFocused = false
        end
        if state.effectsFocused then
            state.effectList:move(effectRows(), dy)
        else
            state.recentList:move(recentRows(), dy)
        end
    elseif dy ~= 0 then
        state.lists[state.tab]:move(buildRows(), dy)
    end
    redraw()
end

local function changeTab(delta)
    state.tab = (state.tab - 1 + delta) % #TAB_KEYS + 1
    click()
    redraw()
end

local function nextGroup()
    if state.tab == TAB.Active then
        return
    end
    if state.lists[state.tab]:nextGroup(buildRows()) then
        click()
        redraw()
    end
end

magic.handler = {
    onButton = function(name)
        if name == 'a' then
            local entry = focusedEntry()
            if entry then
                ready(entry)
            end
        elseif name == 'b' then
            hub.close()
        elseif name == 'x' then
            local entry = focusedEntry()
            if entry then
                askDelete(entry)
            end
        elseif name == 'y' then
            nextGroup()
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
}

-- ---------------------------------------------------------------------------------------------------------------
-- The screen

function magic.title()
    return gmst('sMagicMenu', 'Magic')
end

function magic.show()
    update()
    signature = currentSignature()
    refreshAt = core.getRealTime() + REFRESH_INTERVAL
    if element then
        element:destroy()
    end
    element = ui.create(layout())
end

function magic.hide()
    confirm.close()
    if element then
        element:destroy()
        element = nil
    end
end

-- Spells, charges and effects change while the screen is shown, such as when an effect runs out, so the screen checks
-- a few times a second, and soon after the player readies or deletes something.
function magic.onFrame()
    if element == nil then
        return
    end
    -- The menus pause the game, so the screen counts real time.
    local now = core.getRealTime()
    local due = now >= refreshAt
    if refreshIn > 0 then
        refreshIn = refreshIn - 1
        due = due or refreshIn == 0
    end
    if not due then
        return
    end
    refreshAt = now + REFRESH_INTERVAL
    update()
    local newSignature = currentSignature()
    if newSignature ~= signature then
        signature = newSignature
        redraw()
    end
end

-- Forgets the screen's state, such as when another game is loaded.
function magic.reset()
    resetState()
end

return magic
