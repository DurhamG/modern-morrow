-- The review at the end of character creation. It replaces the engine's ChargenReview window with the fork's review
-- screen: the name, race, class and birth sign on the left with health, magicka and fatigue below them, the
-- attributes and the race's and the sign's specials in the middle, and the skills on the right. Up and down move
-- between the four choices, A goes back to the highlighted one's step, which comes back here, and X finishes. B goes
-- back to the birth sign, as the engine's window does.
--
-- The choices take effect at the end of the frame they are made in, so the screen redraws when the numbers change.

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
local router = require('scripts.modernmorrow.input.router')
local flow = require('scripts.modernmorrow.chargen.flow')

local l10n = core.l10n('ModernMorrow')

local review = {}

local MODE = 'ChargenClassReview'
local MARGIN = style.space.margin
local TOP = 82
local CHOICE_HEIGHT = 56
local LEFT_WIDTH = 300
local MIDDLE_WIDTH = 240

local color = style.color
local size = style.textSize

-- The bars in the colours of the HUD.
local DYNAMIC = {
    { 'health', 'sHealth', 'Health', util.color.rgb(0.78, 0.24, 0.12) },
    { 'magicka', 'sMagic', 'Magicka', util.color.rgb(0.21, 0.33, 0.68) },
    { 'fatigue', 'sFatigue', 'Fatigue', util.color.rgb(0, 0.59, 0.24) },
}

-- The steps that the four choices go back to.
local STEPS = { 'ChargenName', 'ChargenRace', 'ChargenClass', 'ChargenBirth' }

local element = nil
local state = nil
-- The highlighted choice, kept while the player is away at its step.
local lastFocus = 1

local function L(key)
    return text.escape(l10n(key))
end

local function gmst(key, fallback)
    return text.escape(core.getGMST(key) or fallback)
end

local function recordName(store, id)
    local record = id and id ~= '' and store.records[id]
    return record and record.name or ''
end

