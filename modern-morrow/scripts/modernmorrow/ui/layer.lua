-- The layers the mod draws on. All are 720 units tall at any resolution, and as wide as the screen's aspect ratio
-- allows, so the screens keep their size relative to the screen height, as on a TV or a handheld.

local ui = require('openmw.ui')
local util = require('openmw.util')

local layer = {
    -- The HUD, which shows during play, above the engine's HUD.
    hud = 'ModernMorrowHud',
    -- Full screens, such as the inventory.
    menu = 'ModernMorrowMenu',
    -- Dialogs over a screen, such as the count picker.
    modal = 'ModernMorrowModal',
    -- The short messages, such as "Your journal has been updated", over everything else.
    messages = 'ModernMorrowMessages',
    height = 720,
}

-- Creates the layers. They stay when a game is loaded, so only the first game of a session creates them.
function layer.setup()
    if not ui.layers.indexOf(layer.hud) then
        ui.layers.insertAfter('HUD', layer.hud, { interactive = false, height = layer.height })
    end
    if not ui.layers.indexOf(layer.menu) then
        ui.layers.insertAfter('Windows', layer.menu, { interactive = true, height = layer.height })
    end
    if not ui.layers.indexOf(layer.modal) then
        ui.layers.insertAfter(layer.menu, layer.modal, { interactive = true, height = layer.height })
    end
    if not ui.layers.indexOf(layer.messages) then
        ui.layers.insertAfter(layer.modal, layer.messages, { interactive = false, height = layer.height })
    end
end

-- The size of a layer in its own units. Before the layer has a size, this guesses from a 16:10 screen.
function layer.size(name)
    local index = ui.layers.indexOf(name or layer.menu)
    if index then
        local size = ui.layers[index].size
        if size and size.x > 0 and size.y > 0 then
            return size
        end
    end
    return util.vector2(layer.height * 16 / 10, layer.height)
end

return layer
