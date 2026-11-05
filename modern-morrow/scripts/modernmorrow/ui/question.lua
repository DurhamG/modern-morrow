-- Asks the player a question with a list of answers, as the fork's controller dialog does: a script's message box with
-- buttons, the soul gem's "recharge or enchant", or persuasion. It is a panel in the middle of the screen, over a
-- darkened view of what is behind it. The message comes first, then the answers as rows, and long answers wrap. Up and
-- down move the highlight, A picks the highlighted answer, and B picks the cancel answer when there is one.
--
-- The dialog draws in onFrame, because a question can arrive in an engine handler or an event, which may run on the
-- Lua worker thread, where text can't be measured.

local core = require('openmw.core')
local ui = require('openmw.ui')
local ambient = require('openmw.ambient')

local draw = require('scripts.modernmorrow.ui.draw')
local layer = require('scripts.modernmorrow.ui.layer')
local style = require('scripts.modernmorrow.ui.style')
local text = require('scripts.modernmorrow.ui.text')
local hints = require('scripts.modernmorrow.ui.hints')
local router = require('scripts.modernmorrow.input.router')

local l10n = core.l10n('ModernMorrow')

local question = {}

local WIDTH = 760
local ANSWER_HEIGHT = 46
local LINE_HEIGHT = style.textSize.normal + 8
local MAX_MESSAGE_LINES = 14
local MAX_ANSWER_LINES = 3

local element = nil
local current = nil

local function layout()
    local size = layer.size(layer.modal)
    local canvas = draw.canvas()
    local color = style.color
    local textSize = style.textSize.normal

    canvas:rect(0, 0, size.x, size.y, color.black, 0.55)
    local w = math.min(WIDTH, size.x - 2 * style.space.margin)
    local innerW = w - 48
    local lines = {}
    for paragraph in (current.message .. '\n'):gmatch('(.-)\n') do
        if #lines >= MAX_MESSAGE_LINES then
            break
        end
        if paragraph:match('%S') then
            for _, line in ipairs(text.wrap(paragraph, innerW, textSize, MAX_MESSAGE_LINES - #lines)) do
                lines[#lines + 1] = line
            end
        elseif #lines > 0 then
            lines[#lines + 1] = ''
        end
    end
    while #lines > 0 and lines[#lines] == '' do
        lines[#lines] = nil
    end
    local messageH = #lines * LINE_HEIGHT
    local answerLines = {}
    local answersH = 0
    for i, answer in ipairs(current.answers) do
        answerLines[i] = text.wrap(answer, w - 64, textSize, MAX_ANSWER_LINES)
        answersH = answersH + ANSWER_HEIGHT + (#answerLines[i] - 1) * LINE_HEIGHT
    end
    local h = 24 + messageH + (#lines > 0 and 18 or 0) + answersH + 18 + 40
    local x = math.floor((size.x - w) / 2)
    local y = math.max(style.space.margin, math.floor((size.y - h) / 2))
    canvas:rect(x, y, w, h, color.modal)
    canvas:rect(x, y, w, 1, color.accent, 0.6)

    local ty = y + 24
    for _, line in ipairs(lines) do
        canvas:text(x + 24, ty, innerW, LINE_HEIGHT, line, textSize, color.bright)
        ty = ty + LINE_HEIGHT
    end
    if #lines > 0 then
        ty = ty + 8
        canvas:rect(x + 24, ty, innerW, 1, color.accent, 0.2)
        ty = ty + 10
    end

    for i, answer in ipairs(answerLines) do
        local focused = i == current.focus
        local rowH = ANSWER_HEIGHT + (#answer - 1) * LINE_HEIGHT
        if focused then
            canvas:rect(x + 16, ty, w - 32, rowH - 4, color.accent, 0.16)
            canvas:rect(x + 16, ty, 3, rowH - 4, color.accent)
        end
        local lineY = ty + math.floor((ANSWER_HEIGHT - 4 - LINE_HEIGHT) / 2)
        for _, line in ipairs(answer) do
            canvas:text(x + 32, lineY, w - 64, LINE_HEIGHT, line, textSize, focused and color.active or color.text)
            lineY = lineY + LINE_HEIGHT
        end
        ty = ty + rowH
    end

    if #current.answers > 0 then
        hints.draw(canvas, x + 24, w - 48, y + h - 4, { { 'a', text.escape(l10n('Select')) } })
    end
    local root = canvas:layout({ size = size })
    root.layer = layer.modal
    return root
end

local function close()
    router.remove(question.handler)
    if element then
        element:destroy()
        element = nil
    end
    current = nil
end

local function pick(index)
    local asked = current
    close()
    ambient.playSound('menu click')
    asked.onAnswer(index)
end

question.handler = {
    onButton = function(name)
        if current == nil or #current.answers == 0 then
            return
        end
        if name == 'a' then
            pick(current.focus)
        elseif name == 'b' and current.cancel then
            pick(current.cancel)
        elseif name == 'b' and current.onCancel then
            local onCancel = current.onCancel
            close()
            onCancel()
        end
    end,
    onMove = function(_, dy)
        if current == nil or dy == 0 then
            return
        end
        local focus = math.max(1, math.min(#current.answers, current.focus + dy))
        if focus ~= current.focus then
            current.focus = focus
            current.dirty = true
            ambient.playSound('menu click')
        end
    end,
}

-- Asks a question. `options` has `message` and `answers`, as plain text, and `onAnswer(index)`, which is called with
-- the index of the answer the player picked. `focus` is the answer highlighted first, 1 by default, and `cancel` the
-- answer that B picks, if any. Without a `cancel`, B closes the question without an answer when `onCancel()` is given,
-- and calls it. A new question replaces one that wasn't answered yet, without calling its onAnswer. Returns a handle
-- for question.close.
function question.open(options)
    current = {
        message = options.message or '',
        answers = options.answers,
        onAnswer = options.onAnswer,
        onCancel = options.onCancel,
        focus = math.max(1, math.min(#options.answers, options.focus or 1)),
        cancel = options.cancel,
        dirty = true,
    }
    router.push(question.handler)
    return current
end

function question.isOpen()
    return current ~= nil
end

-- Closes the question that question.open returned the handle of, without an answer, if it is still open.
function question.close(handle)
    if current and current == handle then
        close()
    end
end

function question.onFrame()
    if current == nil or not current.dirty then
        return
    end
    current.dirty = false
    if element then
        element.layout = layout()
        element:update()
    else
        element = ui.create(layout())
    end
end

return question
