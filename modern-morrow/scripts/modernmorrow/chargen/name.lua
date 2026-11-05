-- The name step of character creation. It replaces the engine's ChargenName window with the on-screen keyboard (see
-- ui/keyboard.lua), as in the fork. The keyboard starts empty the first time, and from the player's name after that.

local core = require('openmw.core')
local self = require('openmw.self')
local types = require('openmw.types')
local I = require('openmw.interfaces')

local keyboard = require('scripts.modernmorrow.ui.keyboard')
local flow = require('scripts.modernmorrow.chargen.flow')

local name = {}

local MODE = 'ChargenName'

local function show()
    keyboard.open({
        label = core.getGMST('sName') or 'Name',
        text = flow.hasName() and types.NPC.record(self).name or '',
        emptyMessage = core.getGMST('sNotifyMessage37') or 'You need to type in your name.',
        onDone = function(typed)
            types.Player.setCharGenName(self, typed)
            flow.done(MODE)
        end,
    })
end

local function hide()
    keyboard.close()
end

function name.register()
    I.UI.registerWindow(MODE, show, hide, { capturesInput = true })
end

return name
