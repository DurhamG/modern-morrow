-- The conversation with an NPC, as the fork's dialogue panel: a panel along the bottom of the screen, with the NPC in
-- view above it. It replaces the engine's Dialogue window.
--
-- The left side holds the conversation: the NPC's name and disposition, then every answer, one after the other. A new
-- answer scrolls so that it starts at the top, and the right stick scrolls back through the earlier ones. The topics
-- that an answer mentions are blue.
--
-- The right side holds the options, with Topics and Services tabs that the shoulder buttons switch. New topics come
-- first: the ones the answers mention, the newest answer's first, then the ones this NPC has an answer of their own to,
-- then the rest. The asked topics, with nothing new to hear, follow, dimmed. When an answer asks a question, only the
-- answers to it show, and B does nothing until the player picks one. When a script ends the conversation, only
-- Goodbye is left.
--
-- The services open the mod's own screens on top of the conversation, which come back to it. Persuasion asks with the
-- question dialog.

local async = require('openmw.async')
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
local scroll = require('scripts.modernmorrow.ui.scroll')
local question = require('scripts.modernmorrow.ui.question')
local router = require('scripts.modernmorrow.input.router')

local l10n = core.l10n('ModernMorrow')
local v2 = util.vector2

local dialogue = {}

local color = style.color
local size = style.textSize

-- The panel's place on the layer, which is 720 units tall.
local PANEL_TOP = 404
local PANEL_BOTTOM = 696
local PANEL_MARGIN = 48
local PANEL_PADDING = 20
-- The conversation's text is a little larger than the options, so it reads well from a distance.
local TEXT_SIZE = 20
local ROW_HEIGHT = 28
local SERVICE_ROW_HEIGHT = 52
local HISTORY_TOP = PANEL_TOP + 56
local HINTS_HEIGHT = 44
local DISPOSITION_COLOR = util.color.hex('3a5aa8')
-- How often the screen checks whether a script changed the conversation, in seconds of real time.
local POLL_INTERVAL = 0.25

local SERVICES = {
    persuasion = { name = 'sPersuasion', detail = 'DialogueServicePersuasion' },
    barter = { name = 'sBarter', detail = 'DialogueServiceBarter' },
    spells = { name = 'sSpells', detail = 'DialogueServiceSpells' },
    travel = { name = 'sTravel', detail = 'DialogueServiceTravel' },
    spellmaking = { name = 'sSpellmakingMenuTitle', detail = 'DialogueServiceSpellmaking' },
    enchanting = { name = 'sEnchanting', detail = 'DialogueServiceEnchanting' },
    training = { name = 'sServiceTrainingTitle', detail = 'DialogueServiceTraining' },
    repair = { name = 'sRepair', detail = 'DialogueServiceRepair' },
    companionShare = { name = 'sCompanionShare', detail = 'DialogueServiceCompanionShare' },
}

-- The ways to persuade, in the order of the engine's persuasion window, with the game settings that name them.
local PERSUASION = {
    { type = 'admire', name = 'sAdmire' },
    { type = 'intimidate', name = 'sIntimidate' },
    { type = 'taunt', name = 'sTaunt' },
    { type = 'bribe10', name = 'sBribe 10 Gold' },
    { type = 'bribe100', name = 'sBribe 100 Gold' },
    { type = 'bribe1000', name = 'sBribe 1000 Gold' },
}

local function L(key)
    return text.escape(l10n(key))
end

local function gmst(name, fallback)
    local value = core.getGMST(name)
    if type(value) == 'string' and value ~= '' then
        return value
    end
    return fallback
end

local function colorCode(c)
    return '#' .. c:asHex()
end

local function columns(width)
    local split = math.floor(width * 0.62)
    local textX = PANEL_MARGIN + PANEL_PADDING
    return {
        textX = textX,
        textW = split - 24 - textX,
        optionsX = split,
        optionsW = width - PANEL_MARGIN - PANEL_PADDING - split,
    }
end

-- The conversation on show. Nil when there is none.
local state = nil
-- The widgets: the panel's background and the NPC's name, the conversation, the "Earlier" hint, and the options.
local frame = nil
local history = nil
local paper = nil
local mover = nil
local earlier = nil
local options = nil

