-- The journal screen. It lists the player's quests, the ones in progress first, and shows the highlighted quest's
-- entries beside the list, newest first. The Recent, Places and Factions tabs sort the same quests by their last
-- update, by the town they started in, and by the faction they are for.
-- It replaces the engine's journal book, which is the only window of the Journal mode, and follows the fork's design in
-- ui-rework/design/journal.md. Like the fork, it has no topic index and no topic links.

local core = require('openmw.core')
local ui = require('openmw.ui')
local util = require('openmw.util')
local ambient = require('openmw.ambient')

local style = require('scripts.modernmorrow.ui.style')
local layer = require('scripts.modernmorrow.ui.layer')
local draw = require('scripts.modernmorrow.ui.draw')
local text = require('scripts.modernmorrow.ui.text')
local hints = require('scripts.modernmorrow.ui.hints')
local list = require('scripts.modernmorrow.ui.list')
local scroll = require('scripts.modernmorrow.ui.scroll')
local tabs = require('scripts.modernmorrow.ui.tabs')
local hub = require('scripts.modernmorrow.screens.hub')
local quests = require('scripts.modernmorrow.journal.quests')

local l10n = core.l10n('ModernMorrow')

local journal = { window = 'Journal', mode = 'Journal' }

local color = style.color
local size = style.textSize
local space = style.space

local ENTRY_HEIGHT = 56
local HEADER_ROW_HEIGHT = 28
local COLUMNS_HEIGHT = 30
local PLACE_COLUMN_WIDTH = 170
local FACTION_COLUMN_WIDTH = 180
local LINE_HEIGHT = size.normal + 6

local TAB = { Recent = 1, Places = 2, Factions = 3 }
local TAB_KEYS = { 'JournalTabRecent', 'JournalTabPlaces', 'JournalTabFactions' }

local function L(key)
    return text.escape(l10n(key))
end

-- ---------------------------------------------------------------------------------------------------------------
-- State

local element = nil
local state = {}

local function resetState()
    state.tab = TAB.Recent
    state.list = list.new()
    -- The quests, read when the screen shows. The journal can't change while the menus are open.
    state.quests = {}
    -- The highlighted quest, by name, so it stays highlighted when the tab changes.
    state.selected = nil
    -- How far the highlighted quest's entries are scrolled, and how far they can scroll.
    state.entriesQuest = nil
    state.entriesScroll = 0
    state.entriesMax = 0
end
resetState()

-- Entries wrapped into lines, by width and text. Wrapping measures every word, so each entry is wrapped once.
local wrapped = {}

local function wrapLines(s, width)
    local key = width .. '\0' .. s
    local lines = wrapped[key]
    if lines == nil then
        lines = text.wrap(s, width, size.normal, 1000)
        wrapped[key] = lines
    end
    return lines
end

-- ---------------------------------------------------------------------------------------------------------------
-- The list

