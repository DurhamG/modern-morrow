-- Replaces the Dialogue window: the conversation so far, the topics, the answers to a question,
-- the services, and the persuasion choices.

local async = require('openmw.async')
local core = require('openmw.core')
local self = require('openmw.self')
local types = require('openmw.types')
local ui = require('openmw.ui')
local util = require('openmw.util')

local style = require('scripts.uiproof.style')
local list = require('scripts.uiproof.list')

local dialogue = {}

local ROW_HEIGHT = 28
local LIST_WIDTH = 340
local HEIGHT = 640
local TRANSCRIPT_LINES = 14

local SERVICE_NAMES = {
    persuasion = 'sPersuasion',
    barter = 'sBarter',
    spells = 'sSpells',
    travel = 'sTravel',
    spellmaking = 'sSpellmakingMenuTitle',
    enchanting = 'sEnchanting',
    training = 'sServiceTrainingTitle',
    repair = 'sRepair',
    companionShare = 'sCompanionShare',
}

local PERSUASION = {
    { type = 'admire', name = 'sAdmire' },
    { type = 'intimidate', name = 'sIntimidate' },
    { type = 'taunt', name = 'sTaunt' },
    { type = 'bribe10', name = 'sBribe 10 Gold' },
    { type = 'bribe100', name = 'sBribe 100 Gold' },
    { type = 'bribe1000', name = 'sBribe 1000 Gold' },
}

local element = nil
local actor = nil
local view = 'main'
local status = nil
local rows = nil
local seen = nil

local function gmst(name, fallback)
    local value = core.getGMST(name)
    if type(value) == 'string' and value ~= '' then
        return value
    end
    return fallback
end

local function actorName()
    if not actor then
        return ''
    end
    return actor.type.record(actor).name
end

