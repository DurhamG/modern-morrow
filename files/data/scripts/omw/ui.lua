local ui = require('openmw.ui')
local util = require('openmw.util')
local self = require('openmw.self')
local core = require('openmw.core')
local ambient = require('openmw.ambient')
local input = require('openmw.input')

local MODE = ui._getAllUiModes()
local WINDOW = ui._getAllWindowIds()

local replacedWindows = {}
local hiddenWindows = {}
local modeStack = {}

local inputCapturedByInterface = false

local interactiveMessageHandler = nil
local waitingMessageId = nil

local modePause = {}
for _, mode in pairs(MODE) do
    modePause[mode] = true
end

local function registerWindow(window, showFn, hideFn, options)
    if not WINDOW[window] then
        error('At the moment it is only possible to override existing windows. Window "'..
              tostring(window)..'" not found.')
    end
    ui._setWindowDisabled(window, true)
    if replacedWindows[window] then
        replacedWindows[window].hideFn()
    end
    replacedWindows[window] = {
        showFn = showFn,
        hideFn = hideFn,
        visible = false,
        capturesInput = options ~= nil and options.capturesInput == true,
    }
    hiddenWindows[window] = nil
end

local function updateInputCapture()
    local shouldCaptureInput = waitingMessageId ~= nil
    for _, state in pairs(replacedWindows) do
        shouldCaptureInput = shouldCaptureInput or (state.visible and state.capturesInput)
    end
    if shouldCaptureInput ~= inputCapturedByInterface then
        input.setGuiInputCaptured(shouldCaptureInput)
        inputCapturedByInterface = shouldCaptureInput
    end
end

local function registerInteractiveMessageHandler(handler)
    interactiveMessageHandler = handler
    ui._setLuaAnswersInteractiveMessages(handler ~= nil)
    if handler == nil and waitingMessageId ~= nil then
        -- The engine shows the waiting message in its own box now.
        waitingMessageId = nil
        updateInputCapture()
    end
end

local function onInteractiveMessage(id, message, buttons)
    if interactiveMessageHandler == nil then
        return
    end
    waitingMessageId = id
    updateInputCapture()
    local function answer(button)
        if waitingMessageId ~= id then
            return
        end
        if type(button) ~= 'number' or button % 1 ~= 0 or button < 1 or button > #buttons then
            error('Invalid button index: ' .. tostring(button))
        end
        waitingMessageId = nil
        ui._answerInteractiveMessage(id, button - 1)
        updateInputCapture()
    end
    interactiveMessageHandler(message, buttons, answer)
end

local function updateHidden(mode, options)
    local toHide = {}
    if options and options.windows then
        for _, w in pairs(ui._getAllowedWindows(mode)) do
            toHide[w] = true
        end
        for _, w in pairs(options.windows) do
            toHide[w] = nil
        end
    end
    for w, _ in pairs(hiddenWindows) do
        if toHide[w] then
            toHide[w] = nil
        else
            hiddenWindows[w] = nil
            if not replacedWindows[w] then
                ui._setWindowDisabled(w, false)
            end
        end
    end
    for w, _ in pairs(toHide) do
        hiddenWindows[w] = true
        if not replacedWindows[w] then
            ui._setWindowDisabled(w, true)
        end
    end
end

local function setMode(mode, options)
    local function impl()
        updateHidden(mode, options)
        ui._setUiModeStack({mode}, options and options.target)
    end
    if mode then
        if not pcall(impl) then
            error('Invalid mode: ' .. tostring(mode))
        end
    else
        ui._setUiModeStack({})
    end
end

