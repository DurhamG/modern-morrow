-- Brewing potions. It replaces the engine's Alchemy window with the fork's alchemy screen
-- (ui-rework/design/crafting.md in the fork): the ingredients on the left, each with the effects the player knows, and
-- the potion on the right. A puts the highlighted ingredient in the potion or takes it out, LB and RB change how many
-- potions to brew, X brews, and pressing the left stick names the potion with the on-screen keyboard. The potion takes
-- the name of its first effect until the player names it, and a change of ingredients goes back to that name.
--
-- Once the potion holds an ingredient, the list splits into the ingredients in the potion, those that share an effect
-- with it, and the others. Effects the player doesn't know don't count, so the list never gives them away.
--
-- The engine's Lua API works out the potion, with the best apparatus the player carries, and the global script brews
-- it. The API shows no message and plays no sound, so this screen does both, as the engine's window does.

local core = require('openmw.core')
local self = require('openmw.self')
local types = require('openmw.types')
local ui = require('openmw.ui')
local ambient = require('openmw.ambient')
local I = require('openmw.interfaces')

local style = require('scripts.modernmorrow.ui.style')
local layer = require('scripts.modernmorrow.ui.layer')
local draw = require('scripts.modernmorrow.ui.draw')
local text = require('scripts.modernmorrow.ui.text')
local hints = require('scripts.modernmorrow.ui.hints')
local keyboard = require('scripts.modernmorrow.ui.keyboard')
local list = require('scripts.modernmorrow.ui.list')
local screen = require('scripts.modernmorrow.ui.screen')
local panel = require('scripts.modernmorrow.ui.panel')
local info = require('scripts.modernmorrow.items.info')
local castables = require('scripts.modernmorrow.magic.castables')
local router = require('scripts.modernmorrow.input.router')

local l10n = core.l10n('ModernMorrow')

local alchemy = {}

local color = style.color
local size = style.textSize

local ROW_HEIGHT = 58
local SLOT_HEIGHT = 36
local EFFECT_HEIGHT = 32
local SLOTS = 4

local function L(key)
    return text.escape(l10n(key))
end

local function gmst(name, fallback)
    return core.getGMST(name) or fallback
end

local element = nil
local state = nil

-- An effect written out without its numbers, such as "Restore Fatigue". Plain text.
local function effectName(effect)
    return core.magic.effects.getDescription(effect, { isConstant = true, noTarget = true, noMagnitude = true })
end

local function knownEffects(item)
    local names = {}
    for i, effect in ipairs(types.Player.getKnownIngredientEffects(self, item)) do
        names[i] = effectName(effect)
    end
    return names
end

