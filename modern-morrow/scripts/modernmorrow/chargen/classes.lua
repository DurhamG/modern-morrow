-- The class step of character creation, except the class form (see chargen/createclass.lua). It replaces three of
-- the engine's windows, as in the fork:
--
-- - ChargenClass asks how to find the class, in the question dialog (see ui/question.lua). B goes back to the race.
-- - ChargenClassGenerate asks the class questions in the question dialog, with their voice, and then shows the class
--   that the answers lead to as a list of one. A takes the class, and B takes it too and goes back to the first
--   question, as the engine's window does.
-- - ChargenClassPick lists the classes. A takes the highlighted one, and B takes it and goes back.

local core = require('openmw.core')
local self = require('openmw.self')
local types = require('openmw.types')
local ui = require('openmw.ui')
local vfs = require('openmw.vfs')
local ambient = require('openmw.ambient')
local I = require('openmw.interfaces')

local text = require('scripts.modernmorrow.ui.text')
local choice = require('scripts.modernmorrow.ui.choice')
local layer = require('scripts.modernmorrow.ui.layer')
local question = require('scripts.modernmorrow.ui.question')
local router = require('scripts.modernmorrow.input.router')
local flow = require('scripts.modernmorrow.chargen.flow')

local l10n = core.l10n('ModernMorrow')

local classes = {}

local SPECIALIZATIONS = { 'combat', 'magic', 'stealth' }

local function gmstRaw(key, fallback)
    return core.getGMST(key) or fallback
end

local function gmst(key, fallback)
    return text.escape(gmstRaw(key, fallback))
end

function classes.specializationName(specialization)
    local key = 'sSpecialization' .. specialization:sub(1, 1):upper() .. specialization:sub(2)
    return gmstRaw(key, specialization)
end

-- The class picture, or the warrior's when the class has none, as the engine's windows fall back.
function classes.picture(classId)
    local path = 'textures/levelup/' .. classId .. '.dds'
    if not vfs.fileExists(path) then
        path = 'textures/levelup/warrior.dds'
    end
    return path
end