local function destroyElements()
    for _, element in ipairs({ frame, history, mover, paper, earlier, options }) do
        element:destroy()
    end
    frame, history, mover, paper, earlier, options = nil, nil, nil, nil, nil, nil
end

local function isNpc(actor)
    return actor ~= nil and types.NPC.objectIsInstance(actor)
end

local function actorName(actor)
    if actor == nil or not actor:isValid() then
        return ''
    end
    return actor.type.record(actor).name or ''
end

local function disposition()
    if not isNpc(state.actor) then
        return nil
    end
    return types.NPC.getDisposition(state.actor, self)
end

-- Reads the conversation from the engine.
local function read()
    state.lines = types.Player.getDialogueTranscript(self)
    state.choices = types.Player.getDialogueChoices(self)
    state.goodbye = types.Player.mustSayGoodbye(self)
    state.topics = types.Player.getDialogueTopics(self)
    state.services = types.Actor.getDialogueServices(state.actor)
    state.disposition = disposition()
end

-- A string that changes when the conversation does, such as when a script adds a topic.
local function signature()
    local lines = types.Player.getDialogueTranscript(self)
    local parts = {
        #lines,
        #types.Player.getDialogueChoices(self),
        tostring(types.Player.mustSayGoodbye(self)),
        tostring(disposition()),
    }
    for _, topic in ipairs(types.Player.getDialogueTopics(self)) do
        parts[#parts + 1] = topic.id .. (topic.exhausted and '-' or '') .. (topic.specific and '+' or '')
    end
    return table.concat(parts, ',')
end

-- ---------------------------------------------------------------------------------------------------------------
-- The conversation

-- A line's text with its links in blue, escaped for a Text widget.
local function markup(line)
    local shown = types.Player.findTopicLinks(self, line.text)
    local s = shown.text
    local parts = {}
    local pos = 1
    for _, link in ipairs(shown.links) do
        parts[#parts + 1] = text.escape(s:sub(pos, link.first - 1))
        parts[#parts + 1] = colorCode(color.new) .. text.escape(s:sub(link.first, link.last)) .. colorCode(color.text)
        pos = link.last + 1
    end
    parts[#parts + 1] = text.escape(s:sub(pos))
    return table.concat(parts), shown.links
end

-- The topics the answers mention, newest answer first, as lower case topic IDs, each with its place in that order.
local function mentionedTopics()
    local mentioned = {}
    local count = 0
    for i = #state.lines, 1, -1 do
        local line = state.lines[i]
        if not line.isMessage then
            for _, link in ipairs(types.Player.findTopicLinks(self, line.text).links) do
                local id = link.topicId:lower()
                if mentioned[id] == nil then
                    count = count + 1
                    mentioned[id] = count
                end
            end
        end
    end
    return mentioned
end

local heights = {}
local heightCount = 0

local function wrappedHeight(caption, textSize, width)
    local key = textSize .. '|' .. width .. '|' .. caption
    local h = heights[key]
    if h == nil then
        if heightCount > 500 then
            heights = {}
            heightCount = 0
        end
        h = text.height(caption, textSize, width)
        heights[key] = h
        heightCount = heightCount + 1
    end
    return h
end

local WRAPPED = { multiline = true, wordWrap = true, textAlignV = ui.ALIGNMENT.Start }

-- Lays out every line of the conversation in a column. Returns the canvas, its height and where the latest answer
-- starts.
local function layOutHistory(width)
    local canvas = draw.canvas()
    local y = 0
    local latestTop = 0
    for _, line in ipairs(state.lines) do
        if line.isMessage then
            -- Messages, such as a journal update.
            local caption = text.escape(line.text)
            local h = wrappedHeight(caption, size.normal, width)
            canvas:text(0, y, width, h, caption, size.normal, color.dim, nil, WRAPPED)
            y = y + h + 10
        else
            latestTop = y
            if line.title and line.title ~= '' then
                canvas:text(0, y, width, 28, text.fit(line.title, width, TEXT_SIZE), TEXT_SIZE, color.bright)
                y = y + 30
            end
            local caption = markup(line)
            local h = wrappedHeight(caption, TEXT_SIZE, width)
            canvas:text(0, y, width, h, caption, TEXT_SIZE, color.text, nil, WRAPPED)
            y = y + h + 14
        end
    end
    return canvas, y, latestTop
end

local function placeHistory()
    local maxScroll = math.max(state.latestTop, state.historyH - state.viewH)
    state.historyScroll = math.max(0, math.min(state.historyScroll, math.max(0, maxScroll)))
    mover.layout.props.position = v2(0, -math.floor(state.historyScroll))
    mover:update()
    local showEarlier = state.historyScroll > 1
    if earlier.layout.props.visible ~= showEarlier then
        earlier.layout.props.visible = showEarlier
        earlier:update()
    end
end

local function frameLayout(width, cols)
    local canvas = draw.canvas()
    canvas:rect(PANEL_MARGIN, PANEL_TOP, width - 2 * PANEL_MARGIN, PANEL_BOTTOM - PANEL_TOP, color.modal)
    canvas:rect(PANEL_MARGIN, PANEL_TOP, width - 2 * PANEL_MARGIN, 1, color.accent, 0.6)
    -- A faint line between the conversation and the options.
    canvas:rect(cols.optionsX - 24, PANEL_TOP + 12, 1, PANEL_BOTTOM - PANEL_TOP - 24, color.accent, 0.15)

    -- The NPC's name and disposition.
    local name = text.fit(actorName(state.actor), cols.textW - 360, size.title)
    canvas:text(cols.textX, PANEL_TOP + 10, cols.textW, 34, name, size.title, color.bright)
    if state.disposition then
        local dx = cols.textX + text.width(name, size.title) + 20
        local label = L('DialogueDisposition') .. '  ' .. state.disposition
        canvas:text(dx, PANEL_TOP + 14, 240, 30, label, size.small, color.dim)
        local barX = dx + text.width(label, size.small) + 12
        canvas:rect(barX, PANEL_TOP + 26, 80, 6, color.black, 0.8)
        local filled = math.floor(math.max(0, math.min(100, state.disposition)) * 80 / 100)
        if filled > 0 then
            canvas:rect(barX, PANEL_TOP + 26, filled, 6, DISPOSITION_COLOR)
        end
    end
    canvas:rect(cols.textX, HISTORY_TOP - 8, cols.textW, 1, color.accent, 0.2)
    local root = canvas:layout({ size = layer.size(layer.menu) })
    root.layer = layer.menu
    return root
end

local function earlierLayout(cols)
    local canvas = draw.canvas()
    canvas:glyph(0, 4, 22, 'rs')
    canvas:text(28, 0, 92, 30, L('DialogueEarlier'), size.small, color.dim)
    local root = canvas:layout({
        position = v2(cols.textX + cols.textW - 120, PANEL_TOP + 12),
        size = v2(120, 30),
        visible = false,
    })
    root.layer = layer.menu
    return root
end

-- ---------------------------------------------------------------------------------------------------------------
-- The options

local function inChoice()
    return #state.choices > 0
end

-- The rows of the options: group headers, and entries with a `key`, a `label` (plain text) and what A does.
local function buildRows()
    local rows = {}
    if inChoice() then
        for _, choice in ipairs(state.choices) do
            rows[#rows + 1] = { key = 'choice:' .. choice.answer, label = choice.text, choice = choice.answer }
        end
        return rows
    end
    if state.goodbye then
        rows[#rows + 1] = { key = 'goodbye', label = gmst('sGoodbye', 'Goodbye'), goodbye = true }
        return rows
    end
    if state.tab == 'services' then
        for _, service in ipairs(state.services) do
            local known = SERVICES[service]
            rows[#rows + 1] = {
                key = 'service:' .. service,
                label = known and gmst(known.name, service) or service,
                detail = known and L(known.detail) or nil,
                service = service,
            }
        end
        return rows
    end

    local mentioned = mentionedTopics()
    local ranked = {}
    local asked = {}
    for i, topic in ipairs(state.topics) do
        local row = { key = 'topic:' .. topic.id, label = topic.name, topic = topic.id }
        if topic.exhausted then
            row.asked = true
            asked[#asked + 1] = row
        else
            local place = mentioned[topic.id:lower()]
            if place then
                row.mentioned = true
                row.rank = place
            elseif topic.specific then
                row.rank = 100000
            else
                row.rank = 200000
            end
            -- The topics come sorted by name, so the order within a rank stays by name.
            row.order = i
            ranked[#ranked + 1] = row
        end
    end
    table.sort(ranked, function(a, b)
        if a.rank ~= b.rank then
            return a.rank < b.rank
        end
        return a.order < b.order
    end)
    if #ranked > 0 then
        rows[#rows + 1] = { header = L('DialogueNewTopics') }
        for _, row in ipairs(ranked) do
            rows[#rows + 1] = row
        end
    end
    if #asked > 0 then
        rows[#rows + 1] = { header = L('DialogueAskedTopics') }
        for _, row in ipairs(asked) do
            rows[#rows + 1] = row
        end
    end
    return rows
end

local function choiceLines(row, w)
    return text.wrap(row.label, w - 16, size.normal, 4)
end

local function rowHeight(row, w)
    if row.header then
        return ROW_HEIGHT
    end
    if row.choice then
        return #choiceLines(row, w) * 24 + 10
    end
    return row.detail and SERVICE_ROW_HEIGHT or ROW_HEIGHT
end

-- Keeps the selection on the same option when the list changes around it, or puts it at the top after a new answer.
local function placeFocus(rows)
    if state.focusOnTop then
        state.list:reset()
        return
    end
    if state.focusKey then
        local index = 0
        for _, row in ipairs(rows) do
            if not row.header then
                index = index + 1
                if row.key == state.focusKey then
                    state.list.focus = index
                    return
                end
            end
        end
    end
end

local function optionsLayout(cols)
    local canvas = draw.canvas()
    local x, w = cols.optionsX, cols.optionsW
    local y = PANEL_TOP + 12
    local rows = buildRows()
    state.rows = rows
    placeFocus(rows)

    if inChoice() then
        canvas:text(x, y, w, 28, L('DialogueChooseReply'), size.normal, color.bright)
        canvas:rect(x, y + 32, w, 1, color.accent, 0.2)
        y = y + 44
    elseif not state.goodbye then
        -- The Topics and Services tabs, switched with the shoulder buttons.
        canvas:glyph(x, y + 5, 22, 'lb')
        local tx = x + 32
        for _, tab in ipairs({ 'topics', 'services' }) do
            local selected = tab == state.tab
            local caption = L(tab == 'topics' and 'DialogueTabTopics' or 'DialogueTabServices')
            local tabW = text.width(caption, size.normal) + 24
            canvas:text(tx, y, tabW, 32, caption, size.normal, selected and color.bright or color.dim,
                ui.ALIGNMENT.Center)
            if selected then
                canvas:rect(tx + 8, y + 30, tabW - 16, 2, color.accent)
            end
            tx = tx + tabW + 4
        end
        canvas:glyph(tx + 2, y + 5, 22, 'rb')
        y = y + 44
    end

    local placed = state.list:place(rows, y, PANEL_BOTTOM - 8, function(row)
        return rowHeight(row, w)
    end)
    local optionCount = state.list:itemCount(rows)
    for _, place in ipairs(placed) do
        local row = place.row
        local py = place.y
        local h = place.height
        if row.header then
            canvas:text(x, py + 2, w, 22, row.header, size.small, color.dim)
            canvas:rect(x, py + 23, w, 1, color.accent, 0.2)
        else
            if place.focused then
                canvas:rect(x - 8, py, w + 8, h - 2, color.accent, 0.16)
                canvas:rect(x - 8, py, 3, h - 2, color.accent)
            end
            local rowColor = row.asked and color.dim or (row.mentioned and color.new or color.bright)
            if place.focused then
                rowColor = color.active
            end
            if row.choice then
                for i, line in ipairs(choiceLines(row, w)) do
                    canvas:text(x + 4, py + 5 + (i - 1) * 24, w - 8, 24, line, size.normal, rowColor)
                end
            elseif row.detail then
                canvas:text(x + 4, py + 2, w - 8, 26, text.fit(row.label, w - 8, size.normal), size.normal, rowColor)
                canvas:text(x + 4, py + 24, w - 8, 22, row.detail, size.small, color.dim)
            else
                canvas:text(x + 4, py, w - 8, h - 2, text.fit(row.label, w - 8, size.normal), size.normal, rowColor)
            end
        end
    end

    -- What the buttons do now, along the bottom of the conversation.
    local hintList = {}
    if inChoice() then
        hintList[#hintList + 1] = { 'a', L('DialogueReply') }
    elseif not state.goodbye then
        if optionCount > 0 then
            hintList[#hintList + 1] = { 'a', L(state.tab == 'topics' and 'DialogueAsk' or 'DialogueOpen') }
        end
        if state.list:groupCount(rows) > 1 then
            hintList[#hintList + 1] = { 'y', L('MagicNextGroup') }
        end
    end
    hints.draw(canvas, cols.textX, cols.textW, PANEL_BOTTOM - 4, hintList)

    local root = canvas:layout({ size = layer.size(layer.menu) })
    root.layer = layer.menu
    return root
end

local function redrawOptions()
    if options == nil then
        return
    end
    options.layout = optionsLayout(columns(layer.size(layer.menu).x))
    options:update()
end

-- Draws everything again from what the engine says now.
local function refresh()
    read()
    local width = layer.size(layer.menu).x
    local cols = columns(width)
    -- A new answer starts the options again from the top, where the topics it mentions are.
    local newEntries = #state.lines ~= state.shownEntries
    if newEntries and state.tab == 'topics' then
        state.focusOnTop = true
        state.focusKey = nil
    end

    destroyElements()
    frame = ui.create(frameLayout(width, cols))

    local canvas, historyH, latestTop = layOutHistory(cols.textW)
    state.viewH = PANEL_BOTTOM - HINTS_HEIGHT - HISTORY_TOP
    state.historyH = historyH
    state.latestTop = latestTop
    if newEntries then
        -- A new answer scrolls the conversation so it starts at the top.
        state.historyScroll = latestTop
        state.shownEntries = #state.lines
    end
    paper = ui.create(canvas:layout({ size = v2(cols.textW, math.max(historyH, state.viewH)) }))
    mover = ui.create({
        type = ui.TYPE.Widget,
        props = { position = v2(0, 0), size = v2(cols.textW, math.max(historyH, state.viewH)) },
        content = ui.content({ paper }),
    })
    history = ui.create({
        layer = layer.menu,
        type = ui.TYPE.Widget,
        props = { position = v2(cols.textX, HISTORY_TOP), size = v2(cols.textW, state.viewH) },
        content = ui.content({ mover }),
    })
    earlier = ui.create(earlierLayout(cols))
    options = ui.create(optionsLayout(cols))
    placeHistory()
    state.signature = signature()
    state.nextPoll = core.getRealTime() + POLL_INTERVAL
end

-- ---------------------------------------------------------------------------------------------------------------
-- Input

local function onOutcome()
    -- The callback may run on the Lua worker thread, which can't measure text, so the next frame redraws.
    if state then
        state.waiting = false
        state.dirty = true
    end
end

local function persuade()
    local answers = {}
    local ways = {}
    for _, way in ipairs(PERSUASION) do
        if types.Player.canPersuadeDialogueActor(self, way.type) then
            answers[#answers + 1] = gmst(way.name, way.type)
            ways[#ways + 1] = way.type
        end
    end
    if #answers == 0 then
        return
    end
    -- The message says who is being persuaded, how they feel about the player, and how much gold the player has for
    -- bribes. The bribes the player can't afford are left out.
    local gold = types.Actor.inventory(self):countOf('gold_001')
    local message = actorName(state.actor) .. '.  '
    if state.disposition then
        message = message .. l10n('DialogueDisposition') .. ' ' .. state.disposition .. '.  '
    end
    message = message .. l10n('BarterYourGold') .. ' ' .. gold .. '.'
    state.persuasion = question.open({
        message = message,
        answers = answers,
        onAnswer = function(index)
            if state == nil then
                return
            end
            local ok, err = pcall(types.Player.persuadeDialogueActor, self, ways[index], async:callback(onOutcome))
            if ok then
                state.waiting = true
            else
                print('Modern Morrow: persuasion failed: ' .. tostring(err))
            end
        end,
        onCancel = function() end,
    })
end

local function activate()
    local row = state.list:focused(state.rows or {})
    if row == nil or state.waiting then
        return
    end
    ambient.playSound('menu click')
    local ok, err = true, nil
    if row.choice then
        ok, err = pcall(types.Player.answerDialogueChoice, self, row.choice, async:callback(onOutcome))
        state.waiting = ok
    elseif row.goodbye then
        ok, err = pcall(types.Player.sayGoodbye, self)
    elseif row.service == 'persuasion' then
        persuade()
    elseif row.service then
        ok, err = pcall(types.Player.requestDialogueService, self, row.service, async:callback(onOutcome))
        state.waiting = ok
    elseif row.topic then
        ok, err = pcall(types.Player.askDialogueTopic, self, row.topic, async:callback(onOutcome))
        state.waiting = ok
    end
    if not ok then
        print('Modern Morrow: the conversation refused a step: ' .. tostring(err))
    end
end

local function move(dy)
    local rows = state.rows or {}
    if state.list:move(rows, dy) then
        local row = state.list:focused(rows)
        state.focusKey = row and row.key
        state.focusOnTop = false
        redrawOptions()
    end
end

dialogue.handler = {
    onButton = function(name)
        if state == nil then
            return
        end
        if name == 'a' then
            activate()
        elseif name == 'b' then
            -- A question has to be answered before the conversation can end.
            if not inChoice() then
                pcall(types.Player.sayGoodbye, self)
            end
        elseif name == 'y' then
            if state.list:nextGroup(state.rows or {}) then
                local row = state.list:focused(state.rows)
                state.focusKey = row and row.key
                state.focusOnTop = false
                ambient.playSound('menu click')
                redrawOptions()
            end
        elseif name == 'lb' or name == 'rb' then
            -- There are only two tabs, so either button switches to the other.
            if not inChoice() and not state.goodbye then
                state.tab = state.tab == 'topics' and 'services' or 'topics'
                state.list:reset()
                state.focusKey = nil
                state.focusOnTop = false
                ambient.playSound('menu click')
                redrawOptions()
            end
        end
    end,
    onMove = function(_, dy)
        if state ~= nil and dy ~= 0 then
            move(dy)
        end
    end,
    -- The right stick scrolls the conversation.
    onStick = function(_, y, dt)
        if state == nil or mover == nil or y == 0 then
            return
        end
        local before = state.historyScroll
        local maxScroll = math.max(state.latestTop, state.historyH - state.viewH)
        state.historyScroll = scroll.step(before, maxScroll, y, dt)
        if math.floor(state.historyScroll) ~= math.floor(before) then
            placeHistory()
        end
    end,
}

local function newState(actor)
    return {
        actor = actor,
        tab = 'topics',
        list = list.new(),
        focusOnTop = true,
        shownEntries = 0,
        historyScroll = 0,
        lines = {},
        choices = {},
        topics = {},
        services = {},
    }
end

local function show(target)
    local actor = types.Player.getDialogueActor(self) or target
    if actor == nil then
        return
    end
    -- Opening a service, such as barter, hides the panel until the player comes back to the same conversation.
    if state == nil or state.actor ~= actor
        or #types.Player.getDialogueTranscript(self) < state.shownEntries then
        state = newState(actor)
    end
    state.waiting = false
    router.push(dialogue.handler)
    state.dirty = true
end

local function hide()
    router.remove(dialogue.handler)
    if state then
        question.close(state.persuasion)
    end
    destroyElements()
    local inConversation = false
    for _, mode in ipairs(I.UI.modes) do
        inConversation = inConversation or mode == 'Dialogue'
    end
    if not inConversation then
        state = nil
    end
end

function dialogue.onFrame()
    if state == nil or not I.UI.isWindowVisible('Dialogue') then
        return
    end
    if not state.dirty and core.getRealTime() >= (state.nextPoll or 0) then
        state.nextPoll = core.getRealTime() + POLL_INTERVAL
        if signature() ~= state.signature then
            state.dirty = true
        end
    end
    if state.dirty then
        state.dirty = false
        refresh()
    end
end

function dialogue.register()
    I.UI.registerWindow('Dialogue', show, hide, { capturesInput = true })
end

return dialogue
