-- The class form of character creation. It replaces the engine's ChargenClassCreate window with the fork's custom
-- class screen: the class on the left, one line each for the name, the description, the specialization, the two
-- favorite attributes and the five major and five minor skills, and on the right the choices for the highlighted line,
-- with the current one marked and the skills grouped by specialization. Up and down move between the lines, and right
-- or A moves to the choices, where A picks one. Picking an attribute or a skill that the class has already swaps the
-- two, so the class never has one twice. A on the name or the description types it with the on-screen keyboard. X is
-- done, and asks for a name first when the class has none.
--
-- The form keeps what the player entered while the game runs, as the engine's window does.

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
local keyboard = require('scripts.modernmorrow.ui.keyboard')
local router = require('scripts.modernmorrow.input.router')
local flow = require('scripts.modernmorrow.chargen.flow')
local classes = require('scripts.modernmorrow.chargen.classes')

local l10n = core.l10n('ModernMorrow')

local createclass = {}

local MODE = 'ChargenClassCreate'
local MARGIN = style.space.margin
local TOP = 82
local LINE_HEIGHT = 30
local HEADER_HEIGHT = 28
local CHOICE_HEIGHT = 36
local SPECIALIZATIONS = { 'combat', 'magic', 'stealth' }

local color = style.color
local size = style.textSize

local function L(key)
    return text.escape(l10n(key))
end

local function gmst(key, fallback)
    return text.escape(core.getGMST(key) or fallback)
end

-- The class being made, with the engine window's starting choices.
local class = {
    name = '',
    description = '',
    specialization = 'combat',
    attributes = { 'strength', 'agility' },
    majorSkills = { 'block', 'armorer', 'mediumarmor', 'heavyarmor', 'bluntweapon' },
    minorSkills = { 'longblade', 'axe', 'spear', 'athletics', 'enchant' },
}

