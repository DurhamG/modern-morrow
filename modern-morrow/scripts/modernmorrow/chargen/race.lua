-- The race step of character creation. It replaces the engine's ChargenRace window with the fork's race screen: the
-- races on the left, the head and the appearance in the middle, and the race's description, skill bonuses and
-- specials on the right. Up and down change the race, Y the sex, RB the face and LB the hair. A is done, and B goes
-- back to the name. Both keep the choice, as the engine's window does.
--
-- The fork shows the head in 3D in the middle. Lua can't draw the head, so the middle has a plain panel with the
-- appearance below it.

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
local router = require('scripts.modernmorrow.input.router')
local flow = require('scripts.modernmorrow.chargen.flow')

local l10n = core.l10n('ModernMorrow')

local race = {}

local MODE = 'ChargenRace'
local MARGIN = style.space.margin
local TOP = 82
local ENTRY_HEIGHT = 44
local LIST_WIDTH = 230

local color = style.color
local size = style.textSize

local element = nil
local state = nil

local function L(key)
    return text.escape(l10n(key))
end

local function gmst(key, fallback)
    return text.escape(core.getGMST(key) or fallback)
end

local function playableRaces()
    local races = {}
    for _, record in ipairs(types.NPC.races.records) do
        if record.isPlayable then
            races[#races + 1] = record
        end
    end
    table.sort(races, function(a, b)
        return a.name < b.name
    end)
    return races
end

local function indexOf(list, value)
    for i, entry in ipairs(list) do
        if entry == value then
            return i
        end
    end
    return nil
end

-- The heads and hairs that the race and sex offer. A change of race or sex starts again from the first of each, as the
-- engine's window does.
local function countParts()
    local record = state.races[state.focus]
    state.heads = types.Player.getCharGenHeads(record.id, state.isMale)
    state.hairs = types.Player.getCharGenHairs(record.id, state.isMale)
    state.head = 1
    state.hair = 1
end

local function appearance(canvas, x, y, w, button, label, value)
    canvas:glyph(x, y + 4, 22, button)
    canvas:text(x + 32, y, 120, 30, label, size.normal, color.text)
    canvas:text(x + 32, y, w - 32, 30, value, size.normal, color.bright, ui.ALIGNMENT.End)
end

local function layout()
    local layerSize = layer.size(layer.menu)
    local w, h = layerSize.x, layerSize.y
    local canvas = draw.canvas()
    canvas:rect(0, 0, w, h, color.black)
    canvas:text(MARGIN, 20, w - 2 * MARGIN, 40, L('ChargenRaceTitle'), size.title, color.bright)
    canvas:rect(MARGIN, 66, w - 2 * MARGIN, 1, color.accent, 0.2)
    local bottom = h - 64

    -- The races on the left.
    local count = #state.races
    local visible = math.max(1, math.floor((bottom - TOP) / ENTRY_HEIGHT))
    state.scroll = math.max(1, math.min(state.focus, math.max(state.scroll, state.focus - visible + 1)))
    canvas:rect(MARGIN, TOP, LIST_WIDTH, bottom - TOP, color.panel, 0.9)
    for i = state.scroll, math.min(count, state.scroll + visible - 1) do
        local ry = TOP + 6 + (i - state.scroll) * ENTRY_HEIGHT
        local focused = i == state.focus
        if focused then
            canvas:rect(MARGIN + 4, ry, LIST_WIDTH - 8, ENTRY_HEIGHT - 4, color.accent, 0.16)
            canvas:rect(MARGIN + 4, ry, 3, ENTRY_HEIGHT - 4, color.accent)
        end
        canvas:text(MARGIN + 18, ry, LIST_WIDTH - 30, ENTRY_HEIGHT - 4,
            text.fit(state.races[i].name, LIST_WIDTH - 30, size.normal), size.normal,
            focused and color.active or color.bright)
    end

    -- Where the fork shows the head, and how it looks below it.
    local px = MARGIN + LIST_WIDTH + 20
    local previewH = math.min(bottom - TOP - 120, 400)
    local previewW = previewH
    canvas:rect(px, TOP, previewW, previewH, color.panel, 0.9)
    canvas:rect(px, TOP, previewW, 1, color.accent, 0.3)
    local ay = TOP + previewH + 12
    appearance(canvas, px, ay, previewW, 'y', L('ChargenSex'), state.isMale and L('ChargenMale') or L('ChargenFemale'))
    appearance(canvas, px, ay + 34, previewW, 'rb', L('ChargenFace'), state.head .. ' / ' .. #state.heads)
    appearance(canvas, px, ay + 68, previewW, 'lb', L('ChargenHair'), state.hair .. ' / ' .. #state.hairs)

    -- The race's description, skill bonuses and specials on the right.
    local dx = px + previewW + 24
    local dw = w - MARGIN - dx
    local record = state.races[state.focus]
    if record and dw > 100 then
        local ty = TOP
        canvas:text(dx, ty, dw, 34, text.fit(record.name, dw, size.large), size.large, color.bright)
        ty = ty + 38
        for _, line in ipairs(text.wrap(record.description or '', dw, size.small, 8)) do
            canvas:text(dx, ty, dw, 22, line, size.small, color.text)
            ty = ty + 22
        end
        ty = ty + 10
        canvas:text(dx, ty, dw, 24, gmst('sBonusSkillTitle', 'Skill Bonus'), size.small, color.dim)
        ty = ty + 26
        local bonuses = {}
        for skillId, bonus in pairs(record.skills) do
            local skill = core.stats.Skill.records[skillId]
            if skill then
                bonuses[#bonuses + 1] = { name = skill.name, bonus = bonus }
            end
        end
        table.sort(bonuses, function(a, b)
            return a.name < b.name
        end)
        for _, entry in ipairs(bonuses) do
            if ty + 24 <= bottom then
                canvas:text(dx, ty, dw, 24, text.escape(entry.name), size.normal, color.text)
                canvas:text(dx, ty, dw, 24, '+' .. entry.bonus, size.normal, color.better, ui.ALIGNMENT.End)
                ty = ty + 24
            end
        end
        ty = ty + 10
        if #record.spells > 0 and ty + 26 < bottom then
            canvas:text(dx, ty, dw, 24, gmst('sRaceMenu7', 'Specials'), size.small, color.dim)
            ty = ty + 26
            for _, spellId in ipairs(record.spells) do
                local spell = core.magic.spells.records[spellId]
                if spell and ty + 24 <= bottom then
                    canvas:text(dx, ty, dw, 24, text.fit(spell.name, dw, size.normal), size.normal, color.new)
                    ty = ty + 24
                end
            end
        end
    end

    hints.draw(canvas, MARGIN, w - 2 * MARGIN, h - 10, { { 'a', gmst('sDone', 'Done') } })

    local root = canvas:layout({ size = layerSize })
    root.layer = layer.menu
    return root
end

local function redraw()
    if element then
        element.layout = layout()
        element:update()
    end
end

-- Gives the player the race and appearance on the screen.
local function apply()
    local record = state.races[state.focus]
    types.Player.setCharGenRace(self, {
        race = record.id,
        isMale = state.isMale,
        head = state.heads[state.head],
        hair = state.hairs[state.hair],
    })
end

local function step(index, count, delta)
    if count == 0 then
        return 1
    end
    return (index - 1 + delta) % count + 1
end

race.handler = {
    onButton = function(name)
        if state == nil then
            return
        end
        if name == 'a' then
            ambient.playSound('menu click')
            apply()
            flow.done(MODE)
        elseif name == 'b' then
            apply()
            flow.go('ChargenName')
        elseif name == 'y' then
            state.isMale = not state.isMale
            countParts()
            ambient.playSound('menu click')
            redraw()
        elseif name == 'rb' then
            state.head = step(state.head, #state.heads, 1)
            ambient.playSound('menu click')
            redraw()
        elseif name == 'lb' then
            state.hair = step(state.hair, #state.hairs, 1)
            ambient.playSound('menu click')
            redraw()
        end
    end,
    onMove = function(_, dy)
        if state == nil or dy == 0 then
            return
        end
        local focus = math.max(1, math.min(#state.races, state.focus + dy))
        if focus ~= state.focus then
            state.focus = focus
            countParts()
            ambient.playSound('menu click')
            redraw()
        end
    end,
}

-- The screen starts from the player's race, sex, head and hair.
local function show()
    local player = types.NPC.record(self)
    state = { races = playableRaces(), focus = 1, scroll = 1, isMale = player.isMale }
    for i, record in ipairs(state.races) do
        if record.id == player.race then
            state.focus = i
        end
    end
    countParts()
    state.head = indexOf(state.heads, player.head) or 1
    state.hair = indexOf(state.hairs, player.hair) or 1
    router.push(race.handler)
    if element then
        element:destroy()
    end
    element = ui.create(layout())
end

local function hide()
    router.remove(race.handler)
    if element then
        element:destroy()
        element = nil
    end
    state = nil
end

function race.register()
    I.UI.registerWindow(MODE, show, hide, { capturesInput = true })
end

return race
