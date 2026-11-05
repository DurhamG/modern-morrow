-- The look that every Modern Morrow screen shares: its colours, text sizes and spacing.
-- The numbers are units of the 720-unit layer (see ui/layer.lua), and match the fork's ControllerUi namespace.

local util = require('openmw.util')

local hex = util.color.hex

local style = {}

style.color = {
    -- Ordinary text, such as list rows.
    text = hex('caa560'),
    -- Names and headers that should stand out.
    bright = hex('dfc99f'),
    -- Labels and secondary lines.
    dim = hex('8c7a55'),
    -- The highlighted row's text.
    active = util.color.rgb(1, 1, 1),
    -- Selection bars, underlines and thin rules.
    accent = hex('caa560'),
    -- Markers for new and unread things.
    new = hex('7fb3ff'),
    -- Stats that beat what is equipped, and stats that fall short.
    better = hex('7fd36b'),
    worse = hex('e0675a'),
    panel = hex('15120c'),
    iconBack = hex('1a1610'),
    -- The background of dialogs over a darkened screen, such as the count picker.
    modal = hex('0d0b08'),
    -- Book text is dark, as the books set it, so it sits on a page of this colour.
    page = hex('d8c7a0'),
    pageText = hex('1e160c'),
    glyph = hex('ccb589'),
    black = util.color.rgb(0, 0, 0),
}

style.textSize = {
    small = 15,
    normal = 18,
    large = 22,
    title = 26,
}

style.space = {
    -- The bar at the top, with the names of the screens, covers this much of the layer.
    topBarHeight = 44,
    margin = 32,
    -- Where a screen's own content starts, below the top bar and the screen's tab row.
    contentTop = 110,
    contentBottomMargin = 24,
}

-- How far a stick must move before it counts, as a fraction of its travel.
style.stickDeadzone = 0.15

return style
