-- The level-up. It replaces the engine's LevelUpDialog with the fork's level-up screen
-- (ui-rework/design/other-screens.md in the fork): the class picture and the level's text on the left, and the
-- attributes as tiles with their bonuses on the right. The d-pad moves between the tiles, A chooses an attribute or
-- takes it back, and X finishes once enough are chosen. The highlighted attribute's description and the skills that
-- raise it show below the tiles.
--
-- The numbers come from types.Player.getLevelUpInfo, and the global script levels the player up.

local core = require('openmw.core')
local self = require('openmw.self')
local types = require('openmw.types')
local ui = require('openmw.ui')
local ambient = require('openmw.ambient')
local vfs = require('openmw.vfs')
local I = require('openmw.interfaces')

local style = require('scripts.modernmorrow.ui.style')
local layer = require('scripts.modernmorrow.ui.layer')
local draw = require('scripts.modernmorrow.ui.draw')
local text = require('scripts.modernmorrow.ui.text')
local hints = require('scripts.modernmorrow.ui.hints')
local screen = require('scripts.modernmorrow.ui.screen')
local router = require('scripts.modernmorrow.input.router')

local l10n = core.l10n('ModernMorrow')

local levelup = {}

local color = style.color
local size = style.textSize

local PER_COLUMN = 4
local GAP = 12
local TILE_HEIGHT = 74

local function L(key)
    return text.escape(l10n(key))
end

local element = nil
local state = nil

