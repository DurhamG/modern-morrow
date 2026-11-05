local core = require('openmw.core')
local ui = require('openmw.ui')
local util = require('openmw.util')

local function gmstColor(name, fallback)
    local value = core.getGMST(name)
    if value then
        return util.color.commaString(value)
    end
    return fallback
end

local style = {
    menuLayer = 'UiProofMenu',
    hudLayer = 'UiProofHud',
    layerHeight = 720,

    white = ui.texture { path = 'white' },

    normal = gmstColor('FontColor_color_normal', util.color.rgb(0.79, 0.65, 0.38)),
    over = gmstColor('FontColor_color_normal_over', util.color.rgb(0.87, 0.79, 0.62)),
    header = gmstColor('FontColor_color_header', util.color.rgb(0.87, 0.79, 0.62)),
    link = gmstColor('FontColor_color_link', util.color.rgb(0.44, 0.49, 0.81)),
    disabled = gmstColor('FontColor_color_disabled', util.color.rgb(0.70, 0.66, 0.53)),
    background = util.color.rgb(0.04, 0.035, 0.03),
    frame = util.color.rgb(0.45, 0.36, 0.20),
    selection = util.color.rgb(0.42, 0.32, 0.12),
    dim = util.color.rgb(0.55, 0.50, 0.42),

    textSize = 20,
    smallTextSize = 17,
    titleSize = 26,
}

function style.layerSize(name)
    local index = ui.layers.indexOf(name)
    if index then
        local size = ui.layers[index].size
        if size and size.x > 0 then
            return size
        end
    end
    return util.vector2(style.layerHeight * 16 / 10, style.layerHeight)
end

function style.text(text, props)
    local merged = {
        text = text,
        textSize = style.textSize,
        textColor = style.normal,
    }
    for k, v in pairs(props or {}) do
        merged[k] = v
    end
    return { type = ui.TYPE.Text, props = merged }
end

function style.fill(color, alpha, props)
    local merged = {
        resource = style.white,
        color = color,
        alpha = alpha or 1,
        relativeSize = util.vector2(1, 1),
    }
    for k, v in pairs(props or {}) do
        merged[k] = v
    end
    return { type = ui.TYPE.Image, props = merged }
end

-- A dark panel with a thin frame. `props` places the panel, `content` is a list of layouts inside it.
function style.panel(props, content)
    local children = {
        style.fill(style.frame, 1),
        style.fill(style.background, 0.94, {
            position = util.vector2(2, 2),
            size = util.vector2(-4, -4),
        }),
    }
    for _, child in ipairs(content) do
        children[#children + 1] = child
    end
    return { type = ui.TYPE.Widget, props = props, content = ui.content(children) }
end

-- Text for a MyGUI caption: a `#` would start a colour code.
function style.escape(text)
    return (text:gsub('#', '##'))
end

function style.colorCode(color)
    return '#' .. color:asHex()
end

return style