-- Fills in a choice screen (see ui/choice.lua) with a class's specialization, favorite attributes, skills and
-- description.
function classes.describe(record, screen)
    screen.name = record.name
    screen.subtitle = classes.specializationName(record.specialization)
    screen.description = record.description
    screen.picture = classes.picture(record.id)
    local function names(ids, store)
        local lines = {}
        for _, id in ipairs(ids) do
            local entry = store.records[id]
            lines[#lines + 1] = entry and entry.name or id
        end
        return lines
    end
    screen.sections = {
        { gmst('sChooseClassMenu2', 'Favorite Attributes:'), names(record.attributes, core.stats.Attribute) },
        { gmst('sChooseClassMenu3', 'Major Skills:'), names(record.majorSkills, core.stats.Skill) },
        { gmst('sChooseClassMenu4', 'Minor Skills:'), names(record.minorSkills, core.stats.Skill) },
    }
end

local function showScreen(state, screen)
    local root = choice.draw(screen):layout({ size = layer.size(layer.menu) })
    root.layer = layer.menu
    if state.element then
        state.element.layout = root
        state.element:update()
    else
        state.element = ui.create(root)
    end
end

local function hideScreen(state)
    if state.element then
        state.element:destroy()
        state.element = nil
    end
end

-- How to find the class.

local choiceQuestion = nil

local function showChoice()
    choiceQuestion = question.open({
        message = '',
        answers = {
            gmstRaw('sClassChoiceMenu1', 'Answer questions'),
            gmstRaw('sClassChoiceMenu2', 'Pick from a list'),
            gmstRaw('sClassChoiceMenu3', 'Create a class'),
            gmstRaw('sBack', 'Back'),
        },
        cancel = 4,
        onAnswer = function(index)
            choiceQuestion = nil
            local modes = { 'ChargenClassGenerate', 'ChargenClassPick', 'ChargenClassCreate', 'ChargenRace' }
            flow.go(modes[index])
        end,
    })
end

local function hideChoice()
    if choiceQuestion then
        question.close(choiceQuestion)
        choiceQuestion = nil
    end
end

-- The class questions and the class they lead to.

local quiz = nil

local function askQuestion()
    local asked = quiz.questions[quiz.step]
    -- The answers show in a random order, as in the engine's window.
    local order = { 1, 2, 3 }
    for i = 3, 2, -1 do
        local j = math.random(i)
        order[i], order[j] = order[j], order[i]
    end
    local answers = {}
    for i, index in ipairs(order) do
        answers[i] = asked.answers[SPECIALIZATIONS[index]]
    end
    ambient.stopSay()
    if asked.sound and asked.sound ~= '' then
        ambient.say(asked.sound)
    end
    quiz.question = question.open({
        message = asked.question,
        answers = answers,
        onAnswer = function(index)
            local specialization = SPECIALIZATIONS[order[index]]
            quiz.counts[specialization] = quiz.counts[specialization] + 1
            quiz.question = nil
            quiz.step = quiz.step + 1
            if quiz.step > #quiz.questions then
                quiz.classId = types.Player.getCharGenClassForAnswers(quiz.counts)
                ambient.stopSay()
                router.push(classes.resultHandler)
                quiz.dirty = true
            else
                askQuestion()
            end
        end,
    })
end

local function drawResult()
    local record = types.NPC.classes.records[quiz.classId]
    local screen = {
        title = gmst('sMessageQuestionAnswer1', 'Your personality and past reflect a:'),
        choices = { record.name },
        focus = 1,
        scroll = {},
        -- B chooses another class here, rather than only going back, so it has a hint.
        hints = {
            { 'a', gmst('sMessageQuestionAnswer2', 'Accept this Class') },
            { 'b', gmst('sMessageQuestionAnswer3', 'Choose Another Class') },
        },
    }
    classes.describe(record, screen)
    showScreen(quiz, screen)
end

classes.resultHandler = {
    onButton = function(name)
        if quiz == nil or quiz.classId == nil then
            return
        end
        if name == 'a' or name == 'x' then
            ambient.playSound('menu click')
            types.Player.setCharGenClass(self, quiz.classId)
            flow.done('ChargenClassGenerate')
        elseif name == 'b' then
            types.Player.setCharGenClass(self, quiz.classId)
            flow.go('ChargenClass')
        end
    end,
}

local function showGenerate()
    quiz = {
        questions = types.Player.getCharGenClassQuestions(),
        step = 1,
        counts = { combat = 0, magic = 0, stealth = 0 },
    }
    askQuestion()
end

local function hideGenerate()
    ambient.stopSay()
    router.remove(classes.resultHandler)
    if quiz then
        if quiz.question then
            question.close(quiz.question)
        end
        hideScreen(quiz)
    end
    quiz = nil
end

-- The list of classes.

local pick = nil

-- The classes the player can pick: the playable ones, without a class that the player made up, by name.
function classes.playable()
    local list = {}
    for _, record in ipairs(types.NPC.classes.records) do
        if record.isPlayable and not record.isCustom then
            list[#list + 1] = record
        end
    end
    table.sort(list, function(a, b)
        return a.name < b.name
    end)
    return list
end

local function drawPick()
    local names = {}
    for i, record in ipairs(pick.list) do
        names[i] = record.name
    end
    local screen = {
        title = text.escape(l10n('ChargenClassTitle')),
        choices = names,
        focus = pick.focus,
        scroll = pick.scroll,
        hints = { { 'a', gmst('sDone', 'Done') } },
    }
    classes.describe(pick.list[pick.focus], screen)
    showScreen(pick, screen)
end

classes.pickHandler = {
    onButton = function(name)
        if pick == nil then
            return
        end
        if name == 'a' then
            ambient.playSound('menu click')
            types.Player.setCharGenClass(self, pick.list[pick.focus].id)
            flow.done('ChargenClassPick')
        elseif name == 'b' then
            types.Player.setCharGenClass(self, pick.list[pick.focus].id)
            flow.go('ChargenClass')
        end
    end,
    onMove = function(_, dy)
        if pick == nil or dy == 0 then
            return
        end
        local focus = math.max(1, math.min(#pick.list, pick.focus + dy))
        if focus ~= pick.focus then
            pick.focus = focus
            ambient.playSound('menu click')
            drawPick()
        end
    end,
}

-- The list starts on the player's class, or on the first class.
local function showPick()
    pick = { list = classes.playable(), focus = 1, scroll = {} }
    local current = types.NPC.record(self).class
    for i, record in ipairs(pick.list) do
        if record.id == current then
            pick.focus = i
        end
    end
    router.push(classes.pickHandler)
    drawPick()
end

local function hidePick()
    router.remove(classes.pickHandler)
    if pick then
        hideScreen(pick)
    end
    pick = nil
end

-- The class questions' result is drawn in onFrame, because the last answer can come from an event.
function classes.onFrame()
    if quiz and quiz.dirty then
        quiz.dirty = false
        drawResult()
    end
end

function classes.register()
    I.UI.registerWindow('ChargenClass', showChoice, hideChoice, { capturesInput = true })
    I.UI.registerWindow('ChargenClassGenerate', showGenerate, hideGenerate, { capturesInput = true })
    I.UI.registerWindow('ChargenClassPick', showPick, hidePick, { capturesInput = true })
end

return classes