-- Guild and House quests have the faction in their name, such as "Fighters Guild: Exterminator". The list shows the
-- faction in its own column, so the name leaves it out.
local function shortName(quest)
    local prefix, rest = quest.name:match('^([^:]*):%s*(.*)$')
    if prefix == nil or rest == '' or quest.faction == nil then
        return quest.name
    end
    local faction = quests.factionName(quest):lower()
    prefix = prefix:lower()
    if faction:sub(-#prefix) ~= prefix then
        return quest.name
    end
    return rest
end

local function groupOf(quest)
    if quest.finished then
        return L('JournalFinished')
    end
    if state.tab == TAB.Places then
        return quest.town and text.escape(quest.town) or L('JournalUnknownPlace')
    elseif state.tab == TAB.Factions then
        local faction = quests.factionName(quest)
        return faction ~= '' and text.escape(faction) or L('JournalNoFaction')
    end
    return L('JournalInProgress')
end

-- The quests in the order the tab lists them. They are already in order of the last update. Quests in progress come
-- first, and on the Places and Factions tabs they are grouped by name, with the quests that have no place or faction
-- after the rest.
local function ordered()
    local function key(quest)
        if quest.finished then
            return 2, ''
        elseif state.tab == TAB.Places then
            return quest.town and 0 or 1, (quest.town or ''):lower()
        elseif state.tab == TAB.Factions then
            local faction = quests.factionName(quest)
            return faction ~= '' and 0 or 1, faction:lower()
        end
        return 0, ''
    end
    local order = {}
    for i, quest in ipairs(state.quests) do
        local rank, name = key(quest)
        order[i] = { quest = quest, rank = rank, name = name, index = i }
    end
    table.sort(order, function(a, b)
        if a.rank ~= b.rank then
            return a.rank < b.rank
        elseif a.name ~= b.name then
            return a.name < b.name
        end
        return a.index < b.index
    end)
    return order
end

local function buildRows()
    local rows = {}
    local group = nil
    for _, item in ipairs(ordered()) do
        local header = groupOf(item.quest)
        if header ~= group then
            group = header
            rows[#rows + 1] = { header = header }
        end
        rows[#rows + 1] = { quest = item.quest }
    end
    return rows
end

-- Moves the selection to the quest highlighted before, such as after a tab change.
local function focusSelected(rows)
    local index = 0
    for _, row in ipairs(rows) do
        if row.quest then
            index = index + 1
            if row.quest.name == state.selected then
                state.list.focus = index
                return
            end
        end
    end
    state.list.focus = 1
end

-- ---------------------------------------------------------------------------------------------------------------
-- Drawing

local function drawEntry(canvas, x, y, w, h, quest, focused)
    if focused then
        canvas:rect(x, y, w, h, color.accent, 0.16)
        canvas:rect(x, y, 3, h, color.accent)
    end
    local nameColor = focused and color.active or (quest.finished and color.text or color.bright)
    local columnsX = x + w - 8 - PLACE_COLUMN_WIDTH - FACTION_COLUMN_WIDTH
    local nameW = columnsX - x - 16 - 12
    canvas:text(x + 12, y + 4, nameW, 26, text.fit(shortName(quest), nameW, size.normal), size.normal, nameColor)
    canvas:text(x + 12, y + 29, nameW, 20, text.escape(quests.timestamp(quest.entries[#quest.entries])), size.small,
        color.dim)
    canvas:text(columnsX, y, PLACE_COLUMN_WIDTH - 8, h, text.fit(quest.town or '', PLACE_COLUMN_WIDTH - 8, size.small),
        size.small, color.text)
    canvas:text(columnsX + PLACE_COLUMN_WIDTH, y, FACTION_COLUMN_WIDTH - 4, h,
        text.fit(quests.factionName(quest), FACTION_COLUMN_WIDTH - 4, size.small), size.small, color.text)
end

local function drawList(canvas, x, y, w, h, rows)
    canvas:rect(x, y, w, h, color.panel, 0.9)
    local columnsX = x + w - 12 - PLACE_COLUMN_WIDTH - FACTION_COLUMN_WIDTH
    canvas:text(columnsX, y + 6, PLACE_COLUMN_WIDTH, 22, L('JournalPlace'), size.small, color.dim)
    canvas:text(columnsX + PLACE_COLUMN_WIDTH, y + 6, FACTION_COLUMN_WIDTH, 22, L('JournalFaction'), size.small,
        color.dim)
    local placed = state.list:place(rows, y + COLUMNS_HEIGHT, y + h - 4, function(row)
        return row.quest and ENTRY_HEIGHT or HEADER_ROW_HEIGHT
    end)
    for _, place in ipairs(placed) do
        local row = place.row
        if row.header then
            canvas:text(x + 12, place.y, w - 24, 24, row.header, size.small, color.bright)
            canvas:rect(x + 12, place.y + 23, w - 24, 1, color.accent, 0.25)
        else
            drawEntry(canvas, x + 4, place.y, w - 8, ENTRY_HEIGHT - 2, row.quest, place.focused)
        end
    end
end

local function drawDetails(canvas, x, y, w, h, quest, groups)
    if quest.name ~= state.entriesQuest then
        state.entriesQuest = quest.name
        state.entriesScroll = 0
    end
    canvas:rect(x, y, w, h, color.panel, 0.9)
    canvas:rect(x, y, w, 1, color.accent, 0.3)
    local innerX = x + 16
    local innerW = w - 32
    local ty = y + 12

    for _, line in ipairs(text.wrap(quest.name, innerW, size.large, 2)) do
        canvas:text(innerX, ty, innerW, 32, line, size.large, color.bright)
        ty = ty + 30
    end
    canvas:text(innerX, ty, innerW, 22, quest.finished and L('JournalFinished') or L('JournalInProgress'), size.small,
        quest.finished and color.better or color.new)
    ty = ty + 30

    -- Who gave the quest, where, and for whom, when that is known.
    local function fact(label, value)
        if value == nil or value == '' then
            return
        end
        canvas:text(innerX, ty, 100, 26, label, size.small, color.dim)
        canvas:text(innerX + 100, ty, innerW - 100, 26, text.fit(value, innerW - 100, size.normal), size.normal,
            color.text)
        ty = ty + 26
    end
    fact(L('JournalGivenBy'), quests.giverName(quest))
    fact(L('JournalPlace'), quest.place)
    fact(L('JournalFaction'), quests.factionName(quest))
    ty = ty + 8
    canvas:rect(innerX, ty, innerW, 1, color.accent, 0.25)
    ty = ty + 10

    -- The entries, newest first, on a page that the right stick scrolls when they don't fit.
    local areaBottom = y + h - 56
    local areaH = areaBottom - ty
    local paperW = innerW - 12
    local paper = draw.canvas()
    local py = 0
    for i = #quest.entries, 1, -1 do
        local entry = quest.entries[i]
        local latest = i == #quest.entries
        paper:text(0, py - state.entriesScroll, paperW, 22, text.escape(quests.timestamp(entry)), size.small,
            latest and color.new or color.dim)
        py = py + 24
        for _, line in ipairs(wrapLines(entry.text, paperW)) do
            local lineY = py - state.entriesScroll
            -- Only the lines that show are drawn.
            if lineY + LINE_HEIGHT > 0 and lineY < areaH then
                paper:text(0, lineY, paperW, LINE_HEIGHT, line, size.normal, latest and color.bright or color.text)
            end
            py = py + LINE_HEIGHT
        end
        py = py + 14
    end
    canvas:add(paper:layout({ position = util.vector2(innerX, ty), size = util.vector2(paperW, areaH) }))

    state.entriesMax = math.max(0, py - areaH)
    scroll.drawBar(canvas, innerX + innerW - 4, ty, areaH, state.entriesScroll, py, areaH, color.accent, 0.12, 0.6)

    local hintList = {}
    if groups then
        hintList[#hintList + 1] = { 'y', L('MagicNextGroup') }
    end
    if state.entriesMax > 0 then
        hintList[#hintList + 1] = { 'rstick', L('Scroll') }
    end
    hints.draw(canvas, innerX, innerW, y + h, hintList)
end

local function layout()
    local layerSize = layer.size(layer.menu)
    local width, height = layerSize.x, layerSize.y
    local canvas = draw.canvas()
    canvas:rect(0, space.topBarHeight, width, height - space.topBarHeight, color.black, 0.97)
    hub.drawTopBar(canvas, width, journal)

    local finished = 0
    for _, quest in ipairs(state.quests) do
        if quest.finished then
            finished = finished + 1
        end
    end
    local captions = {}
    for i, key in ipairs(TAB_KEYS) do
        captions[i] = L(key)
    end
    tabs.draw(canvas, width, captions, state.tab,
        string.format('%d %s    %d %s', #state.quests - finished, L('JournalCountInProgress'), finished,
            L('JournalCountFinished')))

    local top = space.contentTop
    local bottom = height - space.contentBottomMargin
    local panelW = math.max(320, math.floor(width * 0.36))
    local listW = width - 2 * space.margin - panelW - 16
    local px = width - space.margin - panelW
    if #state.quests == 0 then
        canvas:rect(space.margin, top, listW, bottom - top, color.panel, 0.9)
        canvas:text(space.margin + 16, top + 16, listW - 32, 30, L('JournalNoQuests'), size.normal, color.dim)
        canvas:rect(px, top, panelW, bottom - top, color.panel, 0.9)
    else
        local rows = buildRows()
        drawList(canvas, space.margin, top, listW, bottom - top, rows)
        local row = state.list:focused(rows)
        state.selected = row.quest.name
        drawDetails(canvas, px, top, panelW, bottom - top, row.quest, state.list:groupCount(rows) > 1)
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
-- Input

local function click()
    ambient.playSound('menu click')
end

local function move(_, dy)
    if dy ~= 0 and state.list:move(buildRows(), dy) then
        redraw()
    end
end

local function changeTab(delta)
    state.tab = (state.tab - 1 + delta) % #TAB_KEYS + 1
    local rows = buildRows()
    focusSelected(rows)
    state.list.scroll = 1
    click()
    redraw()
end

local function nextGroup()
    if state.list:nextGroup(buildRows()) then
        click()
        redraw()
    end
end

local function scrollEntries(stickY, dt)
    local before = state.entriesScroll
    state.entriesScroll = scroll.step(before, state.entriesMax, stickY, dt)
    if math.floor(state.entriesScroll) ~= math.floor(before) then
        redraw()
    end
end

journal.handler = {
    onButton = function(name)
        if name == 'b' then
            hub.close()
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
    onStick = function(_, y, dt)
        scrollEntries(y, dt)
    end,
}

-- ---------------------------------------------------------------------------------------------------------------
-- The screen

function journal.title()
    return text.escape(core.getGMST('sJournal') or 'Journal')
end

function journal.show()
    state.quests = quests.collect()
    if state.selected then
        focusSelected(buildRows())
    end
    wrapped = {}
    if element then
        element:destroy()
    end
    element = ui.create(layout())
end

function journal.hide()
    if element then
        element:destroy()
        element = nil
    end
end

-- Collects the quests again when the global script has named the cells that their entries name.
function journal.onFrame()
    if element and quests.hasNewNames() then
        state.quests = quests.collect()
        if state.selected then
            focusSelected(buildRows())
        end
        redraw()
    end
end

-- Forgets the screen's state, such as when another game is loaded.
function journal.reset()
    resetState()
end

return journal
