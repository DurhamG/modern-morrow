-- The on-screen keyboard that types a name or another short text with the controller, as in the fork's text input:
-- a panel in the middle of the screen, over a darkened view, with what the text is for, the text so far, and the
-- keys. The d-pad moves between the keys, A types the highlighted one, B deletes the last letter, X types a space, Y
-- swaps the case of the next letter, and Start finishes. Letters start in upper case at the start of a word.
--
-- The keyboard draws in onFrame, because it can open from an event or a window's show function.

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

local keyboard = {}

-- The keys, row by row. A last row holds the keys that aren't characters.
local ROWS = {
    { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M' },
    { 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z' },
    { "'", '-', '.', ',', '1', '2', '3', '4', '5', '6', '7', '8', '9' },
}
local SPACE, DELETE, DONE = 1, 2, 3
local SPECIAL_KEYS = 3
local KEY_SIZE = 46
local KEY_GAP = 6
local HEIGHT = 400
-- Names in the game are short, and a longer text wouldn't fit the field.
local MAX_LENGTH = 48

local element = nil
local current = nil

local function L(key)
    return text.escape(l10n(key))
end

local function rowLength(row)
    return ROWS[row] and #ROWS[row] or SPECIAL_KEYS
end

local function length(s)
    return utf8.len(s) or #s
end

-- The text without its last character.
local function withoutLast(s)
    local count = length(s)
    if count == 0 then
        return s
    end
    return s:sub(1, (utf8.offset(s, count) or #s) - 1)
end

local function layout()
    local size = layer.size(layer.modal)
    local canvas = draw.canvas()
    local color = style.color
    canvas:rect(0, 0, size.x, size.y, color.black, 0.6)

    local columns = #ROWS[1]
    local keysW = columns * KEY_SIZE + (columns - 1) * KEY_GAP
    local w = keysW + 64
    local x = math.floor((size.x - w) / 2)
    local y = math.floor((size.y - HEIGHT) / 2)
    canvas:rect(x, y, w, HEIGHT, color.modal)
    canvas:rect(x, y, w, 1, color.accent, 0.6)

    -- What the text is for, such as "Name", and the text so far with a cursor after it.
    canvas:text(x + 32, y + 16, keysW, 34, text.escape(current.label), style.textSize.title, color.bright)
    canvas:rect(x + 32, y + 58, keysW, 48, color.black, 0.6)
    local typed = text.fit(current.text .. '_', keysW - 24, style.textSize.title)
    canvas:text(x + 44, y + 58, keysW - 24, 48, typed, style.textSize.title, color.active)

    local ky = y + 126
    for row = 1, #ROWS + 1 do
        local special = ROWS[row] == nil
        local count = rowLength(row)
        local keyW = special and math.floor((keysW - (count - 1) * KEY_GAP) / count) or KEY_SIZE
        for column = 1, count do
            local kx = x + 32 + (column - 1) * (keyW + KEY_GAP)
            local focused = row == current.row and column == current.column
            canvas:rect(kx, ky, keyW, KEY_SIZE, color.panel, 0.9)
            if focused then
                canvas:rect(kx, ky, keyW, KEY_SIZE, color.accent, 0.35)
                canvas:rect(kx, ky + KEY_SIZE - 3, keyW, 3, color.accent)
            end
            local caption
            if not special then
                caption = text.escape(ROWS[row][column])
            elseif column == SPACE then
                caption = L('KeyboardSpace')
            elseif column == DELETE then
                caption = L('KeyboardDelete')
            else
                caption = L('KeyboardDone')
            end
            canvas:text(kx, ky, keyW, KEY_SIZE, caption, special and style.textSize.normal or style.textSize.large,
                focused and color.active or color.bright, ui.ALIGNMENT.Center)
        end
        ky = ky + KEY_SIZE + KEY_GAP
    end

    -- B deletes a letter here, so it gets a hint like the other keys.
    hints.draw(canvas, x + 32, keysW, y + HEIGHT - 4, {
        { 'a', L('KeyboardType') },
        { 'b', L('KeyboardDelete') },
        { 'x', L('KeyboardSpace') },
        { 'y', current.shift and L('KeyboardShiftOn') or L('KeyboardShift') },
        { 'start', L('KeyboardDone') },
    })

    local root = canvas:layout({ size = size })
    root.layer = layer.modal
    return root
end

local function typeText(s)
    if length(current.text) + length(s) > MAX_LENGTH then
        return
    end
    current.text = current.text .. s
    current.dirty = true
end

local function deleteLast()
    current.text = withoutLast(current.text)
    current.dirty = true
end

local function close()
    router.remove(keyboard.handler)
    if element then
        element:destroy()
        element = nil
    end
    current = nil
end

local function finish()
    if current.text:match('^%s*$') and not current.allowEmpty then
        if current.emptyMessage then
            ui.showMessage(current.emptyMessage)
            return
        end
        -- Without a message for it, an empty text leaves things as they were.
        local onCancel = current.onCancel
        close()
        if onCancel then
            onCancel()
        end
        return
    end
    local done = current
    close()
    done.onDone((done.text:gsub('^%s+', ''):gsub('%s+$', '')))
end

local function pressKey()
    local keys = ROWS[current.row]
    if keys then
        -- Letters start in upper case at the start of a word, and in lower case after it. Shift swaps the case of the
        -- next letter.
        local last = current.text:sub(-1)
        local upper = current.text == '' or last == ' ' or last == '-'
        if current.shift then
            upper = not upper
        end
        current.shift = false
        local key = keys[current.column]
        typeText(upper and key or key:lower())
    elseif current.column == SPACE then
        typeText(' ')
    elseif current.column == DELETE then
        deleteLast()
    else
        finish()
    end
end

keyboard.handler = {
    onButton = function(name)
        if current == nil then
            return
        end
        if name == 'a' then
            ambient.playSound('menu click')
            pressKey()
        elseif name == 'b' then
            deleteLast()
        elseif name == 'x' then
            typeText(' ')
        elseif name == 'y' then
            current.shift = not current.shift
            current.dirty = true
        elseif name == 'start' then
            finish()
        end
    end,
    onMove = function(dx, dy)
        if current == nil then
            return
        end
        -- The last row has fewer, wider keys, so moving into it or out of it keeps about the same place across.
        local rows = #ROWS + 1
        if dy ~= 0 then
            local row = (current.row - 1 + dy) % rows + 1
            local across = (current.column - 0.5) / rowLength(current.row)
            current.column = math.min(math.floor(across * rowLength(row)) + 1, rowLength(row))
            current.row = row
        end
        if dx ~= 0 then
            current.column = (current.column - 1 + dx) % rowLength(current.row) + 1
        end
        ambient.playSound('menu click')
        current.dirty = true
    end,
}

-- Opens the keyboard. `options` has `label`, what the text is for, such as "Name", and `text`, the text to start
-- from, as plain text, and `onDone(text)`, which gets the typed text without spaces at its ends. An empty text shows
-- `emptyMessage` and keeps the keyboard open when the message is given, is accepted when `allowEmpty` is set, and
-- otherwise closes the keyboard and calls `onCancel()`, if it is given.
function keyboard.open(options)
    current = {
        label = options.label or '',
        text = options.text or '',
        onDone = options.onDone,
        onCancel = options.onCancel,
        emptyMessage = options.emptyMessage,
        allowEmpty = options.allowEmpty,
        row = 1,
        column = 1,
        shift = false,
        dirty = true,
    }
    router.push(keyboard.handler)
end

function keyboard.isOpen()
    return current ~= nil
end

-- Closes the keyboard without calling anything.
function keyboard.close()
    if current then
        close()
    end
end

function keyboard.onFrame()
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

return keyboard
