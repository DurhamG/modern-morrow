-- Reading a book or a scroll. The text is one tall column on a page in the middle of the screen, which the sticks and
-- the d-pad scroll, in place of the engine's pairs of pages. A shows the book's Take when the book isn't the player's.
-- It replaces the engine's Book and Scroll windows, and follows the fork's reading panel (ui-rework/design/
-- other-screens.md in D:\Code\openmw).
--
-- The engine raises a skill book's skill when the player starts reading it, before any window opens, so that still
-- happens with the mod.

local core = require('openmw.core')
local input = require('openmw.input')
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
local scroll = require('scripts.modernmorrow.ui.scroll')
local books = require('scripts.modernmorrow.items.books')
local router = require('scripts.modernmorrow.input.router')

local l10n = core.l10n('ModernMorrow')
local v2 = util.vector2
local BUTTON = input.CONTROLLER_BUTTON
local AXIS = input.CONTROLLER_AXIS

local reading = {}

local color = style.color

local PAGE_WIDTH = 640
local PADDING = 32
-- How far the d-pad pushes the text, as a stick position.
local DPAD_PUSH = 0.8

-- The book on show, the mode that shows it, and the widgets.
local current = nil
local frame = nil
local paper = nil
local mover = nil
local thumb = nil

local function destroy()
    for _, element in ipairs({ frame, mover, paper, thumb }) do
        element:destroy()
    end
    frame, mover, paper, thumb = nil, nil, nil, nil
end

-- Whether the player can take the book. A book in the player's inventory is already theirs. Taking one from a
-- container or from the world follows the crime rules, so taking an owned book is theft, as in the engine's window.
local function canTake(book)
    local container = book.parentContainer
    if container ~= nil then
        return container ~= self.object
    end
    return book.cell ~= nil
end

local function thumbLayout()
    local canvas = draw.canvas()
    local c = current
    scroll.drawBar(canvas, 0, 0, c.trackH, c.scroll, c.textH, c.areaH, color.black, 0.12, 0.45)
    return canvas:layout({ position = v2(c.trackX, c.trackY), size = v2(4, c.trackH) })
end

local function place()
    mover.layout.props.position = v2(0, -math.floor(current.scroll))
    mover:update()
    thumb.layout = thumbLayout()
    thumb:update()
end

local function build()
    local size = layer.size(layer.menu)
    local canvas = draw.canvas()
    canvas:rect(0, 0, size.x, size.y, color.black, 0.6)
    local w = math.min(PAGE_WIDTH, size.x - 2 * style.space.margin)
    local x = math.floor((size.x - w) / 2)
    local top = 24
    local bottom = size.y - 24

    -- The title above the page, and the button hints below it.
    canvas:rect(x, top, w, bottom - top, color.modal)
    canvas:rect(x, top, w, 1, color.accent, 0.6)
    canvas:text(x + 20, top + 8, w - 40, 34, text.fit(current.title, w - 40, style.textSize.large), style.textSize.large,
        color.bright)
    local pageY = top + 50
    local pageH = bottom - 52 - pageY
    canvas:rect(x + 12, pageY, w - 24, pageH, color.page)

    -- The text moves up inside an area of the page's size, which clips what is outside it.
    local areaX = x + 12 + PADDING
    local areaY = pageY + PADDING / 2
    local areaW = w - 24 - 2 * PADDING
    local areaH = pageH - PADDING
    local content, textH = books.layout(current.raw, areaW, { isBook = current.isBook })
    current.textH = textH
    current.areaH = areaH
    current.max = math.max(0, textH - areaH)
    current.trackX = x + w - 12 - PADDING / 2 - 2
    current.trackY = areaY
    current.trackH = areaH
    paper = ui.create({ type = ui.TYPE.Widget, props = { size = v2(areaW, math.max(textH, 1)) }, content = content })
    mover = ui.create({
        type = ui.TYPE.Widget,
        props = { position = v2(0, 0), size = v2(areaW, math.max(textH, 1)) },
        content = ui.content({ paper }),
    })
    canvas:add({
        type = ui.TYPE.Widget,
        props = { position = v2(areaX, areaY), size = v2(areaW, areaH) },
        content = ui.content({ mover }),
    })
    thumb = ui.create(thumbLayout())
    canvas:add(thumb)

    local hintList = {}
    if current.canTake then
        hintList[#hintList + 1] = { 'a', text.escape(l10n('Take')) }
    end
    if current.max > 0 then
        hintList[#hintList + 1] = { 'rstick', text.escape(l10n('Scroll')) }
    end
    hints.draw(canvas, x + 20, w - 40, bottom, hintList)

    local root = canvas:layout({ size = size })
    root.layer = layer.menu
    frame = ui.create(root)
end

local function close()
    if current then
        I.UI.removeMode(current.mode)
    end
end

local function take()
    if not current.canTake then
        return
    end
    ambient.playSound(types.Item.getUpSound(current.book))
    core.sendGlobalEvent('ModernMorrowTakeBook', { player = self.object, book = current.book })
    close()
end

reading.handler = {
    onButton = function(name)
        if name == 'a' then
            take()
        elseif name == 'b' then
            close()
        end
    end,
    -- Either stick scrolls the text, and so does the d-pad, while held. They are read every frame.
    onStick = function(_, rightY, dt)
        if current == nil or current.max <= 0 then
            return
        end
        local stick = rightY
        local leftY = input.getAxisValue(AXIS.LeftY)
        if math.abs(leftY) > math.abs(stick) then
            stick = leftY
        end
        if input.isControllerButtonPressed(BUTTON.DPadUp) then
            stick = -DPAD_PUSH
        elseif input.isControllerButtonPressed(BUTTON.DPadDown) then
            stick = DPAD_PUSH
        end
        local before = current.scroll
        current.scroll = scroll.step(before, current.max, stick, dt)
        if math.floor(current.scroll) ~= math.floor(before) then
            place()
        end
    end,
}

local function show(mode, book)
    if current then
        reading.hide()
    end
    if book == nil or not book:isValid() then
        return
    end
    local record = book.type.record(book)
    current = {
        mode = mode,
        book = book,
        title = record.name or '',
        raw = record.text or '',
        isBook = types.Book.objectIsInstance(book),
        canTake = canTake(book),
        scroll = 0,
    }
    build()
    router.push(reading.handler)
end

function reading.hide()
    router.remove(reading.handler)
    if frame then
        destroy()
    end
    current = nil
end

function reading.register()
    for _, window in ipairs({ 'Book', 'Scroll' }) do
        I.UI.registerWindow(window, function(book)
            show(window, book)
        end, reading.hide, { capturesInput = true })
    end
end

return reading
