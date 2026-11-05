-- Serving time in jail. It replaces the engine's JailScreen with the fork's progress panel: a label and a bar in the
-- middle of the black screen, while the days pass.
--
-- The engine still runs the jail term behind the replaced window: it fades the screen out, takes the player to the
-- prison marker, passes the days, lowers the skills and closes the mode. The bar follows the JailProgress events that
-- the engine sends as the term passes.

local core = require('openmw.core')
local ui = require('openmw.ui')
local I = require('openmw.interfaces')

local layer = require('scripts.modernmorrow.ui.layer')
local draw = require('scripts.modernmorrow.ui.draw')
local text = require('scripts.modernmorrow.ui.text')
local screen = require('scripts.modernmorrow.ui.screen')

local jail = {}

local element = nil
-- How much of the term has passed, from 0 to 1, and whether the bar needs drawing again.
local progress = 0
local dirty = false

local function layout()
    local canvas = draw.canvas()
    screen.drawProgress(canvas, text.escape(core.getGMST('sInPrisonTitle') or 'Serving time...'), progress)
    local root = canvas:layout({ size = layer.size(layer.menu) })
    root.layer = layer.menu
    return root
end

local function show()
    progress = 0
    dirty = false
    if element then
        element:destroy()
    end
    element = ui.create(layout())
end

local function hide()
    if element then
        element:destroy()
        element = nil
    end
end

-- From the JailProgress event.
function jail.onProgress(data)
    progress = data.progress
    -- Event handlers may run on the Lua worker thread, which can't measure text, so the next frame redraws.
    dirty = true
end

function jail.onFrame()
    if element and dirty then
        dirty = false
        element.layout = layout()
        element:update()
    end
end

function jail.register()
    -- The jail takes no input, so the window doesn't capture it.
    I.UI.registerWindow('JailScreen', show, hide)
end

return jail