-- What the screen shows, read from the player.
local function read()
    local npc = types.NPC.record(self)
    local classRecord = types.NPC.classes.records[npc.class]
    local signId = types.Player.getBirthSign(self)
    local values = {
        choices = {
            npc.name,
            recordName(types.NPC.races, npc.race),
            classRecord and classRecord.name or '',
            recordName(types.Player.birthSigns, signId),
        },
        dynamic = {},
        attributes = {},
        skills = {},
        specials = {},
        major = classRecord and classRecord.majorSkills or {},
        minor = classRecord and classRecord.minorSkills or {},
    }
    for i, entry in ipairs(DYNAMIC) do
        local stat = types.Actor.stats.dynamic[entry[1]](self)
        values.dynamic[i] = { math.floor(stat.current), math.floor(stat.base + stat.modifier) }
    end
    for _, attribute in ipairs(core.stats.Attribute.records) do
        values.attributes[#values.attributes + 1] = {
            name = attribute.name,
            value = math.floor(types.Actor.stats.attributes[attribute.id](self).modified),
        }
    end
    for _, skill in ipairs(core.stats.Skill.records) do
        values.skills[skill.id] = math.floor(types.NPC.stats.skills[skill.id](self).modified)
    end
    local race = types.NPC.races.records[npc.race]
    local sign = signId ~= '' and types.Player.birthSigns.records[signId]
    for _, owner in ipairs({ race or false, sign or false }) do
        if owner then
            for _, spellId in ipairs(owner.spells) do
                local spell = core.magic.spells.records[spellId]
                if spell then
                    values.specials[#values.specials + 1] = spell.name
                end
            end
        end
    end
    return values
end

-- A short summary of the values, to tell when they change.
local function signature(values)
    local parts = {}
    for _, s in ipairs(values.choices) do
        parts[#parts + 1] = s
    end
    for _, d in ipairs(values.dynamic) do
        parts[#parts + 1] = d[1] .. '/' .. d[2]
    end
    for _, a in ipairs(values.attributes) do
        parts[#parts + 1] = a.value
    end
    for _, skill in ipairs(core.stats.Skill.records) do
        parts[#parts + 1] = values.skills[skill.id]
    end
    parts[#parts + 1] = table.concat(values.specials, ',')
    return table.concat(parts, '|')
end

local function layout()
    local values = state.values
    local layerSize = layer.size(layer.menu)
    local w, h = layerSize.x, layerSize.y
    local canvas = draw.canvas()
    canvas:rect(0, 0, w, h, color.black)
    canvas:text(MARGIN, 20, w - 2 * MARGIN, 40, L('ChargenReviewTitle'), size.title, color.bright)
    canvas:rect(MARGIN, 66, w - 2 * MARGIN, 1, color.accent, 0.2)
    local bottom = h - 64

    -- The choices made so far on the left, which A goes back to.
    local labels = { gmst('sName', 'Name'), gmst('sRace', 'Race'), gmst('sClass', 'Class'),
        gmst('sBirthSign', 'Sign') }
    local ty = TOP
    for i, label in ipairs(labels) do
        local focused = i == state.focus
        canvas:rect(MARGIN, ty, LEFT_WIDTH, CHOICE_HEIGHT - 6, color.panel, 0.9)
        if focused then
            canvas:rect(MARGIN, ty, LEFT_WIDTH, CHOICE_HEIGHT - 6, color.accent, 0.16)
            canvas:rect(MARGIN, ty, 3, CHOICE_HEIGHT - 6, color.accent)
        end
        canvas:text(MARGIN + 14, ty + 2, LEFT_WIDTH - 28, 20, label, size.small, color.dim)
        canvas:text(MARGIN + 14, ty + 20, LEFT_WIDTH - 28, 28, text.fit(values.choices[i], LEFT_WIDTH - 28, size.normal),
            size.normal, focused and color.active or color.bright)
        ty = ty + CHOICE_HEIGHT
    end

    -- Health, magicka and fatigue below them.
    ty = ty + 10
    for i, entry in ipairs(DYNAMIC) do
        local current, full = values.dynamic[i][1], values.dynamic[i][2]
        canvas:text(MARGIN, ty, LEFT_WIDTH, 26, gmst(entry[2], entry[3]), size.normal, color.text)
        canvas:text(MARGIN, ty, LEFT_WIDTH, 26, current .. ' / ' .. full, size.normal, color.bright, ui.ALIGNMENT.End)
        canvas:bar(MARGIN, ty + 28, LEFT_WIDTH, 8, full > 0 and current / full or 0, entry[4])
        ty = ty + 48
    end

    -- The attributes in the middle, with the specials below them.
    local mx = MARGIN + LEFT_WIDTH + 32
    ty = TOP
    canvas:text(mx, ty, MIDDLE_WIDTH, 24, L('StatsAttributes'), size.small, color.dim)
    ty = ty + 28
    for _, attribute in ipairs(values.attributes) do
        canvas:text(mx, ty, MIDDLE_WIDTH, 26, text.escape(attribute.name), size.normal, color.text)
        canvas:text(mx, ty, MIDDLE_WIDTH, 26, tostring(attribute.value), size.normal, color.bright, ui.ALIGNMENT.End)
        ty = ty + 28
    end
    if #values.specials > 0 then
        ty = ty + 12
        canvas:text(mx, ty, MIDDLE_WIDTH, 24, gmst('sRaceMenu7', 'Specials'), size.small, color.dim)
        ty = ty + 28
        for _, special in ipairs(values.specials) do
            if ty + 26 > bottom then
                break
            end
            canvas:text(mx, ty, MIDDLE_WIDTH, 26, text.fit(special, MIDDLE_WIDTH, size.normal), size.normal, color.new)
            ty = ty + 26
        end
    end

    -- The skills on the right: the major and minor skills in one column, and the others in the next.
    local rx = mx + MIDDLE_WIDTH + 40
    local columnW = math.floor((w - MARGIN - rx - 24) / 2)
    local function skillGroup(x, y, heading, ids)
        canvas:text(x, y, columnW, 24, heading, size.small, color.dim)
        y = y + 26
        for _, id in ipairs(ids) do
            local skill = core.stats.Skill.records[id]
            if skill and y + 24 <= bottom then
                canvas:text(x, y, columnW, 24, text.fit(skill.name, columnW - 40, size.normal), size.normal,
                    color.text)
                canvas:text(x, y, columnW, 24, tostring(values.skills[id] or 0), size.normal, color.bright,
                    ui.ALIGNMENT.End)
                y = y + 24
            end
        end
        return y + 12
    end
    local misc = {}
    local onClass = {}
    for _, id in ipairs(values.major) do
        onClass[id] = true
    end
    for _, id in ipairs(values.minor) do
        onClass[id] = true
    end
    for _, skill in ipairs(core.stats.Skill.records) do
        if not onClass[skill.id] then
            misc[#misc + 1] = skill.id
        end
    end
    local sy = skillGroup(rx, TOP, gmst('sSkillClassMajor', 'Major Skills'), values.major)
    skillGroup(rx, sy, gmst('sSkillClassMinor', 'Minor Skills'), values.minor)
    skillGroup(rx + columnW + 24, TOP, gmst('sSkillClassMisc', 'Misc Skills'), misc)

    hints.draw(canvas, MARGIN, w - 2 * MARGIN, h - 10,
        { { 'a', L('ChargenChange') }, { 'x', gmst('sDone', 'Done') } })

    local root = canvas:layout({ size = layerSize })
    root.layer = layer.menu
    return root
end

local function redraw()
    if element then
        element.layout = layout()
        element:update()
    else
        element = ui.create(layout())
    end
end

review.handler = {
    onButton = function(name)
        if state == nil then
            return
        end
        if name == 'a' then
            ambient.playSound('menu click')
            lastFocus = state.focus
            flow.fromReview(STEPS[state.focus])
        elseif name == 'x' then
            ambient.playSound('menu click')
            I.UI.removeMode(MODE)
        elseif name == 'b' then
            lastFocus = state.focus
            flow.fromReview('ChargenBirth')
        end
    end,
    onMove = function(_, dy)
        if state == nil or dy == 0 then
            return
        end
        local focus = math.max(1, math.min(#STEPS, state.focus + dy))
        if focus ~= state.focus then
            state.focus = focus
            ambient.playSound('menu click')
            redraw()
        end
    end,
}

-- Redraws when the player's numbers change, such as at the end of the frame that made the last choice.
function review.onFrame()
    if state == nil then
        return
    end
    local values = read()
    local key = signature(values)
    if key ~= state.signature then
        state.values = values
        state.signature = key
        redraw()
    end
end

local function show()
    state = { focus = lastFocus }
    state.values = read()
    state.signature = signature(state.values)
    router.push(review.handler)
    if element then
        element:destroy()
        element = nil
    end
    redraw()
end

local function hide()
    router.remove(review.handler)
    if element then
        element:destroy()
        element = nil
    end
    state = nil
end

function review.register()
    I.UI.registerWindow('ChargenReview', show, hide, { capturesInput = true })
end

return review