-- The ingredients in the potion, in the order of their slots, without the empty slots.
local function ingredients()
    local result = {}
    for i = 1, SLOTS do
        if state.slots[i] then
            result[#result + 1] = state.slots[i]
        end
    end
    return result
end

local function slotOf(item)
    for i = 1, SLOTS do
        local slotted = state.slots[i]
        if slotted and slotted.recordId == item.recordId then
            return i
        end
    end
    return nil
end

-- What brewing would make. Without a typed name, the potion gets the name that the engine suggests.
local function potionInfo()
    local ok, result = pcall(types.Player.getAlchemyInfo, self, { ingredients = ingredients(), name = state.name })
    if ok then
        return result
    end
    print('Modern Morrow: ' .. tostring(result))
    return { effects = {}, apparatus = {}, maxCount = 0, suggestedName = '', problem = 'noEffects' }
end

-- Brewing ingredients that share no effect only wastes them, so the screen doesn't offer it.
local function canBrew()
    return state.info.problem == nil
end

-- What the potion side says instead of the name while the potion can't be brewed, and its colour.
local function problemText()
    local problem = state.info.problem
    if problem == 'noMortarAndPestle' then
        return gmst('sNotifyMessage45', 'You need a mortar and pestle to make potions.'), color.worse
    elseif problem == 'lessThanTwoIngredients' then
        return l10n('AlchemyStart'), color.dim
    end
    -- Without a shared effect, the suggested name is empty too, so the engine names that problem first.
    return l10n('AlchemyNoEffects'), color.worse
end

-- The name the player typed, or the one that the engine suggests. Plain text.
local function potionName()
    return state.name or state.info.suggestedName
end

local function readState()
    -- Ingredients that were used up leave their slots.
    for i = 1, SLOTS do
        local item = state.slots[i]
        if item and (not item:isValid() or item.count == 0 or item.parentContainer ~= self.object) then
            state.slots[i] = nil
        end
    end
    state.info = potionInfo()
    state.count = math.max(1, math.min(state.count, math.max(1, state.info.maxCount)))

    -- The selection stays on the same ingredient when it moves between the groups.
    local focusedRow = state.rows and state.list:focused(state.rows)
    local focusedItem = focusedRow and focusedRow.item

    local potionEffects = {}
    for _, item in ipairs(ingredients()) do
        for _, name in ipairs(knownEffects(item)) do
            potionEffects[name] = true
        end
    end
    local groups = { {}, {}, {} }
    for _, item in ipairs(types.Actor.inventory(self):getAll(types.Ingredient)) do
        if item.count > 0 then
            local effects = knownEffects(item)
            local row = { item = item, effects = effects, inPotion = slotOf(item) ~= nil }
            local shares = false
            for _, name in ipairs(effects) do
                if potionEffects[name] then
                    shares = true
                end
            end
            local group = row.inPotion and 1 or (shares and 2 or 3)
            table.insert(groups[group], row)
        end
    end
    -- The groups only get headers once the potion holds an ingredient. Before that, the list is one group.
    local grouped = #groups[1] > 0
    local headers = { 'AlchemyInPotion', 'AlchemySharing', 'AlchemyOther' }
    state.rows = {}
    for g, group in ipairs(groups) do
        table.sort(group, function(a, b)
            return info.name(a.item) < info.name(b.item)
        end)
        if #group > 0 and grouped then
            state.rows[#state.rows + 1] = { header = L(headers[g]) }
        end
        for _, row in ipairs(group) do
            state.rows[#state.rows + 1] = row
        end
    end
    if focusedItem then
        local index = 0
        for _, row in ipairs(state.rows) do
            if row.item then
                index = index + 1
                if row.item == focusedItem then
                    state.list.focus = index
                end
            end
        end
    end
end

local function focused()
    return state.list:focused(state.rows)
end

local function drawEntry(canvas, x, y, w, h, row, isFocused)
    canvas:icon(x + 10, y + 8, 40, row.item, false)
    local textX = x + 62
    local textW = w - 62 - 12
    canvas:text(textX, y + 4, textW, 26, text.fit(panel.itemName(row.item), textW, size.normal), size.normal,
        isFocused and color.active or (row.inPotion and color.new or color.bright))
    local effects = #row.effects > 0 and text.fit(table.concat(row.effects, ', '), textW, size.small)
        or L('AlchemyNoKnownEffects')
    canvas:text(textX, y + 30, textW, 22, effects, size.small, color.dim)
end

local function drawList(canvas, x, y, w, h)
    screen.box(canvas, x, y, w, h)
    canvas:text(x + 16, y + 8, w - 32, 30, text.escape(gmst('sIngredients', 'Ingredients')), size.large,
        color.bright)
    if state.list:itemCount(state.rows) == 0 then
        canvas:text(x + 16, y + 44, w - 32, 30, L('AlchemyNoIngredients'), size.normal, color.dim)
        return
    end
    screen.drawRows(canvas, x + 4, y + 44, w - 8, y + h - 6, state.rows, state.list, ROW_HEIGHT, drawEntry)
end

local function drawPotion(canvas, x, y, w, h)
    screen.box(canvas, x, y, w, h, true)
    local potion = state.info
    local brewable = canBrew()

    -- The potion's name, or what to do first.
    local ry = y + 12
    if brewable then
        canvas:text(x + 16, ry, w - 32, 34, text.fit(potionName(), w - 32, size.large), size.large, color.bright)
        ry = ry + 44
    else
        local message, messageColor = problemText()
        ry = screen.lines(canvas, x + 16, ry, w - 32, message, size.normal, messageColor, 2, 26) + 18
    end

    -- The four ingredient slots.
    canvas:text(x + 16, ry, w - 32, 22, text.escape(gmst('sIngredients', 'Ingredients')), size.small, color.dim)
    ry = ry + 26
    for i = 1, SLOTS do
        local item = state.slots[i]
        if item then
            canvas:icon(x + 16, ry + 2, SLOT_HEIGHT - 6, item, false)
            canvas:text(x + 16 + SLOT_HEIGHT + 4, ry, w - 32 - SLOT_HEIGHT, SLOT_HEIGHT - 2,
                text.escape(info.name(item)), size.normal, color.bright)
        else
            canvas:rect(x + 16, ry + 2, SLOT_HEIGHT - 6, SLOT_HEIGHT - 6, color.iconBack, 0.9)
            canvas:text(x + 16 + SLOT_HEIGHT + 4, ry, w - 32 - SLOT_HEIGHT, SLOT_HEIGHT - 2, L('AlchemyEmptySlot'),
                size.normal, color.dim)
        end
        ry = ry + SLOT_HEIGHT
    end
    ry = ry + 10

    -- The potion's effects. The ones the player doesn't know yet show as unknown, as in the engine's window.
    canvas:text(x + 16, ry, w - 32, 22, text.escape(gmst('sCreatedEffects', 'Created Effects')), size.small,
        color.dim)
    ry = ry + 26
    for _, effect in ipairs(potion.effects) do
        if effect.isKnown then
            local icon = castables.bigIcon(core.magic.effects.records[effect.id])
            if icon then
                canvas:picture(x + 16, ry + 2, EFFECT_HEIGHT - 6, icon)
            end
            canvas:text(x + 16 + EFFECT_HEIGHT + 4, ry, w - 32 - EFFECT_HEIGHT, EFFECT_HEIGHT - 2,
                text.escape(effectName(effect)), size.normal, color.new)
        else
            canvas:rect(x + 16, ry + 2, EFFECT_HEIGHT - 6, EFFECT_HEIGHT - 6, color.iconBack, 0.9)
            canvas:text(x + 16 + EFFECT_HEIGHT + 4, ry, w - 32 - EFFECT_HEIGHT, EFFECT_HEIGHT - 2,
                L('AlchemyUnknownEffect'), size.normal, color.dim)
        end
        ry = ry + EFFECT_HEIGHT
    end
    if #potion.effects == 0 then
        canvas:text(x + 16, ry, w - 32, EFFECT_HEIGHT - 2, '-', size.normal, color.dim)
    end

    -- From the bottom up: the button hints, how many to brew, and the apparatus.
    local list_ = {}
    local row = focused()
    if row then
        list_[#list_ + 1] = { 'a', L(row.inPotion and 'AlchemyRemove' or 'AlchemyAdd') }
    end
    if brewable then
        list_[#list_ + 1] = { 'x', L('AlchemyBrew') }
        list_[#list_ + 1] = { 'ls', L('Rename') }
    end
    local by = hints.draw(canvas, x + 16, w - 32, y + h, list_) - 12

    if brewable then
        local most = math.max(potion.maxCount, 1)
        by = by - 34
        canvas:text(x + 16, by, w - 32, 30, L('AlchemyCount'), size.normal, color.text)
        local value = state.count .. ' / ' .. most
        canvas:text(x + 16, by, w - 32 - 34, 30, value, size.large, color.bright, ui.ALIGNMENT.End)
        local valueLeft = x + w - 16 - 34 - text.width(value, size.large)
        canvas:glyph(valueLeft - 34, by + 2, 26, 'lb')
        canvas:glyph(x + w - 16 - 26, by + 2, 26, 'rb')
        by = by - 8
    end

    local names = {}
    for _, apparatus in ipairs(potion.apparatus) do
        names[#names + 1] = info.name(apparatus)
    end
    local lineCount = screen.lineCount(table.concat(names, ', '), w - 32, size.small, 3)
    by = by - lineCount * 22
    canvas:text(x + 16, by - 24, w - 32, 22, text.escape(gmst('sApparatus', 'Apparatus')), size.small, color.dim)
    screen.lines(canvas, x + 16, by, w - 32, table.concat(names, ', '), size.small, color.text, 3, 22)
end

local function layout()
    readState()
    local canvas = draw.canvas()
    local g = screen.drawFrame(canvas, text.escape(gmst('sSkillAlchemy', 'Alchemy')),
        L('AlchemySkill') .. '  ' .. math.floor(types.NPC.stats.skills.alchemy(self).modified))
    local columns = screen.columns(g, 0.5)
    drawList(canvas, columns.leftX, g.top, columns.leftW, g.bottom - g.top)
    drawPotion(canvas, columns.rightX, g.top, columns.rightW, g.bottom - g.top)
    local root = canvas:layout({ size = layer.size(layer.menu) })
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

local function toggleFocused()
    local row = focused()
    if row == nil then
        return
    end
    local slot = slotOf(row.item)
    if slot then
        state.slots[slot] = nil
        state.name = nil
        ambient.playSound(types.Item.getDownSound(row.item))
    else
        local free = nil
        for i = SLOTS, 1, -1 do
            if state.slots[i] == nil then
                free = i
            end
        end
        if free == nil then
            ui.showMessage(l10n('AlchemyFull'))
            return
        end
        state.slots[free] = row.item
        state.name = nil
        ambient.playSound(types.Item.getUpSound(row.item))
    end
    redraw()
end

local function brew()
    if not canBrew() or state.waiting then
        return
    end
    state.waiting = true
    state.brewing = { name = potionName(), count = math.min(state.count, state.info.maxCount) }
    core.sendGlobalEvent('ModernMorrowBrew', {
        player = self.object,
        setup = { ingredients = ingredients(), count = state.brewing.count, name = state.brewing.name },
    })
end

-- Names the potion with the on-screen keyboard. An empty name keeps the one before.
local function rename()
    if not canBrew() or state.waiting then
        return
    end
    ambient.playSound('menu click')
    keyboard.open({
        label = gmst('sName', 'Name'),
        text = potionName(),
        onDone = function(typed)
            if state then
                state.name = typed
                redraw()
            end
        end,
    })
end

local function changeCount(delta)
    if not canBrew() then
        return
    end
    local count = math.max(1, math.min(state.info.maxCount, state.count + delta))
    if count ~= state.count then
        state.count = count
        ambient.playSound('menu click')
        redraw()
    end
end

alchemy.handler = {
    onButton = function(name)
        if state == nil then
            return
        end
        if name == 'a' then
            toggleFocused()
        elseif name == 'x' then
            brew()
        elseif name == 'lb' then
            changeCount(-1)
        elseif name == 'rb' then
            changeCount(1)
        elseif name == 'ls' then
            rename()
        elseif name == 'b' then
            I.UI.removeMode('Alchemy')
        end
    end,
    onMove = function(_, dy)
        if state == nil or dy == 0 then
            return
        end
        if state.list:move(state.rows, dy) then
            redraw()
        end
    end,
}

-- From the global script, after brewing. The engine played the sound and showed the message.
function alchemy.onBrewResult()
    if state == nil then
        return
    end
    state.waiting = false
    -- Event handlers may run on the Lua worker thread, which can't measure text, so the next frame redraws.
    state.dirty = true
end

local function signature()
    local parts = {}
    for _, item in ipairs(types.Actor.inventory(self):getAll(types.Ingredient)) do
        parts[#parts + 1] = item.id .. ':' .. item.count
    end
    parts[#parts + 1] = tostring(types.NPC.stats.skills.alchemy(self).modified)
    return table.concat(parts, ',')
end

local function show()
    state = { slots = {}, count = 1, list = list.new() }
    router.push(alchemy.handler)
    if element then
        element:destroy()
    end
    element = ui.create(layout())
    state.signature = signature()
end

local function hide()
    keyboard.close()
    router.remove(alchemy.handler)
    if element then
        element:destroy()
        element = nil
    end
    state = nil
end

function alchemy.onFrame()
    if element == nil or state == nil then
        return
    end
    local current = signature()
    if current ~= state.signature or state.dirty then
        state.signature = current
        state.dirty = false
        redraw()
    end
end

function alchemy.register()
    I.UI.registerWindow('Alchemy', show, hide, { capturesInput = true })
end

return alchemy