local function markup(line)
    local shown = types.Player.findTopicLinks(self, line.text)
    local text = shown.text
    local normal = style.colorCode(line.isMessage and style.dim or style.normal)
    local parts = { normal }
    local pos = 1
    for _, link in ipairs(shown.links) do
        parts[#parts + 1] = style.escape(text:sub(pos, link.first - 1))
        parts[#parts + 1] = style.colorCode(style.link) .. style.escape(text:sub(link.first, link.last)) .. normal
        pos = link.last + 1
    end
    parts[#parts + 1] = style.escape(text:sub(pos))
    local body = table.concat(parts)
    if line.title then
        body = style.colorCode(style.header) .. style.escape(line.title) .. '\n' .. body
    end
    return body
end

local function transcriptText(lines)
    local first = math.max(1, #lines - TRANSCRIPT_LINES + 1)
    local parts = {}
    for i = first, #lines do
        parts[#parts + 1] = markup(lines[i])
    end
    return table.concat(parts, '\n\n')
end

local function mainItems(choices)
    local items = {}
    if #choices > 0 then
        items[#items + 1] = { header = true, text = 'Your answer' }
        for _, choice in ipairs(choices) do
            items[#items + 1] = { key = 'choice:' .. choice.answer, kind = 'choice', answer = choice.answer, text = choice.text }
        end
        return items
    end
    if types.Player.mustSayGoodbye(self) then
        items[#items + 1] = { key = 'goodbye', kind = 'goodbye', text = gmst('sGoodbye', 'Goodbye') }
        return items
    end
    local services = types.Actor.getDialogueServices(actor)
    for _, service in ipairs(services) do
        items[#items + 1] = {
            key = 'service:' .. service,
            kind = 'service',
            service = service,
            text = gmst(SERVICE_NAMES[service] or '', service),
        }
    end
    items[#items + 1] = { header = true, text = gmst('sTopics', 'Topics') }
    for _, topic in ipairs(types.Player.getDialogueTopics(self)) do
        local color = nil
        if topic.specific then
            color = style.link
        elseif topic.exhausted then
            color = style.dim
        end
        items[#items + 1] = { key = 'topic:' .. topic.id, kind = 'topic', id = topic.id, text = topic.name, color = color }
    end
    return items
end

local function persuasionItems()
    local items = { { header = true, text = gmst('sPersuasionMenuTitle', 'Persuasion') } }
    for _, option in ipairs(PERSUASION) do
        local allowed = types.Player.canPersuadeDialogueActor(self, option.type)
        items[#items + 1] = {
            key = 'persuade:' .. option.type,
            kind = 'persuade',
            persuasion = option.type,
            allowed = allowed,
            text = gmst(option.name, option.type),
            color = (not allowed) and style.dim or nil,
        }
    end
    return items
end

local function snapshot()
    local lines = types.Player.getDialogueTranscript(self)
    local choices = types.Player.getDialogueChoices(self)
    return {
        lines = lines,
        choices = choices,
        key = string.format('%d:%d:%s:%d', #lines, #choices, tostring(types.Player.mustSayGoodbye(self)),
            #types.Player.getDialogueTopics(self)),
    }
end

local function layout(state)
    local layerSize = style.layerSize(style.menuLayer)
    local width = math.min(layerSize.x - 60, 1180)
    local transcriptWidth = width - LIST_WIDTH - 60
    local listRows = math.floor((HEIGHT - 120) / ROW_HEIGHT)
    rows.rows = listRows
    rows:ensureVisible()

    local title = actorName()
    if actor and types.NPC.objectIsInstance(actor) then
        title = string.format('%s    (%s %d)', title, gmst('sDisposition', 'Disposition'),
            types.NPC.getDisposition(actor, self))
    end

    local body = rows:layout(LIST_WIDTH, {})
    body.props.position = util.vector2(width - LIST_WIDTH - 20, 64)

    local content = {
        style.text(style.escape(title), {
            position = util.vector2(20, 16),
            textSize = style.titleSize,
            textColor = style.header,
        }),
        style.fill(style.frame, 1, {
            relativeSize = util.vector2(0, 0),
            position = util.vector2(width - LIST_WIDTH - 40, 64),
            size = util.vector2(2, HEIGHT - 100),
        }),
        style.text(transcriptText(state.lines), {
            position = util.vector2(20, 64),
            size = util.vector2(transcriptWidth, HEIGHT - 100),
            autoSize = false,
            wordWrap = true,
            multiline = true,
            textAlignV = ui.ALIGNMENT.End,
        }),
        body,
    }
    if status then
        content[#content + 1] = style.text(style.escape(status), {
            position = util.vector2(20, HEIGHT - 28),
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
            size = util.vector2(width, HEIGHT),
        },
        content = ui.content({ style.panel({ relativeSize = util.vector2(1, 1) }, content) }),
    }
end

local function refresh()
    if not element then
        return
    end
    actor = types.Player.getDialogueActor(self)
    if not actor then
        return
    end
    local state = snapshot()
    seen = state.key
    if view == 'persuade' and #state.choices > 0 then
        view = 'main'
    end
    if view == 'persuade' then
        rows:setItems(persuasionItems())
    else
        rows:setItems(mainItems(state.choices))
    end
    element.layout = layout(state)
    element:update()
end

local function onOutcome()
    refresh()
end

function dialogue.show()
    actor = types.Player.getDialogueActor(self)
    view = 'main'
    status = nil
    rows = rows or list.new(18, ROW_HEIGHT)
    if element then
        element:destroy()
    end
    element = ui.create({ layer = style.menuLayer, type = ui.TYPE.Widget })
    rows:setItems({})
    refresh()
end

function dialogue.hide()
    if element then
        element:destroy()
        element = nil
    end
    seen = nil
end

function dialogue.isVisible()
    return element ~= nil
end

local function activate(entry)
    status = nil
    local callback = async:callback(onOutcome)
    if entry.kind == 'topic' then
        types.Player.askDialogueTopic(self, entry.id, callback)
    elseif entry.kind == 'choice' then
        types.Player.answerDialogueChoice(self, entry.answer, callback)
    elseif entry.kind == 'goodbye' then
        types.Player.sayGoodbye(self)
    elseif entry.kind == 'service' then
        if entry.service == 'persuasion' then
            view = 'persuade'
            refresh()
        else
            types.Player.requestDialogueService(self, entry.service, callback)
        end
    elseif entry.kind == 'persuade' then
        if entry.allowed then
            types.Player.persuadeDialogueActor(self, entry.persuasion, async:callback(function(outcome)
                if outcome.success ~= nil then
                    status = string.format('%s: %s', entry.text, outcome.success and 'success' or 'failed')
                end
                refresh()
            end))
        end
    end
end

function dialogue.onAction(action)
    if action == 'up' then
        if rows:move(-1) then refresh() end
    elseif action == 'down' then
        if rows:move(1) then refresh() end
    elseif action == 'accept' then
        local entry = rows:current()
        if entry then
            activate(entry)
        end
    elseif action == 'back' then
        if view == 'persuade' then
            view = 'main'
            status = nil
            refresh()
            return
        end
        local ok, err = pcall(types.Player.sayGoodbye, self)
        if not ok then
            status = 'Answer the question first.'
            print('sayGoodbye refused: ' .. tostring(err))
            refresh()
        end
    end
end

-- The conversation can change without a callback, such as when a dialogue script adds a topic.
function dialogue.onFrame()
    if element and seen and snapshot().key ~= seen then
        refresh()
    end
end

return dialogue
