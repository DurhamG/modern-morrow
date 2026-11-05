-- Repairing with the player's own tools and recharging with soul gems. It replaces the engine's Repair and Recharge
-- windows, as the fork's tool panel does (ui-rework/design/crafting.md in the fork): one full screen for both, with the
-- items that need it on the left, worst first, and the tool on the right. A uses the tool on the highlighted item, Y
-- switches to the next tool of the same kind, and when a tool runs out the next one takes its place.
--
-- The global script uses the tool, with the engine's rules. The engine plays the sounds and shows the messages, such as
-- whether the repair worked.

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
local list = require('scripts.modernmorrow.ui.list')
local screen = require('scripts.modernmorrow.ui.screen')
local info = require('scripts.modernmorrow.items.info')
local router = require('scripts.modernmorrow.input.router')

local l10n = core.l10n('ModernMorrow')

local tools = {}

local color = style.color
local size = style.textSize

local ROW_HEIGHT = 58

local function L(key)
    return text.escape(l10n(key))
end

local function gmst(name, fallback)
    return text.escape(core.getGMST(name) or fallback)
end

local function skillValue(skillId)
    return math.floor(types.NPC.stats.skills[skillId](self).modified)
end

-- An item's charge and the most it holds, or nil when it was never used, which counts as full and can't be
-- recharged.
local function chargeOf(item)
    local enchantment = info.enchantment(item)
    if enchantment == nil then
        return nil
    end
    local data = types.Item.itemData(item)
    local charge = data and data.enchantmentCharge
    if charge == nil then
        return nil
    end
    return math.floor(charge), math.floor(core.magic.enchantments.getMaxCharge(enchantment))
end

-- The soul gems with a soul of a creature that exists.
local function hasSoul(item)
    local data = types.Item.itemData(item)
    local soul = data and data.soul
    return soul ~= nil and soul ~= '' and types.Creature.records[soul] ~= nil
end

-- Each kind of screen has its window, its title, the skill that decides whether a use works, and:
--   items()          the items that need the tool, each with `item`, `value` and `most`
--   tools()          the tools in the player's inventory
--   facts(tool)      { label, value } pairs about the tool, escaped
--   use(item, tool)  uses the tool once on the item
local KINDS = {}

