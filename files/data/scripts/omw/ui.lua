local ui = require('openmw.ui')
local async = require('openmw.async')
local util = require('openmw.util')
local self = require('openmw.self')
local core = require('openmw.core')
local ambient = require('openmw.ambient')
local input = require('openmw.input')

local MODE = ui._getAllUiModes()
local WINDOW = ui._getAllWindowIds()
local HUD_ELEMENT = ui._getAllHudElements()

local replacedWindows = {}
local hiddenWindows = {}
-- The windows that the last I.UI.setMode or I.UI.addMode asked for, or nil if it didn't ask for any
local requestedWindows = nil
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

local function isMainMenuOpen()
    for _, m in pairs(modeStack) do
        if m == MODE.MainMenu then
            return true
        end
    end
    return false
end

local function updateInputCapture()
    local shouldCaptureInput = waitingMessageId ~= nil
    for _, state in pairs(replacedWindows) do
        shouldCaptureInput = shouldCaptureInput or (state.visible and state.capturesInput)
    end
    -- Player scripts get no input events while the main menu is open, so no script could use the captured input.
    shouldCaptureInput = shouldCaptureInput and not isMainMenuOpen()
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

local function showInteractiveMessage(message, options)
    local engineOptions = {}
    if options then
        engineOptions.buttons = options.buttons
        if options.callback then
            engineOptions.callback = async:callback(options.callback)
        end
    end
    ui._showInteractiveMessage(message, engineOptions)
end

