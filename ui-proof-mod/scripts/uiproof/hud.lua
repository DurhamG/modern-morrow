-- Hides the engine's health, magicka and fatigue bars and draws three bars of its own.

local self = require('openmw.self')
local types = require('openmw.types')
local ui = require('openmw.ui')
local util = require('openmw.util')
local I = require('openmw.interfaces')

local style = require('scripts.uiproof.style')

local hud = {}

local BAR_WIDTH = 260
local BAR_HEIGHT = 18
local GAP = 6

local BARS = {
    { stat = 'health', color = util.color.rgb(0.78, 0.16, 0.12) },
    { stat = 'magicka', color = util.color.rgb(0.20, 0.33, 0.85) },
    { stat = 'fatigue', color = util.color.rgb(0.18, 0.62, 0.22) },
}

local element = nil
local shown = {}
local hudVisible = nil

local function readStats()
    local values = {}
    for i, bar in ipairs(BARS) do
        local stat = types.Actor.stats.dynamic[bar.stat](self)
        values[i] = { current = math.floor(stat.current), base = math.floor(stat.base) }
    end
    return values
end

local function barLayout(bar, value, y)
    local ratio = 0
    if value.base > 0 then
        ratio = math.max(0, math.min(1, value.current / value.base))
    end
    return {
        type = ui.TYPE.Widget,
        props = { position = util.vector2(0, y), size = util.vector2(BAR_WIDTH, BAR_HEIGHT) },
        content = ui.content({
            style.fill(util.color.rgb(0, 0, 0), 0.7),
            style.fill(bar.color, 0.9, {
                position = util.vector2(2, 2),
                size = util.vector2(-4, -4),
                relativeSize = util.vector2(ratio, 1),
            }),
            style.text(string.format('%d / %d', value.current, value.base), {
                relativePosition = util.vector2(0.5, 0.5),
                anchor = util.vector2(0.5, 0.5),
                textSize = 15,
                textColor = util.color.rgb(1, 1, 1),
                textShadow = true,
            }),
        }),
    }
end

local function layout(values)
    local bars = {}
    for i, bar in ipairs(BARS) do
        bars[#bars + 1] = barLayout(bar, values[i], (i - 1) * (BAR_HEIGHT + GAP))
    end
    return {
        layer = style.hudLayer,
        type = ui.TYPE.Widget,
        props = {
            relativePosition = util.vector2(0, 1),
            anchor = util.vector2(0, 1),
            position = util.vector2(16, -64),
            size = util.vector2(BAR_WIDTH, #BARS * (BAR_HEIGHT + GAP) - GAP),
            visible = hudVisible,
        },
        content = ui.content(bars),
    }
end

local function changed(values)
    for i, value in ipairs(values) do
        local old = shown[i]
        if not old or old.current ~= value.current or old.base ~= value.base then
            return true
        end
    end
    return false
end

function hud.start()
    I.UI.setHudElementVisibility(I.UI.HUD_ELEMENT.DynamicStats, false)
end

function hud.onFrame()
    local values = readStats()
    local visible = I.UI.isHudVisible()
    if element and not changed(values) and visible == hudVisible then
        return
    end
    shown = values
    hudVisible = visible
    if element then
        element.layout = layout(values)
        element:update()
    else
        element = ui.create(layout(values))
    end
end

return hud
