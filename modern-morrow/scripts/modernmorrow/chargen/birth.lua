-- The birth sign step of character creation. It replaces the engine's ChargenBirth window with the fork's choice
-- screen (see ui/choice.lua): the signs on the left, the sign's picture and description in the middle, and its
-- abilities, powers and spells with their effects on the right. A takes the highlighted sign, and B takes it and goes
-- back to the class, as the engine's window does.

local core = require('openmw.core')
local self = require('openmw.self')
local types = require('openmw.types')
local ui = require('openmw.ui')
local ambient = require('openmw.ambient')
local I = require('openmw.interfaces')

local text = require('scripts.modernmorrow.ui.text')
local choice = require('scripts.modernmorrow.ui.choice')
local layer = require('scripts.modernmorrow.ui.layer')
local style = require('scripts.modernmorrow.ui.style')
local router = require('scripts.modernmorrow.input.router')
local castables = require('scripts.modernmorrow.magic.castables')
local flow = require('scripts.modernmorrow.chargen.flow')

local l10n = core.l10n('ModernMorrow')

local birth = {}

local MODE = 'ChargenBirth'

local element = nil
local state = nil

local function gmst(key, fallback)
    return text.escape(core.getGMST(key) or fallback)
end

local function signs()
    local list = {}
    for _, record in ipairs(types.Player.birthSigns.records) do
        list[#list + 1] = record
    end
    table.sort(list, function(a, b)
        return a.name < b.name
    end)
    return list
end

-- The sign's spells by kind, each with its effects below it.
local function sections(record)
    local result = {}
    local SPELL_TYPE = core.magic.SPELL_TYPE
    local kinds = {
        { SPELL_TYPE.Ability, gmst('sBirthsignmenu1', 'Abilities:') },
        { SPELL_TYPE.Power, gmst('sPowers', 'Powers') },
        { SPELL_TYPE.Spell, gmst('sBirthsignmenu2', 'Spells:') },
    }
    for _, kind in ipairs(kinds) do
        local lines = {}
        for _, spellId in ipairs(record.spells) do
            local spell = core.magic.spells.records[spellId]
            if spell and spell.type == kind[1] then
                lines[#lines + 1] = { text = spell.name, color = style.color.new }
                for _, effect in ipairs(castables.describe(spell.effects)) do
                    lines[#lines + 1] = { text = effect, indent = 24 }
                end
            end
        end
        if #lines > 0 then
            result[#result + 1] = { kind[2], lines }
        end
    end
    return result
end

local function redraw()
    local names = {}
    for i, record in ipairs(state.list) do
        names[i] = record.name
    end
    local record = state.list[state.focus]
    local screen = {
        title = text.escape(l10n('ChargenBirthTitle')),
        choices = names,
        focus = state.focus,
        scroll = state.scroll,
        name = record and record.name,
        description = record and record.description,
        picture = record and record.texture ~= '' and record.texture or nil,
        sections = record and sections(record) or {},
        hints = { { 'a', gmst('sDone', 'Done') } },
    }
    local root = choice.draw(screen):layout({ size = layer.size(layer.menu) })
    root.layer = layer.menu
    if element then
        element.layout = root
        element:update()
    else
        element = ui.create(root)
    end
end

birth.handler = {
    onButton = function(name)
        if state == nil or #state.list == 0 then
            return
        end
        if name == 'a' then
            ambient.playSound('menu click')
            types.Player.setCharGenBirthSign(self, state.list[state.focus].id)
            flow.done(MODE)
        elseif name == 'b' then
            types.Player.setCharGenBirthSign(self, state.list[state.focus].id)
            flow.go('ChargenClass')
        end
    end,
    onMove = function(_, dy)
        if state == nil or dy == 0 then
            return
        end
        local focus = math.max(1, math.min(#state.list, state.focus + dy))
        if focus ~= state.focus then
            state.focus = focus
            ambient.playSound('menu click')
            redraw()
        end
    end,
}

-- The list starts on the player's sign, or on the first sign.
local function show()
    state = { list = signs(), focus = 1, scroll = {} }
    local current = types.Player.getBirthSign(self)
    for i, record in ipairs(state.list) do
        if record.id == current then
            state.focus = i
        end
    end
    router.push(birth.handler)
    if element then
        element:destroy()
        element = nil
    end
    redraw()
end

local function hide()
    router.remove(birth.handler)
    if element then
        element:destroy()
        element = nil
    end
    state = nil
end

function birth.register()
    I.UI.registerWindow(MODE, show, hide, { capturesInput = true })
end

return birth