local function attributes()
    local result = {}
    for _, record in ipairs(core.stats.Attribute.records) do
        result[#result + 1] = record
    end
    return result
end

local function chosenIndex(id)
    for i, chosen in ipairs(state.chosen) do
        if chosen == id then
            return i
        end
    end
    return nil
end

local function remaining()
    return math.max(0, state.info.attributeCount - #state.chosen)
end

-- The class picture, or the warrior's when the class has none, as the engine's window falls back.
local function classImage()
    local path = 'textures/levelup/' .. state.info.classImage .. '.dds'
    if not vfs.fileExists(path) then
        path = 'textures/levelup/warrior.dds'
    end
    return path
end

local function drawTile(canvas, x, y, w, record, focused)
    local base = math.floor(types.Actor.stats.attributes[record.id](self).base)
    local bonus = state.info.attributeIncreases[record.id] or 0
    local maxed = bonus <= 0
    local chosen = chosenIndex(record.id) ~= nil
    canvas:rect(x, y, w, TILE_HEIGHT, color.panel, 0.9)
    if chosen then
        canvas:rect(x, y, w, TILE_HEIGHT, color.better, 0.12)
    end
    if focused then
        canvas:rect(x, y, w, TILE_HEIGHT, color.accent, 0.16)
        canvas:rect(x, y, 3, TILE_HEIGHT, color.accent)
    end
    local iconSize = TILE_HEIGHT - 24
    if record.icon and record.icon ~= '' then
        canvas:picture(x + 12, y + 12, iconSize, record.icon)
    end
    local textX = x + 12 + iconSize + 12
    local textW = w - (textX - x) - 110
    canvas:text(textX, y + 8, textW, 30, text.escape(record.name), size.large,
        maxed and color.dim or (focused and color.active or color.bright))
    local bonusText = maxed and L('LevelUpMaxed') or ('+' .. bonus)
    canvas:text(textX, y + 40, textW, 24, bonusText, size.normal,
        maxed and color.dim or (bonus > 1 and color.better or color.text))
    -- The value now, and after the level-up when the attribute is chosen.
    local value = chosen and (base .. ' -> ' .. math.min(100, base + bonus)) or tostring(base)
    canvas:text(x + w - 120, y, 108, TILE_HEIGHT, value, size.large, chosen and color.better or color.bright,
        ui.ALIGNMENT.End)
end

local function layout()
    local layerSize = layer.size(layer.menu)
    local w, h = layerSize.x, layerSize.y
    local margin = screen.MARGIN
    local canvas = draw.canvas()
    canvas:rect(0, 0, w, h, color.black, 0.97)

    -- The class picture, the new level and its description on the left.
    local leftW = math.max(320, math.floor(w * 0.34))
    local x = margin
    local ty = margin
    local imageH = math.floor(leftW / 2)
    canvas:image(x, ty, leftW, imageH, draw.texture(classImage()))
    canvas:rect(x, ty, leftW, 1, color.accent, 0.6)
    ty = ty + imageH + 18
    local levelText = (core.getGMST('sLevelUpMenu1') or 'You have ascended to Level ') .. state.info.level
    ty = screen.lines(canvas, x, ty, leftW, levelText, size.title, color.bright, 2, 32) + 8
    screen.lines(canvas, x, ty, leftW, state.info.description, size.normal, color.text, 12)

    -- The attributes as tiles on the right, in two columns.
    local rx = x + leftW + 40
    local rw = w - margin - rx
    local left = remaining()
    canvas:text(rx, margin, rw, 34, L('LevelUpChoose'), size.large, color.bright)
    canvas:text(rx, margin, rw, 34, left .. ' ' .. L('LevelUpLeft'), size.normal,
        left > 0 and color.new or color.better, ui.ALIGNMENT.End)
    local tileW = math.floor((rw - GAP) / 2)
    local tilesTop = margin + 50
    local all = attributes()
    for i, record in ipairs(all) do
        local column = math.floor((i - 1) / PER_COLUMN)
        local row = (i - 1) % PER_COLUMN
        drawTile(canvas, rx + column * (tileW + GAP), tilesTop + row * (TILE_HEIGHT + GAP), tileW, record,
            i == state.focus)
    end

    -- The highlighted attribute's description, and the skills that raise its bonus.
    local focused = all[state.focus]
    local dy = tilesTop + PER_COLUMN * (TILE_HEIGHT + GAP) + 6
    local bottom = h - margin - 40
    if focused then
        local lineHeight = size.normal + 6
        for _, line in ipairs(text.wrap(focused.description or '', rw, size.normal, 4)) do
            if dy + lineHeight > bottom then
                break
            end
            canvas:text(rx, dy, rw, lineHeight, line, size.normal, color.text)
            dy = dy + lineHeight
        end
        local skills = {}
        for _, skill in ipairs(core.stats.Skill.records) do
            if skill.attribute == focused.id then
                skills[#skills + 1] = skill.name
            end
        end
        if #skills > 0 and dy + 26 <= bottom then
            canvas:text(rx, dy + 6, rw, 24, L('LevelUpSkills') .. ' ' .. text.escape(table.concat(skills, ', ')),
                size.small, color.dim)
        end
    end

    -- X stays in the hints while attributes are left to choose, dimmed, so the player knows how to finish.
    hints.draw(canvas, rx, rw, h - margin + 6, { { 'a', L('LevelUpPick') } })
    local doneX = rx + 34 + text.width(L('LevelUpPick'), size.normal) + 24
    canvas:glyph(doneX, h - margin - 34, 26, 'x')
    canvas:text(doneX + 34, h - margin - 38, 200, 34, text.escape(core.getGMST('sDone') or 'Done'), size.normal,
        left == 0 and color.bright or color.dim)

    local root = canvas:layout({ size = layerSize })
    root.layer = layer.menu
    return root
end

local function redraw()
    if element == nil then
        return
    end
    element.layout = layout()
    element:update()
end

-- Chooses the highlighted attribute or takes it back. When enough are chosen already, the last one gives way, as in
-- the engine's window.
local function toggle()
    local record = attributes()[state.focus]
    if record == nil or (state.info.attributeIncreases[record.id] or 0) <= 0 then
        return
    end
    local index = chosenIndex(record.id)
    if index then
        table.remove(state.chosen, index)
    elseif #state.chosen >= state.info.attributeCount then
        state.chosen[state.info.attributeCount] = record.id
    else
        state.chosen[#state.chosen + 1] = record.id
    end
    ambient.playSound('menu click')
    redraw()
end

local function finish()
    if remaining() > 0 then
        ui.showMessage(core.getGMST('sNotifyMessage36') or 'You have to distribute all you points before exiting.')
        return
    end
    ambient.playSound('menu click')
    core.sendGlobalEvent('ModernMorrowLevelUp', { player = self.object, attributes = state.chosen })
    I.UI.removeMode('LevelUp')
end

levelup.handler = {
    onButton = function(name)
        if state == nil then
            return
        end
        if name == 'a' then
            toggle()
        elseif name == 'x' then
            finish()
        end
    end,
    onMove = function(dx, dy)
        if state == nil then
            return
        end
        local count = #attributes()
        local column = math.floor((state.focus - 1) / PER_COLUMN)
        local row = (state.focus - 1) % PER_COLUMN
        if dy ~= 0 then
            row = math.max(0, math.min(PER_COLUMN - 1, row + dy))
        elseif dx ~= 0 then
            column = math.max(0, math.min(math.ceil(count / PER_COLUMN) - 1, column + dx))
        end
        local focus = math.min(count, column * PER_COLUMN + row + 1)
        if focus ~= state.focus then
            state.focus = focus
            redraw()
        end
    end,
}

local function show()
    state = { info = types.Player.getLevelUpInfo(self), chosen = {}, focus = 1 }
    router.push(levelup.handler)
    if element then
        element:destroy()
    end
    element = ui.create(layout())
end

local function hide()
    router.remove(levelup.handler)
    if element then
        element:destroy()
        element = nil
    end
    state = nil
end

function levelup.register()
    I.UI.registerWindow('LevelUpDialog', show, hide, { capturesInput = true })
end

return levelup