local function addMode(mode, options)
    local function impl()
        updateHidden(mode, options)
        ui._setUiModeStack(modeStack, options and options.target)
    end
    modeStack[#modeStack + 1] = mode
    if not pcall(impl) then
        modeStack[#modeStack] = nil
        error('Invalid mode: ' .. tostring(mode))
    end
end

local function removeMode(mode)
    local sizeBefore = #modeStack
    local j = 1
    for i = 1, sizeBefore do
        if modeStack[i] ~= mode then
            modeStack[j] = modeStack[i]
            j = j + 1
        end
    end
    for i = j, sizeBefore do modeStack[i] = nil end
    if sizeBefore > #modeStack then
        ui._setUiModeStack(modeStack)
    end
end

local oldMode = nil
local function onUiModeChanged(changedByLua, arg)
    local newStack = ui._getUiModeStack()
    for i = 1, math.max(#modeStack, #newStack) do
        modeStack[i] = newStack[i]
    end
    for w, state in pairs(replacedWindows) do
        if state.visible then
            state.hideFn()
            state.visible = false
        end
    end
    local mode = newStack[#newStack]
    if mode then
        if not changedByLua then
            updateHidden(mode)
        end
        for _, w in pairs(ui._getAllowedWindows(mode)) do
            local state = replacedWindows[w]
            if state and not hiddenWindows[w] then
                state.showFn(arg)
                state.visible = true
            end
        end
    end
    updateInputCapture()
    local shouldPause = false
    for _, m in pairs(modeStack) do
        shouldPause = shouldPause or modePause[m]
    end
    if shouldPause then
        core.sendGlobalEvent('Pause', 'ui')
    else
        core.sendGlobalEvent('Unpause', 'ui')
    end
    self:sendEvent('UiModeChanged', {oldMode = oldMode, newMode = mode, arg = arg})
    oldMode = mode
end

local function onUiModeChangedEvent(data)
    if data.oldMode == data.newMode then
        return
    end
    -- Sounds are processed in the event handler rather than in engine handler
    -- in order to allow them to be overridden in mods.
    if data.newMode == MODE.Journal or data.newMode == MODE.Book then
        ambient.playSound('book open', {scale = false})
    elseif data.oldMode == MODE.Journal or data.oldMode == MODE.Book then
        if not ambient.isSoundPlaying('item book up') then
            ambient.playSound('book close', {scale = false})
        end
    elseif data.newMode == MODE.Scroll or data.oldMode == MODE.Scroll then
        if not ambient.isSoundPlaying('item book up') then
            ambient.playSound('scroll', {scale = false})
        end
    end
end

local function isWindowVisible(windowName)
    if replacedWindows[windowName] then
        return replacedWindows[windowName].visible
    end
    return ui._isWindowVisible(windowName)
end

return {
    interfaceName = 'UI',
    ---
    -- @module UI
    -- @context player
    -- @usage require('openmw.interfaces').UI
    interface = {
        --- Interface version
        -- @field [parent=#UI] #number version
        version = 5,

        --- All available UI modes.
        -- Use `view(I.UI.MODE)` in `luap` console mode to see the list.
        -- @field [parent=#UI] #table MODE
        MODE = util.makeStrictReadOnly(MODE),

        --- All windows.
        -- Use `view(I.UI.WINDOW)` in `luap` console mode to see the list.
        -- @field [parent=#UI] #table WINDOW
        WINDOW = util.makeStrictReadOnly(WINDOW),

        --- Register new implementation for the window with given name; overrides previous implementation.
        -- Adding new windows is not supported yet. At the moment it is only possible to override built-in windows.
        -- @function [parent=#UI] registerWindow
        -- @param #string windowName
        -- @param #function showFn Callback that will be called when the window should become visible
        -- @param #function hideFn Callback that will be called when the window should be hidden
        -- @param #table options (optional) A table with these optional fields:
        --
        --   * `capturesInput` - if true, the engine leaves the controller and the keyboard to Lua while this window is shown.
        --     It sets @{openmw_input#(input).setGuiInputCaptured} when the window is shown, and clears it when no shown window captures the input.
        registerWindow = registerWindow,

        --- Returns windows that can be shown in given mode.
        -- @function [parent=#UI] getWindowsForMode
        -- @param #string mode
        -- @return #table
        getWindowsForMode = ui._getAllowedWindows,

        --- Stack of currently active modes
        -- @field [parent=#UI] modes
        modes = util.makeReadOnly(modeStack),

        --- Get current mode (nil if all windows are closed), equivalent to `I.UI.modes[#I.UI.modes]`
        -- @function [parent=#UI] getMode
        -- @return #string
        getMode = function() return modeStack[#modeStack] end,

        --- Drop all active modes and set mode.
        -- @function [parent=#UI] setMode
        -- @param #string mode (optional) New mode
        -- @param #table options (optional) Table with keys 'windows' and/or 'target' (see example).
        -- @usage I.UI.setMode() -- drop all modes
        -- @usage I.UI.setMode('Interface') -- drop all modes and open interface
        -- @usage -- Drop all modes, open interface, but show only the map window.
        -- I.UI.setMode('Interface', {windows = {'Map'}})
        setMode = setMode,

        --- Add mode to stack without dropping other active modes.
        -- @function [parent=#UI] addMode
        -- @param #string mode New mode
        -- @param #table options (optional) Table with keys 'windows' and/or 'target' (see example).
        -- @usage I.UI.addMode('Journal') -- open journal without dropping active modes.
        -- @usage -- Open barter with an NPC
        -- I.UI.addMode('Barter', {target = actor})
        addMode = addMode,

        --- Remove the specified mode from active modes.
        -- @function [parent=#UI] removeMode
        -- @param #string mode Mode to drop
        removeMode = removeMode,

        --- Set whether the mode should pause the game.
        -- @function [parent=#UI] setPauseOnMode
        -- @param #string mode Mode to configure
        -- @param #boolean shouldPause
        setPauseOnMode = function(mode, shouldPause) modePause[mode] = shouldPause end,

        --- Set whether the UI should be visible.
        -- @function [parent=#UI] setHudVisibility
        -- @param #boolean showHud
        setHudVisibility = function(showHud) ui._setHudVisibility(showHud) end,

        ---
        -- Returns if the player HUD is visible or not
        -- @function [parent=#UI] isHudVisible
        -- @return #boolean
        isHudVisible = function() return ui._isHudVisible() end,

        ---
        -- Returns if the given window is visible or not
        -- @function [parent=#UI] isWindowVisible
        -- @param #string windowName
        -- @return #boolean
        isWindowVisible = isWindowVisible,

        ---
        -- Shows a message as an interactive message box pausing the game, with a single button with the localized text OK.
        -- @function [parent=#UI] showInteractiveMessage
        -- @param #string message Message to display
        -- @param #table options Options (none yet)
        showInteractiveMessage = ui._showInteractiveMessage,

        ---
        -- Show and answer the interactive message boxes, the messages with buttons that the player must answer,
        -- instead of the engine. Use it to draw these messages in the style of a mod's own menus.
        --
        -- These messages come from game scripts (`MessageBox` with buttons), the soul gem and companion questions,
        -- the question after the player's death, and `showInteractiveMessage`. A few messages stop the whole game
        -- until they are answered, and Lua doesn't run then. The engine still shows those, for example the warnings
        -- of the settings menu and the warning about missing content files when a save is loaded.
        --
        -- While a message waits for its answer, the game is paused and can't be saved, as it is while the engine's box
        -- is shown. Until the answer, the interface also captures the menu input with
        -- @{openmw_input#(input).setGuiInputCaptured}, so that the keyboard and the controller only reach Lua.
        --
        -- If a new message arrives before the handler answered the last one, the new message replaces it. The handler
        -- is called again, and should show the new message instead of the old one. The old `answer` function then
        -- does nothing. Registering a new handler replaces the previous one.
        -- @function [parent=#UI] registerInteractiveMessageHandler
        -- @param #function handler A function `handler(message, buttons, answer)`, or nil to let the engine show the
        -- messages again. `message` is the text of the message, and `buttons` is a list of button labels. Call
        -- `answer(index)` with the 1-based index of the button the player chose.
        -- @usage I.UI.registerInteractiveMessageHandler(function(message, buttons, answer)
        --     -- Show the message, and call answer(i) when the player picks buttons[i].
        -- end)
        registerInteractiveMessageHandler = registerInteractiveMessageHandler,

        -- TODO
        -- registerHudElement = function(name, showFn, hideFn) end,
        -- showHudElement = function(name, bool) end,
        -- hudElements,  -- map from element name to its visibility
    },
    engineHandlers = {
        _onUiModeChanged = onUiModeChanged,
        _onInteractiveMessage = onInteractiveMessage,
    },
    eventHandlers = {
        UiModeChanged = onUiModeChangedEvent,
        AddUiMode = function(options) addMode(options.mode, options) end,
        SetUiMode = function(options) setMode(options.mode, options) end,
    },
}
