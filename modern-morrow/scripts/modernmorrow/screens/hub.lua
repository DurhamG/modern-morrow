-- The menus the game opens with the inventory button: the Interface mode. The mod's screens replace the engine's
-- windows of that mode, except the map, which stays the engine's own. One screen shows at a time, below a bar with
-- their names, and the triggers switch between them. A screen can also replace the only window of another mode, such as
-- the journal. Switching to it changes the mode, and its own key, such as the journal key, opens the menus on it.
--
-- A screen is a table with:
--   window          the engine window it replaces, such as 'Inventory'
--   mode            the mode of that window, if it isn't the Interface mode, such as 'Journal'
--   title()         its name in the top bar, escaped
--   show(), hide()  called when the screen appears and goes away
--   handler         the router handler that gets the controller input while the screen is on top
--
-- The engine opens the Interface mode with all its windows. The menus then show the screen the player used last, as the
-- engine's controller menus do. When a script opens the mode with a list of windows, such as with
-- `I.UI.setMode('Interface', { windows = { 'Magic' } })`, the menus show the first screen in the list. hub.open, and the
-- ModernMorrowOpen event with the window's name, do that too.
--
-- Other modes, such as Barter, show the Inventory window beside their own. The hub only shows a screen in the mode the
-- screen belongs to.

local core = require('openmw.core')
local ui = require('openmw.ui')
local ambient = require('openmw.ambient')
local I = require('openmw.interfaces')

local style = require('scripts.modernmorrow.ui.style')
local text = require('scripts.modernmorrow.ui.text')
local router = require('scripts.modernmorrow.input.router')

local hub = {}

local MODE = 'Interface'
local TAB_WIDTH = 120

local screens = {}
local byWindow = {}
-- The screen on show, and the one the menus show next time.
local active = nil
local last = nil
-- The window that a switch to another mode asked for, until the engine opens the mode, and when it asked. While it
-- waits, the interface goes on showing the other windows of the old mode, and they don't ask again.
local pending = nil
local pendingSince = 0
-- How long a switch to another mode waits for the engine, in seconds of real time.
local PENDING_TIMEOUT = 1

local function modeOf(screen)
    return screen.mode or MODE
end

local function showScreen(screen)
    active = screen
    last = screen
    router.push(screen.handler)
    screen.show()
end

local function hideActive()
    if active == nil then
        return
    end
    local screen = active
    active = nil
    router.remove(screen.handler)
    screen.hide()
end

-- Hides every window of the mode but the screen's, including the engine's map. Setting the mode while it is open
-- only changes which windows are hidden, and the engine shows them all again the next time it opens the mode.
local function hideOtherWindows(screen)
    -- Setting the mode drops the modes below it, so only do it when the menus are all that is open. A screen in a mode
    -- of its own has no other windows to hide.
    if modeOf(screen) ~= MODE or I.UI.modes[1] ~= MODE or I.UI.modes[2] ~= nil then
        return
    end
    I.UI.setMode(MODE, { windows = { screen.window } })
end

-- Opens the mode of a screen with only its window, and shows the screen when the engine has opened it.
local function openMode(screen)
    pending = screen.window
    pendingSince = core.getRealTime()
    I.UI.setMode(modeOf(screen), { windows = { screen.window } })
end

-- The first of the hub's screens in a list of window names, or nil.
local function requestedScreen(requested)
    for _, window in ipairs(requested or {}) do
        if byWindow[window] then
            return byWindow[window]
        end
    end
    return nil
end

-- The interface calls this for each of the mod's windows that the mode shows, one after another, with the windows that
-- the script that opened the mode asked for. The first call picks the screen and hides the other windows, so the
-- interface doesn't call it for them.
local function onShown(shown, requested)
    if active or I.UI.getMode() ~= modeOf(shown) then
        return
    end
    local wanted
    if modeOf(shown) ~= MODE then
        -- The screen is the only window of its mode, such as the journal.
        wanted = shown
    else
        -- The interface goes through the windows in no fixed order, so the first screen registered is the default.
        wanted = requestedScreen(requested) or last or screens[1]
    end
    if modeOf(wanted) ~= modeOf(shown) then
        -- The screen asked for, or used last, is in another mode, such as the journal after the inventory button.
        -- The interface shows the mode's other windows next, and they wait for the mode to change.
        if pending ~= wanted.window or core.getRealTime() > pendingSince + PENDING_TIMEOUT then
            openMode(wanted)
        end
        return
    end
    pending = nil
    hideOtherWindows(wanted)
    showScreen(wanted)
end

function hub.register(screen)
    screens[#screens + 1] = screen
    byWindow[screen.window] = screen
    I.UI.registerWindow(screen.window, function(_, requested)
        onShown(screen, requested)
    end, hideActive, { capturesInput = true })
end

-- Shows another screen in place of the one on show.
function hub.switchTo(screen)
    if active == nil or screen == active then
        return
    end
    if modeOf(screen) ~= modeOf(active) then
        hideActive()
        openMode(screen)
        return
    end
    hideActive()
    hideOtherWindows(screen)
    showScreen(screen)
end

-- Opens the menus on the screen that replaces the given window, such as 'Magic'.
function hub.open(window)
    local screen = byWindow[window]
    if screen == nil then
        return
    end
    if active then
        hub.switchTo(screen)
        return
    end
    openMode(screen)
end

-- Closes the menus and returns to the game.
function hub.close()
    I.UI.removeMode(active and modeOf(active) or MODE)
end

-- Switches to the next or the previous screen. Does nothing while the mod has only one.
function hub.cycle(delta)
    if active == nil or #screens < 2 then
        return
    end
    local index = 1
    for i, screen in ipairs(screens) do
        if screen == active then
            index = i
        end
    end
    ambient.playSound('menu click')
    hub.switchTo(screens[(index - 1 + delta) % #screens + 1])
end

-- Forgets which screen was used last, such as when another game is loaded.
function hub.reset()
    last = nil
    pending = nil
end

-- Draws the bar at the top of the layer with the names of the screens, the current one underlined.
function hub.drawTopBar(canvas, width, current)
    local color = style.color
    local height = style.space.topBarHeight
    canvas:rect(0, 0, width, height, color.black)
    canvas:rect(0, height - 1, width, 1, color.accent, 0.35)
    local widths = {}
    local total = 0
    for i, screen in ipairs(screens) do
        widths[i] = math.max(TAB_WIDTH, text.width(screen.title(), style.textSize.large) + 40)
        total = total + widths[i]
    end
    local x = math.floor((width - total) / 2)
    if #screens > 1 then
        canvas:glyph(x - 34, 11, 22, 'lt')
    end
    for i, screen in ipairs(screens) do
        local selected = screen == current
        canvas:text(x, 0, widths[i], height, screen.title(), style.textSize.large,
            selected and color.active or color.dim, ui.ALIGNMENT.Center)
        if selected then
            canvas:rect(x + 20, 38, widths[i] - 40, 2, color.accent)
        end
        x = x + widths[i]
    end
    if #screens > 1 then
        canvas:glyph(x + 12, 11, 22, 'rt')
    end
end

return hub
