-- Scrolling text with a stick, and the thin scroll bar beside it.

local style = require('scripts.modernmorrow.ui.style')

local scroll = {}

-- How far text scrolls per second with a stick pushed all the way, in layer units.
scroll.SPEED = 700

-- The scroll position after `dt` seconds with the stick at `stickY`, from -1 (up) to 1 (down), kept between 0 and
-- `max`. Squaring the deflection gives finer control near the centre of the stick.
function scroll.step(position, max, stickY, dt)
    local magnitude = math.abs(stickY)
    if max <= 0 or magnitude < style.stickDeadzone then
        return math.max(0, math.min(position, math.max(0, max)))
    end
    local scaled = (magnitude - style.stickDeadzone) / (1 - style.stickDeadzone)
    local step = scaled * scaled * scroll.SPEED * dt
    if stickY < 0 then
        step = -step
    end
    return math.max(0, math.min(max, position + step))
end

-- Draws a scroll bar in a track of height `h`, for content of height `contentH` that shows `visibleH` of itself at a
-- time, scrolled down by `position`.
function scroll.drawBar(canvas, x, y, h, position, contentH, visibleH, color, trackAlpha, thumbAlpha)
    local max = contentH - visibleH
    if max <= 0 then
        return
    end
    local width = 4
    canvas:rect(x, y, width, h, color, trackAlpha)
    local thumbH = math.max(16, math.floor(h * visibleH / contentH))
    local thumbY = y + math.floor((h - thumbH) * math.min(1, position / max))
    canvas:rect(x, thumbY, width, thumbH, color, thumbAlpha)
end

return scroll