-- The lines on the left.
local LINES = {
    { kind = 'name' },
    { kind = 'description' },
    { kind = 'specialization' },
    { kind = 'attribute', index = 1 },
    { kind = 'attribute', index = 2 },
}
for i = 1, 5 do
    LINES[#LINES + 1] = { kind = 'major', index = i }
end
for i = 1, 5 do
    LINES[#LINES + 1] = { kind = 'minor', index = i }
end

local element = nil
local state = nil

local function currentId(line)
    if line.kind == 'attribute' then
        return class.attributes[line.index]
    elseif line.kind == 'major' then
        return class.majorSkills[line.index]
    elseif line.kind == 'minor' then
        return class.minorSkills[line.index]
    end
    return nil
end

local function hasChoices(line)
    return line.kind ~= 'name' and line.kind ~= 'description'
end

-- The choices for the highlighted line, with group headers for the skills. The highlight starts on the current one.
local function readChoices()
    local line = LINES[state.line]
    local rows = {}
    if line.kind == 'specialization' then
        for _, specialization in ipairs(SPECIALIZATIONS) do
            local skills = {}
            for _, skill in ipairs(core.stats.Skill.records) do
                if skill.specialization == specialization then
                    skills[#skills + 1] = skill.name
                end
            end
            rows[#rows + 1] = {
                specialization = specialization,
                name = classes.specializationName(specialization),
                description = l10n('ClassSpecializationHelp') .. ' ' .. table.concat(skills, ', '),
                current = specialization == class.specialization,
            }
        end
    elseif line.kind == 'attribute' then
        local other = class.attributes[3 - line.index]
        for _, attribute in ipairs(core.stats.Attribute.records) do
            rows[#rows + 1] = {
                id = attribute.id,
                icon = attribute.icon,
                name = attribute.name,
                description = attribute.description,
                current = attribute.id == currentId(line),
                marker = attribute.id == other and L('ClassChosen') or nil,
            }
        end
    elseif line.kind == 'major' or line.kind == 'minor' then
        for _, specialization in ipairs(SPECIALIZATIONS) do
            rows[#rows + 1] = { header = text.escape(classes.specializationName(specialization)) }
            for _, skill in ipairs(core.stats.Skill.records) do
                if skill.specialization == specialization then
                    local governing = core.stats.Attribute.records[skill.attribute]
                    local row = {
                        id = skill.id,
                        icon = skill.icon,
                        name = skill.name,
                        detail = governing and governing.name,
                        description = skill.description,
                        current = skill.id == currentId(line),
                    }
                    if not row.current then
                        for i = 1, 5 do
                            if class.majorSkills[i] == skill.id then
                                row.marker = gmst('sSkillClassMajor', 'Major Skills')
                            elseif class.minorSkills[i] == skill.id then
                                row.marker = gmst('sSkillClassMinor', 'Minor Skills')
                            end
                        end
                    end
                    rows[#rows + 1] = row
                end
            end
        end
    end
    state.choices = rows
    state.choice = 1
    state.choiceScroll = 1
    local index = 0
    for _, row in ipairs(rows) do
        if not row.header then
            index = index + 1
            if row.current then
                state.choice = index
            end
        end
    end
end

local function choiceCount()
    local count = 0
    for _, row in ipairs(state.choices) do
        if not row.header then
            count = count + 1
        end
    end
    return count
end

local function focusedChoice()
    local index = 0
    for _, row in ipairs(state.choices) do
        if not row.header then
            index = index + 1
            if index == state.choice then
                return row
            end
        end
    end
    return nil
end

local function drawLines(canvas, x, y, w, h)
    canvas:rect(x, y, w, h, color.panel, 0.9)
    local ry = y + 10
    local previous = 'name'
    for i, line in ipairs(LINES) do
        -- A heading above each group of attributes and skills.
        if line.kind ~= previous and (line.kind == 'attribute' or line.kind == 'major' or line.kind == 'minor') then
            local heading
            if line.kind == 'attribute' then
                heading = gmst('sChooseClassMenu2', 'Favorite Attributes:')
            elseif line.kind == 'major' then
                heading = gmst('sSkillClassMajor', 'Major Skills')
            else
                heading = gmst('sSkillClassMinor', 'Minor Skills')
            end
            canvas:text(x + 16, ry + 4, w - 32, 22, heading, size.small, color.dim)
            canvas:rect(x + 16, ry + 26, w - 32, 1, color.accent, 0.25)
            ry = ry + HEADER_HEIGHT + 2
        end
        previous = line.kind

        local focused = i == state.line
        if focused then
            canvas:rect(x + 4, ry, w - 8, LINE_HEIGHT - 2, color.accent, state.inChoices and 0.08 or 0.16)
            canvas:rect(x + 4, ry, 3, LINE_HEIGHT - 2, color.accent)
        end
        local valueColor = (focused and not state.inChoices) and color.active or color.bright
        local label, value, icon
        if line.kind == 'name' then
            label = gmst('sName', 'Name')
            if class.name == '' then
                value = L('ClassNoName')
                valueColor = color.dim
            else
                value = text.fit(class.name, w - 180, size.normal)
            end
        elseif line.kind == 'description' then
            label = L('ClassDescription')
            value = class.description == '' and '-' or text.fit(class.description, w - 180, size.normal)
        elseif line.kind == 'specialization' then
            label = gmst('sChooseClassMenu1', 'Specialization:')
            value = text.escape(classes.specializationName(class.specialization))
        else
            local store = line.kind == 'attribute' and core.stats.Attribute or core.stats.Skill
            local record = store.records[currentId(line)]
            value = record and text.escape(record.name) or ''
            icon = record and record.icon
        end
        if label then
            canvas:text(x + 20, ry, 140, LINE_HEIGHT - 2, label, size.normal, color.text)
            canvas:text(x + 160, ry, w - 180, LINE_HEIGHT - 2, value, size.normal, valueColor)
        else
            if icon and icon ~= '' then
                canvas:image(x + 20, ry + 1, LINE_HEIGHT - 4, LINE_HEIGHT - 4, draw.texture(icon))
            end
            canvas:text(x + 24 + LINE_HEIGHT, ry, w - 44 - LINE_HEIGHT, LINE_HEIGHT - 2, value, size.normal,
                valueColor)
        end
        ry = ry + LINE_HEIGHT
        if line.kind == 'specialization' then
            ry = ry + 4
        end
    end
end

local function drawChoices(canvas, x, y, w, h)
    canvas:rect(x, y, w, h, color.panel, 0.9)
    canvas:rect(x, y, w, 1, color.accent, 0.3)
    local line = LINES[state.line]

    local list = {}
    if state.inChoices then
        list[#list + 1] = { 'a', L('EnchantChoose') }
    elseif hasChoices(line) then
        list[#list + 1] = { 'a', L('EnchantChange') }
    else
        list[#list + 1] = { 'a', L('EnchantEdit') }
    end
    list[#list + 1] = { 'x', gmst('sDone', 'Done') }
    local hintsTop = hints.draw(canvas, x + 16, w - 32, y + h, list)

    -- The name and the description are typed, so there is nothing to pick from.
    if not hasChoices(line) then
        local help = 'ClassDescriptionHelp'
        if line.kind == 'name' then
            help = class.name == '' and 'ClassNameHelp' or 'ClassNameChangeHelp'
        end
        local ty = y + 16
        for _, s in ipairs(text.wrap(l10n(help), w - 32, size.normal, 4)) do
            canvas:text(x + 16, ty, w - 32, 26, s, size.normal, color.text)
            ty = ty + 26
        end
        return
    end

    local title
    if line.kind == 'specialization' then
        title = gmst('sChooseClassMenu1', 'Specialization:')
    elseif line.kind == 'attribute' then
        title = gmst('sChooseClassMenu2', 'Favorite Attributes:')
    elseif line.kind == 'major' then
        title = gmst('sSkillClassMajor', 'Major Skills')
    else
        title = gmst('sSkillClassMinor', 'Minor Skills')
    end
    canvas:text(x + 16, y + 8, w - 32, 30, title, size.large, color.bright)

    -- The highlighted choice's description at the bottom, while the choices have the input.
    local descriptionTop = hintsTop - 12
    local focused = focusedChoice()
    if focused and focused.description and focused.description ~= '' then
        local lines = text.wrap(focused.description, w - 32, size.small, 5)
        descriptionTop = descriptionTop - #lines * 21
        local ty = descriptionTop
        for _, s in ipairs(lines) do
            canvas:text(x + 16, ty, w - 32, 21, s, size.small, color.dim)
            ty = ty + 21
        end
        descriptionTop = descriptionTop - 10
    end

    -- The choices, scrolled so that the highlighted one shows.
    local listTop = y + 44
    local listBottom = descriptionTop
    local focusRow = 1
    local index = 0
    for i, row in ipairs(state.choices) do
        if not row.header then
            index = index + 1
            if index == state.choice then
                focusRow = i
                break
            end
        end
    end
    local function rowHeight(i)
        return state.choices[i].header and HEADER_HEIGHT or CHOICE_HEIGHT
    end
    if focusRow < state.choiceScroll then
        local before = state.choices[focusRow - 1]
        state.choiceScroll = (before and before.header) and focusRow - 1 or focusRow
    end
    while state.choiceScroll < focusRow do
        local bottomOfFocus = listTop
        for i = state.choiceScroll, focusRow do
            bottomOfFocus = bottomOfFocus + rowHeight(i)
        end
        if bottomOfFocus <= listBottom then
            break
        end
        state.choiceScroll = state.choiceScroll + 1
    end

    local ry = listTop
    index = 0
    for i = 1, state.choiceScroll - 1 do
        if not state.choices[i].header then
            index = index + 1
        end
    end
    for i = state.choiceScroll, #state.choices do
        local row = state.choices[i]
        if ry + rowHeight(i) > listBottom then
            break
        end
        if row.header then
            canvas:text(x + 12, ry + 4, w - 24, 22, row.header, size.small, color.bright)
            canvas:rect(x + 12, ry + 26, w - 24, 1, color.accent, 0.25)
            ry = ry + HEADER_HEIGHT
        else
            index = index + 1
            local highlighted = state.inChoices and index == state.choice
            if highlighted then
                canvas:rect(x + 4, ry, w - 8, CHOICE_HEIGHT - 2, color.accent, 0.16)
                canvas:rect(x + 4, ry, 3, CHOICE_HEIGHT - 2, color.accent)
            end
            local tx = x + 20
            if row.icon and row.icon ~= '' then
                canvas:image(tx, ry + 3, CHOICE_HEIGHT - 8, CHOICE_HEIGHT - 8, draw.texture(row.icon))
                tx = tx + CHOICE_HEIGHT
            end
            local caption = text.escape(row.name)
            local nameColor = highlighted and color.active or (row.current and color.new or color.bright)
            canvas:text(tx, ry, x + w - 16 - tx, CHOICE_HEIGHT - 2, caption, size.normal, nameColor)
            if row.detail then
                canvas:text(tx + text.width(caption, size.normal) + 12, ry + 2, 200, CHOICE_HEIGHT - 4,
                    text.escape(row.detail), size.small, color.dim)
            end
            local marker = row.current and L('ClassCurrent') or row.marker
            if marker then
                canvas:text(x + 16, ry, w - 32, CHOICE_HEIGHT - 2, marker, size.small,
                    row.current and color.new or color.dim, ui.ALIGNMENT.End)
            end
            ry = ry + CHOICE_HEIGHT
        end
    end
end

local function layout()
    local layerSize = layer.size(layer.menu)
    local w, h = layerSize.x, layerSize.y
    local canvas = draw.canvas()
    canvas:rect(0, 0, w, h, color.black)
    canvas:text(MARGIN, 20, w - 2 * MARGIN, 40, L('ClassCreateTitle'), size.title, color.bright)
    canvas:rect(MARGIN, 66, w - 2 * MARGIN, 1, color.accent, 0.2)
    local bottom = h - style.space.contentBottomMargin
    local leftW = math.floor((w - 2 * MARGIN - 16) * 45 / 100)
    drawLines(canvas, MARGIN, TOP, leftW, bottom - TOP)
    local px = MARGIN + leftW + 16
    drawChoices(canvas, px, TOP, w - MARGIN - px, bottom - TOP)
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

-- Puts `id` into `list` at `index`. If the list, or the `other` list, has it already, that place takes the old one.
local function swapInto(list, index, id, other)
    local old = list[index]
    for i, existing in ipairs(list) do
        if existing == id and i ~= index then
            list[i] = old
        end
    end
    if other then
        for i, existing in ipairs(other) do
            if existing == id then
                other[i] = old
            end
        end
    end
    list[index] = id
end

local function pick(row)
    local line = LINES[state.line]
    if line.kind == 'specialization' then
        class.specialization = row.specialization
    elseif line.kind == 'attribute' then
        swapInto(class.attributes, line.index, row.id)
    elseif line.kind == 'major' then
        swapInto(class.majorSkills, line.index, row.id, class.minorSkills)
    elseif line.kind == 'minor' then
        swapInto(class.minorSkills, line.index, row.id, class.majorSkills)
    end
    ambient.playSound('menu click')
    state.inChoices = false
    readChoices()
end

local function editText(description)
    keyboard.open({
        label = description and l10n('ClassDescription') or (core.getGMST('sName') or 'Name'),
        text = description and class.description or class.name,
        allowEmpty = description,
        onDone = function(typed)
            if description then
                class.description = typed
            else
                class.name = typed
            end
            if state then
                readChoices()
                redraw()
            end
        end,
    })
end

-- The class as setCharGenClass takes it.
local function classTable()
    return {
        name = class.name,
        description = class.description,
        specialization = class.specialization,
        attributes = { class.attributes[1], class.attributes[2] },
        majorSkills = { table.unpack(class.majorSkills) },
        minorSkills = { table.unpack(class.minorSkills) },
    }
end

local function finish()
    if class.name == '' then
        editText(false)
        return
    end
    ambient.playSound('menu click')
    types.Player.setCharGenClass(self, classTable())
    flow.done(MODE)
end

createclass.handler = {
    onButton = function(name)
        if state == nil then
            return
        end
        local line = LINES[state.line]
        if name == 'a' then
            if state.inChoices then
                local row = focusedChoice()
                if row then
                    pick(row)
                end
            elseif hasChoices(line) then
                state.inChoices = true
                ambient.playSound('menu click')
            else
                editText(line.kind == 'description')
                return
            end
            redraw()
        elseif name == 'b' then
            if state.inChoices then
                state.inChoices = false
                readChoices()
                redraw()
            else
                -- The engine's window keeps the class when the player goes back, if the class can be kept.
                if class.name ~= '' then
                    types.Player.setCharGenClass(self, classTable())
                end
                flow.go('ChargenClass')
            end
        elseif name == 'x' then
            finish()
        end
    end,
    onMove = function(dx, dy)
        if state == nil then
            return
        end
        local line = LINES[state.line]
        if dx > 0 and not state.inChoices and hasChoices(line) then
            state.inChoices = true
        elseif dx < 0 and state.inChoices then
            state.inChoices = false
        elseif dy ~= 0 and state.inChoices then
            state.choice = math.max(1, math.min(choiceCount(), state.choice + dy))
        elseif dy ~= 0 then
            local focus = math.max(1, math.min(#LINES, state.line + dy))
            if focus == state.line then
                return
            end
            state.line = focus
            readChoices()
        else
            return
        end
        ambient.playSound('menu click')
        redraw()
    end,
}

-- When the player has a made-up class already, the form starts from it, so it survives loading a game.
local function readPlayerClass()
    local record = types.NPC.classes.records[types.NPC.record(self).class]
    if record == nil or not record.isCustom then
        return
    end
    local function copy(list)
        local result = {}
        for _, id in ipairs(list) do
            result[#result + 1] = id
        end
        return result
    end
    class.name = record.name
    class.description = record.description or ''
    class.specialization = record.specialization
    class.attributes = copy(record.attributes)
    class.majorSkills = copy(record.majorSkills)
    class.minorSkills = copy(record.minorSkills)
end

local function show()
    readPlayerClass()
    state = { line = 1, inChoices = false }
    readChoices()
    router.push(createclass.handler)
    if element then
        element:destroy()
    end
    element = ui.create(layout())
end

local function hide()
    keyboard.close()
    router.remove(createclass.handler)
    if element then
        element:destroy()
        element = nil
    end
    state = nil
end

function createclass.register()
    I.UI.registerWindow(MODE, show, hide, { capturesInput = true })
end

return createclass
