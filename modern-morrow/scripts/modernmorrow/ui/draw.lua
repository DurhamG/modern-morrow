-- Drawing primitives for screens that build their widgets from scratch each time they redraw.
-- A canvas collects widgets at absolute positions in layer units, and turns them into one layout.

local input = require('openmw.input')
local ui = require('openmw.ui')
local util = require('openmw.util')

local style = require('scripts.modernmorrow.ui.style')
local text = require('scripts.modernmorrow.ui.text')

local v2 = util.vector2

local draw = {}

local textures = {}

-- A texture resource, created once per path and region.
function draw.texture(path, offset, size)
    local key = path
    if offset then
        key = string.format('%s|%d,%d,%d,%d', path, offset.x, offset.y, size.x, size.y)
    end
    local texture = textures[key]
    if texture == nil then
        texture = ui.texture({ path = path, offset = offset, size = size })
        textures[key] = texture
    end
    return texture
end

local BUTTON = input.CONTROLLER_BUTTON
local AXIS = input.CONTROLLER_AXIS

local BUTTONS = {
    a = BUTTON.A,
    b = BUTTON.B,
    x = BUTTON.X,
    y = BUTTON.Y,
    lb = BUTTON.LeftShoulder,
    rb = BUTTON.RightShoulder,
    ls = BUTTON.LeftStick,
    rs = BUTTON.RightStick,
    dpad = BUTTON.DPadLeft,
    start = BUTTON.Start,
    back = BUTTON.Back,
}

local AXES = {
    lt = AXIS.TriggerLeft,
    rt = AXIS.TriggerRight,
    lstick = AXIS.LeftX,
    rstick = AXIS.RightX,
}

-- The texture of a controller button's icon, as the engine shows it for the controller in use. The names are the
-- router's button names (see input/router.lua), plus `dpad`, `lstick` and `rstick`.
function draw.glyphPath(button)
    local path
    if BUTTONS[button] then
        path = input.getControllerButtonIcon(BUTTONS[button])
    elseif AXES[button] then
        path = input.getControllerAxisIcon(AXES[button])
    end
    return path or input.getControllerButtonIcon(BUTTON.A)
end

local Canvas = {}
Canvas.__index = Canvas

function draw.canvas()
    return setmetatable({ widgets = {} }, Canvas)
end

function Canvas:add(layout)
    self.widgets[#self.widgets + 1] = layout
    return layout
end

function Canvas:rect(x, y, w, h, color, alpha)
    return self:add({
        type = ui.TYPE.Image,
        props = {
            resource = draw.texture('white'),
            color = color,
            alpha = alpha or 1,
            position = v2(x, y),
            size = v2(w, h),
        },
    })
end

-- Text in a box. It is centred vertically and starts at the left unless `alignH` says otherwise. `caption` must
-- already be escaped (see text.escape).
function Canvas:text(x, y, w, h, caption, size, color, alignH, options)
    local props = {
        text = caption,
        textSize = size,
        textColor = color,
        textShadow = true,
        textShadowColor = style.color.black,
        autoSize = false,
        position = v2(x, y),
        size = v2(w, h),
        textAlignH = alignH or ui.ALIGNMENT.Start,
        textAlignV = ui.ALIGNMENT.Center,
    }
    if options then
        for key, value in pairs(options) do
            props[key] = value
        end
    end
    return self:add({ type = ui.TYPE.Text, props = props })
end

function Canvas:image(x, y, w, h, resource, color, alpha)
    return self:add({
        type = ui.TYPE.Image,
        props = {
            resource = resource,
            color = color,
            alpha = alpha or 1,
            position = v2(x, y),
            size = v2(w, h),
        },
    })
end

-- A controller button's icon.
function Canvas:glyph(x, y, size, button)
    return self:image(x, y, size, size, draw.texture(draw.glyphPath(button)), style.color.glyph, 0.85)
end

-- An item's icon on a dark square, with the backgrounds the game uses for magic and equipped items.
function Canvas:icon(x, y, size, item, equipped)
    self:rect(x, y, size, size, style.color.iconBack, 0.9)
    local record = item.type.record(item)
    local magic = record.enchant ~= nil and record.enchant ~= ''
    local frame
    if magic then
        frame = equipped and 'textures/menu_icon_magic_equip.dds' or 'textures/menu_icon_magic.dds'
    elseif equipped then
        frame = 'textures/menu_icon_equip.dds'
    end
    if frame then
        -- The backgrounds use the top left 44 by 44 pixels of a 64 by 64 texture, as the game's item widget does.
        self:image(x, y, size, size, draw.texture(frame, v2(0, 0), v2(44, 44)))
    end
    if record.icon and record.icon ~= '' then
        local inset = size * 5 / 42
        local iconSize = size * 32 / 42
        local resource = draw.texture(record.icon)
        self:image(x + size * 9 / 42, y + size * 9 / 42, iconSize, iconSize, resource, style.color.black, 0.5)
        self:image(x + inset, y + inset, iconSize, iconSize, resource)
    end
end

-- An image, such as a magic effect's icon, on a dark square.
function Canvas:picture(x, y, size, path)
    self:rect(x, y, size, size, style.color.iconBack, 0.9)
    self:image(x + 2, y + 2, size - 4, size - 4, draw.texture(path))
end

-- A bar that is filled from the left by a fraction from 0 to 1, such as the progress to the next level.
function Canvas:bar(x, y, w, h, fraction, color)
    self:rect(x, y, w, h, style.color.black, 0.6)
    local filled = math.floor(w * math.max(0, math.min(1, fraction)) + 0.5)
    if filled > 0 then
        self:rect(x, y, filled, h, color, 0.9)
    end
end

-- Plain text wrapped to the width, one line under another, stopping before `bottom` if it is given. Returns the y
-- below the last line.
function Canvas:paragraph(x, y, w, s, size, color, bottom)
    local lineHeight = size + 6
    for _, line in ipairs(text.wrap(s, w, size, 100)) do
        if bottom and y + lineHeight > bottom then
            break
        end
        self:text(x, y, w, lineHeight, line, size, color)
        y = y + lineHeight
    end
    return y
end

-- The layout of everything drawn so far, inside a widget with the given props.
function Canvas:layout(props)
    return { type = ui.TYPE.Widget, props = props, content = ui.content(self.widgets) }
end

return draw