KINDS.Repair = {
    window = 'Repair',
    title = function()
        return gmst('sRepair', 'Repair')
    end,
    skill = 'armorer',
    listTitle = 'ToolDamaged',
    empty = 'ToolNothingToRepair',
    toolTitle = 'ToolRepairTool',
    noTools = 'ToolNoRepairTools',
    help = 'ToolRepairHelp',
    action = 'Repair',
    valueLabel = function()
        return gmst('sCondition', 'Condition')
    end,
    items = function()
        local result = {}
        local inventory = types.Actor.inventory(self)
        for _, itemType in ipairs({ types.Weapon, types.Armor }) do
            for _, item in ipairs(inventory:getAll(itemType)) do
                local most = itemType.record(item).health
                local value = info.condition(item)
                if info.hasCondition(item) and value and most and value < most then
                    result[#result + 1] = { item = item, value = math.floor(value), most = most }
                end
            end
        end
        return result
    end,
    tools = function()
        return types.Actor.inventory(self):getAll(types.Repair)
    end,
    facts = function(tool)
        return {
            { gmst('sUses', 'Uses'), tostring(info.condition(tool) or 0) },
            { gmst('sQuality', 'Quality'), text.number(types.Repair.record(tool).quality) },
        }
    end,
    use = function(item, tool)
        core.sendGlobalEvent('ModernMorrowRepair', { player = self.object, item = item, tool = tool })
    end,
}

KINDS.Recharge = {
    window = 'Recharge',
    title = function()
        return gmst('sRechargeEnchantment', 'Recharge')
    end,
    skill = 'enchant',
    listTitle = 'ToolDrained',
    empty = 'ToolNothingToRecharge',
    toolTitle = function()
        return gmst('sSoulGem', 'Soul Gem')
    end,
    noTools = 'ToolNoSoulGems',
    help = 'ToolRechargeHelp',
    action = 'ToolRecharge',
    valueLabel = function()
        return L('MagicCharge')
    end,
    items = function()
        local result = {}
        for _, item in ipairs(types.Actor.inventory(self):getAll()) do
            local value, most = chargeOf(item)
            if value and value < most then
                result[#result + 1] = { item = item, value = value, most = most }
            end
        end
        return result
    end,
    tools = function()
        local result = {}
        for _, item in ipairs(types.Actor.inventory(self):getAll(types.Miscellaneous)) do
            if info.isSoulGem(item) and hasSoul(item) then
                result[#result + 1] = item
            end
        end
        return result
    end,
    facts = function(tool)
        local soul = types.Creature.records[types.Item.itemData(tool).soul]
        return {
            { L('ToolSoul'), text.escape(soul.name) },
            { L('ToolSoulStrength'), tostring(soul.soulValue) },
        }
    end,
    use = function(item, tool)
        core.sendGlobalEvent('ModernMorrowRecharge', { player = self.object, item = item, soulGem = tool })
    end,
}

local element = nil
local state = nil

local function caption(value)
    if type(value) == 'function' then
        return value()
    end
    return L(value)
end

local function toolUsable(tool)
    return tool ~= nil and tool:isValid() and tool.count > 0 and tool.parentContainer == self.object
end

local function readState()
    -- When the tool runs out, the next one of its kind takes its place.
    if not toolUsable(state.tool) then
        state.tool = state.kind.tools()[1]
    end
    local items = state.kind.items()
    -- The ones in the worst shape come first.
    table.sort(items, function(a, b)
        local ra, rb = a.value / math.max(1, a.most), b.value / math.max(1, b.most)
        if ra ~= rb then
            return ra < rb
        end
        return info.name(a.item) < info.name(b.item)
    end)
    state.rows = {}
    for i, entry in ipairs(items) do
        state.rows[i] = { entry = entry }
    end
end

local function focused()
    local row = state.list:focused(state.rows)
    return row and row.entry
end

local function drawEntry(canvas, x, y, w, h, row, isFocused)
    local entry = row.entry
    canvas:icon(x + 10, y + 8, 40, entry.item, false)
    local textX = x + 62
    local textW = x + w - 16 - textX
    canvas:text(textX, y + 4, textW, 26, text.fit(info.name(entry.item), textW - 110, size.normal), size.normal,
        isFocused and color.active or color.bright)
    canvas:text(textX, y + 4, textW, 26, entry.value .. ' / ' .. entry.most, size.normal, color.text,
        ui.ALIGNMENT.End)
    screen.bar(canvas, textX, y + 38, textW, 6, entry.value, entry.most)
end

local function drawList(canvas, x, y, w, h)
    screen.box(canvas, x, y, w, h)
    canvas:text(x + 16, y + 8, w - 32, 30, caption(state.kind.listTitle), size.large, color.bright)
    if #state.rows == 0 then
        canvas:text(x + 16, y + 44, w - 32, 30, caption(state.kind.empty), size.normal, color.dim)
        return
    end
    screen.drawRows(canvas, x + 4, y + 44, w - 8, y + h - 6, state.rows, state.list, ROW_HEIGHT, drawEntry)
end

local function drawTool(canvas, x, y, w, h)
    local kind = state.kind
    screen.box(canvas, x, y, w, h, true)
    canvas:text(x + 16, y + 8, w - 32, 30, caption(kind.toolTitle), size.large, color.bright)

    local tool = state.tool
    local ry = y + 52
    if not toolUsable(tool) then
        canvas:text(x + 16, ry, w - 32, 30, caption(kind.noTools), size.normal, color.worse)
    else
        canvas:icon(x + 16, ry, 56, tool, false)
        local name = info.name(tool)
        if tool.count > 1 then
            name = name .. ' (' .. tool.count .. ')'
        end
        canvas:text(x + 84, ry, w - 100, 28, text.fit(name, w - 100, size.normal), size.normal, color.bright)
        local fy = ry + 30
        for _, fact in ipairs(kind.facts(tool)) do
            canvas:text(x + 84, fy, w - 100, 24, fact[1], size.small, color.dim)
            canvas:text(x + 84, fy, w - 100, 24, fact[2], size.small, color.text, ui.ALIGNMENT.End)
            fy = fy + 22
        end
    end
    ry = ry + 110

    -- What decides whether a use works.
    ry = screen.lines(canvas, x + 16, ry, w - 32, l10n(kind.help), size.small, color.dim, 4, 22)

    -- The highlighted item, larger.
    local entry = focused()
    if entry then
        ry = ry + 20
        canvas:rect(x + 16, ry, w - 32, 1, color.accent, 0.2)
        ry = ry + 14
        canvas:icon(x + 16, ry, 56, entry.item, false)
        canvas:text(x + 84, ry, w - 100, 30, text.fit(info.name(entry.item), w - 100, size.large), size.large,
            color.bright)
        canvas:text(x + 84, ry + 30, w - 100, 24,
            caption(kind.valueLabel) .. '  ' .. entry.value .. ' / ' .. entry.most, size.normal, color.text)
        screen.bar(canvas, x + 16, ry + 70, w - 32, 10, entry.value, entry.most)
    end

    local list_ = {}
    if toolUsable(tool) and entry then
        list_[#list_ + 1] = { 'a', L(kind.action) }
    end
    if #kind.tools() > 1 then
        list_[#list_ + 1] = { 'y', L('ToolNext') }
    end
    hints.draw(canvas, x + 16, w - 32, y + h, list_)
end

local function layout()
    readState()
    local canvas = draw.canvas()
    local skill = core.stats.Skill.records[state.kind.skill]
    local g = screen.drawFrame(canvas, state.kind.title(),
        text.escape(skill.name) .. '  ' .. skillValue(state.kind.skill))
    local columns = screen.columns(g, 0.55)
    drawList(canvas, columns.leftX, g.top, columns.leftW, g.bottom - g.top)
    drawTool(canvas, columns.rightX, g.top, columns.rightW, g.bottom - g.top)
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

local function signature()
    local parts = { tostring(state.tool and state.tool:isValid() and state.tool.count) }
    for _, item in ipairs(types.Actor.inventory(self):getAll()) do
        local data = types.Item.itemData(item)
        parts[#parts + 1] = string.format('%s:%d:%s:%s', item.id, item.count, tostring(data and data.condition),
            tostring(data and data.enchantmentCharge))
    end
    return table.concat(parts, ',')
end

local function useTool()
    local entry = focused()
    if entry == nil or not toolUsable(state.tool) then
        return
    end
    state.kind.use(entry.item, state.tool)
end

local function nextTool()
    local available = state.kind.tools()
    if #available < 2 then
        return
    end
    local index = 0
    for i, tool in ipairs(available) do
        if tool == state.tool then
            index = i
        end
    end
    state.tool = available[index % #available + 1]
    ambient.playSound(types.Item.getDownSound(state.tool))
    redraw()
end

tools.handler = {
    onButton = function(name)
        if state == nil then
            return
        end
        if name == 'a' then
            useTool()
        elseif name == 'y' then
            nextTool()
        elseif name == 'b' then
            I.UI.removeMode(state.kind.window)
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

local function show(kind, tool)
    state = { kind = kind, tool = tool, list = list.new(), rows = {} }
    router.push(tools.handler)
    if element then
        element:destroy()
    end
    element = ui.create(layout())
    state.signature = signature()
end

local function hide()
    router.remove(tools.handler)
    if element then
        element:destroy()
        element = nil
    end
    state = nil
end

-- Redraws after each use, which changes the item and wears down the tool.
function tools.onFrame()
    if element == nil or state == nil then
        return
    end
    local current = signature()
    if current ~= state.signature then
        state.signature = current
        redraw()
    end
end

function tools.register()
    for _, kind in pairs(KINDS) do
        -- The engine opens the window with the tool that the player used from the inventory.
        I.UI.registerWindow(kind.window, function(tool)
            show(kind, tool)
        end, hide, { capturesInput = true })
    end
end

return tools