local function updateHidden(mode, options)
    requestedWindows = nil
    if options and options.windows then
        requestedWindows = {}
        for _, w in pairs(options.windows) do
            requestedWindows[#requestedWindows + 1] = w
        end
    end
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
                state.showFn(arg, requestedWindows)
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

local function setHudElementVisibility(element, visible)
    if not HUD_ELEMENT[element] then
        error('HUD element "' .. tostring(element) .. '" not found.')
    end
    ui._setHudElementVisibility(element, visible)
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
        version = 7,

        --- All available UI modes.
        -- Use `view(I.UI.MODE)` in `luap` console mode to see the list.
        -- @field [parent=#UI] #table MODE
        MODE = util.makeStrictReadOnly(MODE),

        --- All windows.
        -- Use `view(I.UI.WINDOW)` in `luap` console mode to see the list.
        -- @field [parent=#UI] #table WINDOW
        WINDOW = util.makeStrictReadOnly(WINDOW),

        --- All HUD elements that @{#UI.setHudElementVisibility} can hide.
        --
        --   * `DynamicStats` - the health, magicka and fatigue bars
        --   * `EnemyHealth` - the health bar of the last actor the player hit
        --   * `Weapon` - the icon and condition bar of the equipped weapon
        --   * `Spell` - the icon of the selected spell or enchanted item, and its bar
        --   * `WeaponSpellName` - the name that appears for a few seconds when the weapon or the spell changes
        --   * `Sneak` - the sneak indicator
        --   * `ActiveEffects` - the icons of the spell effects on the player
        --   * `Minimap` - the local map with the compass
        --   * `CellName` - the cell name that appears for a few seconds when the player enters a cell
        --   * `DrowningBar` - the breath meter that appears under water
        -- @field [parent=#UI] #table HUD_ELEMENT
        HUD_ELEMENT = util.makeStrictReadOnly(HUD_ELEMENT),

        --- Register new implementation for the window with given name; overrides previous implementation.
        -- Adding new windows is not supported yet. At the moment it is only possible to override built-in windows.
        -- A replacement of the main menu (`MainMenu`) is shown when the player opens the menu during a game. Before a game
        -- is started or loaded, there are no player scripts, so the engine shows its own main menu. While the main menu
        -- is open, player scripts get no input events such as `onKeyPress` or `onControllerButtonPress`, so the
        -- replacement has to rely on the events of its own UI elements, such as `mouseClick`.
        --
        -- Each step of character creation has a window with the name of its mode: `ChargenName`, `ChargenRace`,
        -- `ChargenClass`, `ChargenClassGenerate`, `ChargenClassPick`, `ChargenClassCreate` and `ChargenBirth`. The
        -- window of the `ChargenClassReview` mode is `ChargenReview`. A replacement of `ChargenClassGenerate` replaces
        -- both the class questions and the window that shows the resulting class. A replacement closes its mode itself
        -- when the player is done with the step. Escape doesn't close it, just as it doesn't close the engine's window.
        -- The engine's review window sends the player back to a step and expects the step to return to the review. A
        -- replacement of a step that opened from the review, which the `oldMode` of the `UiModeChanged` event shows,
        -- should open `ChargenClassReview` again when the player is done.
        -- @function [parent=#UI] registerWindow
        -- @param #string windowName
        -- @param #function showFn Callback that will be called when the window should become visible.
        -- Its first argument is the mode's target, if any. Its second argument is the list of windows that
        -- @{#UI.setMode} or @{#UI.addMode} asked for with the `windows` option, or nil when the mode opened without such
        -- a list, such as when the player opened it. A script that replaces several windows of a mode can use it to
        -- tell a request for one of them from the player opening the whole mode.
        -- @param #function hideFn Callback that will be called when the window should be hidden
        -- @param #table options (optional) A table with these optional fields:
        --
        --   * `capturesInput` - if true, the engine leaves the controller and the keyboard to Lua while this window is shown.
        --     It sets @{openmw_input#(input).setGuiInputCaptured} when the window is shown, and clears it when no shown window captures the input.
        --     The main menu is an exception. While it is open, the interface doesn't capture the input, because player scripts get no input events then.
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
        -- Hide one element of the HUD, or let the engine show it again. Use it to replace an element with a mod's own,
        -- for example to draw the health bars in another style. The other elements of the HUD stay as they are.
        --
        -- While an element is hidden this way, the engine keeps it hidden, even at times when it would show it. With
        -- `visible` true, the engine shows and hides the element by its own rules again. For example, it hides the
        -- minimap while the map window is pinned, and shows the sneak indicator while the player sneaks unseen. The
        -- elements beside a hidden one move into its place, as they do when the player pins a window.
        --
        -- All elements go back to the engine when a game is started or loaded.
        -- @function [parent=#UI] setHudElementVisibility
        -- @param #string element One of @{#UI.HUD_ELEMENT}
        -- @param #boolean visible False to hide the element, true to let the engine show it
        -- @usage I.UI.setHudElementVisibility(I.UI.HUD_ELEMENT.DynamicStats, false)
        setHudElementVisibility = setHudElementVisibility,

        ---
        -- Returns whether the given HUD element is on the screen now.
        -- @function [parent=#UI] isHudElementVisible
        -- @param #string element One of @{#UI.HUD_ELEMENT}
        -- @return #boolean
        isHudElementVisible = function(element) return ui._isHudElementVisible(element) end,

        ---
        -- Returns if the given window is visible or not
        -- @function [parent=#UI] isWindowVisible
        -- @param #string windowName
        -- @return #boolean
        isWindowVisible = isWindowVisible,

        ---
        -- Shows a message as an interactive message box pausing the game, with buttons that the player must choose
        -- from.
        -- @function [parent=#UI] showInteractiveMessage
        -- @param #string message Message to display
        -- @param #table options (optional) A table with these optional fields:
        --
        --   * `buttons` - a list of button labels. Without it, the message has a single button with the localized
        --     text OK.
        --   * `callback` - a function that is called with the 1-based index of the button that the player chose.
        --     It isn't called if another interactive message replaces this one before the player answers.
        -- @usage I.UI.showInteractiveMessage('Rest here?', {
        --     buttons = { 'Yes', 'No' },
        --     callback = function(button) print('The player chose button ' .. button) end,
        -- })
        showInteractiveMessage = showInteractiveMessage,

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
