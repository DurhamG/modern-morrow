-- The barter screen, which replaces the engine's Trade window. It is the fork's barter panel
-- (ui-rework/design/barter.md in the fork): the Buy and Sell tabs list what the merchant sells and what they buy from
-- the player, A and the d-pad put items into the deal, the right stick haggles, and X makes the offer.
--
-- Nothing moves until the offer: the deal is a list of items and counts, and the global script makes the trade with
-- types.Player.barter, which rolls the haggle as the engine's window does.
--
-- The merchant's goods come from types.Actor.getBarterItems, which also lists the items in the containers that the
-- merchant owns nearby. Items of the same kind from different places show as one stack, as in the engine's window.

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
local panel = require('scripts.modernmorrow.ui.panel')
local info = require('scripts.modernmorrow.items.info')
local details = require('scripts.modernmorrow.items.details')
local router = require('scripts.modernmorrow.input.router')

local l10n = core.l10n('ModernMorrow')

local barter = { window = 'Trade', mode = 'Barter' }

local color = style.color
local size = style.textSize

-- How often the offer changes while the right stick is held, in seconds, and how far the stick must be pushed.
local HAGGLE_INTERVAL = 0.06
local HAGGLE_THRESHOLD = 0.3
-- How often the screen looks for changes in the goods, in seconds. Listing the merchant's goods searches the nearby
-- containers, so it doesn't happen every frame.
local CHECK_INTERVAL = 0.5

local function L(key)
    return text.escape(l10n(key))
end

local function gmst(name, fallback)
    return core.getGMST(name) or fallback
end

local element = nil
local state = nil

local function playerGold()
    return types.Actor.inventory(self):countOf('gold_001')
end

local function merchantGold()
    return types.Actor.getBarterGold(state.merchant)
end

-- ---------------------------------------------------------------------------------------------------------------
-- The deal

-- The deal for the current tab: the items bought, or the items sold, by the key of their stack. Each entry has the
-- stack's `items` and the `count` in the deal.
local function side(selling)
    if selling == nil then
        selling = state.selling
    end
    return selling and state.deal.sell or state.deal.buy
end

local function inDeal(key, selling)
    local entry = side(selling)[key]
    return entry and entry.count or 0
end

local function dealCount(selling)
    local count = 0
    for _, entry in pairs(side(selling)) do
        count = count + entry.count
    end
    return count
end

local function hasDeal()
    return dealCount(false) + dealCount(true) > 0
end

local function isThere(item)
    return item:isValid() and item.count > 0
end

-- Adds a stack in the deal to a list of types.Player.barter's items. The count is taken from the stack's items in
-- order, and items that are gone since are left out.
local function addStack(list_, entry)
    local left = entry.count
    for _, item in ipairs(entry.items) do
        if left <= 0 then
            break
        end
        if isThere(item) then
            local count = math.min(left, item.count)
            list_[#list_ + 1] = { item = item, count = count }
            left = left - count
        end
    end
end

-- The deal as types.Player.barter takes it.
local function setup(gold)
    local result = { buy = {}, sell = {}, gold = gold }
    for _, entry in pairs(state.deal.buy) do
        addStack(result.buy, entry)
    end
    for _, entry in pairs(state.deal.sell) do
        addStack(result.sell, entry)
    end
    return result
end

-- What the merchant offers for the deal: positive when the merchant pays, negative when the player pays.
local function merchantOffer()
    local ok, offer = pcall(types.Player.getBarterOffer, self, state.merchant, setup())
    if ok then
        return offer
    end
    return 0
end

local function unitPrice(item, selling)
    if selling then
        return types.Actor.getSellingPrice(state.merchant, item, 1)
    end
    return types.Actor.getBuyingPrice(state.merchant, item, 1)
end

-- After each change of the deal, the offer goes back to the merchant's price, as in the engine's window.
local function dealChanged()
    state.offer = merchantOffer()
    state.balance = state.offer
end

-- ---------------------------------------------------------------------------------------------------------------
-- The lists

-- What makes items of the same kind one stack: the record, and what can differ between two copies of it.
local function stackKey(item)
    local data = types.Item.itemData(item)
    local owner = item.owner
    return table.concat({
        item.recordId,
        tostring(data and data.condition),
        tostring(data and data.enchantmentCharge),
        tostring(data and data.soul),
        tostring(owner and owner.recordId),
        tostring(owner and owner.factionId),
    }, '|')
end

-- The merchant's goods, as stacks. Each stack has a `key`, its `items` and their `total` count.
local function merchantStacks()
    local stacks = {}
    local byKey = {}
    for _, item in ipairs(types.Actor.getBarterItems(state.merchant)) do
        if item.count > 0 then
            local key = stackKey(item)
            local stack = byKey[key]
            if stack == nil then
                stack = { key = key, items = {}, total = 0 }
                byKey[key] = stack
                stacks[#stacks + 1] = stack
            end
            stack.items[#stack.items + 1] = item
            stack.total = stack.total + item.count
        end
    end
    return stacks
end

-- The player's items that the merchant buys, as stacks of one item each, since the inventory stacks them already.
local function playerStacks()
    local stacks = {}
    for _, item in ipairs(types.Actor.inventory(self):getAll()) do
        if item.count > 0 and types.Actor.canBarterItem(state.merchant, item) then
            stacks[#stacks + 1] = { key = item.id, items = { item }, total = item.count }
        end
    end
    return stacks
end

local function readEntries()
    local entries = {}
    local equipped = {}
    if state.selling then
        for _, item in pairs(types.Actor.getEquipment(self)) do
            equipped[item.id] = true
        end
    end
    for _, stack in ipairs(state.selling and playerStacks() or merchantStacks()) do
        local count = math.min(inDeal(stack.key), stack.total)
        entries[#entries + 1] = {
            key = stack.key,
            item = stack.items[1],
            items = stack.items,
            total = stack.total,
            inDeal = count,
            available = stack.total - count,
            equipped = equipped[stack.items[1].id] == true,
        }
    end
    return entries
end

local function currentList()
    return state.selling and state.sellList or state.buyList
end

local function buildRows()
    local rows = panel.groupedRows(readEntries())
    for _, row in ipairs(rows) do
        if row.entry and row.entry.inDeal > 0 then
            row.tint = true
        end
    end
    return rows
end

local function focusedEntry(rows)
    local row = currentList():focused(rows or buildRows())
    return row and row.entry
end

-- ---------------------------------------------------------------------------------------------------------------
-- Drawing

local function drawEntry(canvas, x, y, w, h, row, focused)
    local entry = row.entry
    local item = entry.item
    local total = entry.total
    local marker, markerColor = nil, color.new
    if entry.inDeal > 0 then
        marker = L(state.selling and 'BarterSelling' or 'BarterBuying')
        if total > 1 then
            marker = marker .. ' ' .. entry.inDeal
        end
    elseif state.selling and entry.equipped then
        marker, markerColor = L('Equipped'), color.dim
    elseif not state.selling and info.isUpgrade(item) then
        marker, markerColor = L('LootBetter'), color.better
    end
    panel.drawEntry(canvas, x, y, w, h, focused, {
        item = item,
        equipped = entry.equipped,
        name = text.escape(panel.itemName(item, total)),
        second = info.summary(item),
        topRight = unitPrice(item, state.selling) .. ' ' .. L('BarterGoldShort'),
        bottomRight = marker,
        bottomRightColor = markerColor,
        rightW = 120,
    })
end

local function drawDeal(canvas, x, w, bottom)
    local buying, selling = dealCount(false), dealCount(true)
    local top = bottom - 100
    canvas:rect(x + 12, top, w - 24, 100, color.accent, 0.08)
    canvas:rect(x + 12, top, w - 24, 1, color.accent, 0.3)
    local items = {}
    if buying > 0 then
        items[#items + 1] = L('BarterBuying') .. ' ' .. buying
    end
    if selling > 0 then
        items[#items + 1] = L('BarterSelling') .. ' ' .. selling
    end
    canvas:text(x + 24, top + 6, w - 48, 24, table.concat(items, ',  '), size.small, color.dim)
    -- A negative balance is what the player pays, and a positive one what they get.
    canvas:text(x + 24, top + 32, w - 48, 30, L(state.balance < 0 and 'BarterYouPay' or 'BarterYouGet'), size.normal,
        color.text)
    canvas:text(x + 24, top + 32, w - 48, 30, tostring(math.abs(state.balance)), size.large,
        state.balance ~= state.offer and color.new or color.bright, ui.ALIGNMENT.End)
    canvas:text(x + 24, top + 66, w - 48, 24,
        L(state.offer < 0 and 'BarterMerchantAsks' or 'BarterMerchantOffers') .. ' ' .. math.abs(state.offer),
        size.small, color.dim)
    -- Paying more gold than the player has, or asking more than the merchant has, can't work.
    if (state.balance < 0 and playerGold() < -state.balance) or (state.balance > 0 and merchantGold() < state.balance) then
        canvas:text(x + 24, top + 6, w - 48, 24, L('BarterNotEnoughGold'), size.small, color.worse,
            ui.ALIGNMENT.End)
    end
    return top
end

local function drawDetails(canvas, g, rows, entry)
    local x, y, w = g.detailsX, g.detailsY, g.detailsW
    local bottom = g.bottom
    local list_ = {}
    if entry then
        local action = entry.inDeal > 0 and 'BarterRemove' or (state.selling and 'BarterSellAction' or 'BarterBuyAction')
        list_[#list_ + 1] = { 'a', L(action) }
        if entry.total > 1 then
            list_[#list_ + 1] = { 'dpad', L('BarterOneMoreLess') }
        end
    end
    if currentList():groupCount(rows) > 1 then
        list_[#list_ + 1] = { 'y', L('MagicNextGroup') }
    end
    if hasDeal() then
        list_[#list_ + 1] = { 'x', L('BarterOffer') }
        list_[#list_ + 1] = { 'rstick', L('BarterHaggle') }
    end
    local hintsTop = hints.draw(canvas, x + 16, w - 32, bottom - 4, list_)
    local dealTop = hintsTop - 12
    if hasDeal() then
        dealTop = drawDeal(canvas, x, w, hintsTop - 10) - 2
    end
    if entry == nil then
        return
    end
    local item = entry.item
    details.drawHeader(canvas, x, y, w, { item = item, equipped = entry.equipped }, state.selling)
    local priceY = y + 104
    panel.fact(canvas, x, priceY, w, L(state.selling and 'BarterSellPrice' or 'BarterBuyPrice'),
        tostring(unitPrice(item, state.selling)))
    -- What a bought item would replace, as on the loot screen.
    local compared = state.selling and {} or details.comparedWith(item)
    details.drawStats(canvas, x, priceY + 34, w, dealTop - 8, item, compared)
end

local function layout()
    -- The merchant's price follows their disposition, which a refused offer lowers. An offer the player hasn't haggled
    -- follows it too.
    local offer = merchantOffer()
    if state.balance == state.offer then
        state.balance = offer
    end
    state.offer = offer
    local canvas = draw.canvas()
    local status = string.format('%s  %d      %s  %d', L('BarterYourGold'), playerGold(), L('BarterMerchantGold'),
        merchantGold())
    local g = panel.drawFrame(canvas, text.escape(info.name(state.merchant)), status)
    panel.drawTabs(canvas, g, { L('BarterBuy'), L('BarterSell') }, state.selling and 2 or 1)
    local rows = buildRows()
    panel.drawList(canvas, g, rows, currentList(), drawEntry,
        L(state.selling and 'BarterNothingToSell' or 'BarterNothingForSale'))
    drawDetails(canvas, g, rows, focusedEntry(rows))
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

-- ---------------------------------------------------------------------------------------------------------------
-- Actions

local function click()
    ambient.playSound('menu click')
end

-- Puts `delta` more of the stack into the deal, or takes them out when it is negative.
local function changeDeal(entry, delta)
    local deal = side()
    local current = deal[entry.key]
    local count = (current and current.count or 0) + delta
    count = math.max(0, math.min(entry.total, count))
    if count == (current and current.count or 0) then
        return
    end
    if count == 0 then
        deal[entry.key] = nil
    else
        deal[entry.key] = { items = entry.items, count = count }
    end
    ambient.playSound(delta > 0 and types.Item.getUpSound(entry.item) or types.Item.getDownSound(entry.item))
    dealChanged()
    redraw()
end

local function haggle(direction)
    -- Right asks for more gold, or offers to pay less. The step grows with the deal, so haggling a big deal doesn't
    -- take long. The offer can't cross zero: the player who buys pays something, and one who sells gets something.
    if state.offer == 0 then
        return
    end
    local step = math.max(1, math.floor(math.abs(state.offer) / 50))
    local balance = state.balance + direction * step
    if state.offer < 0 then
        balance = math.min(balance, -1)
    else
        balance = math.max(balance, 1)
    end
    if balance ~= state.balance then
        state.balance = balance
        redraw()
    end
end

local function showMessage(key, fallback)
    ui.showMessage(gmst(key, fallback))
end

-- A sold item that the player stole from this merchant, if any.
local function stolenFromMerchant()
    for _, entry in pairs(state.deal.sell) do
        local item = entry.items[1]
        for _, owner in ipairs(types.Item.getStolenFrom(item)) do
            if owner.recordId == state.merchant.recordId then
                return item
            end
        end
    end
    return nil
end

local function makeOffer()
    if not hasDeal() then
        showMessage('sBarterDialog11', 'There is nothing to trade.')
        return
    end
    if state.balance < 0 and playerGold() < -state.balance then
        showMessage('sBarterDialog1', "You don't have enough gold.")
        return
    elseif state.balance > 0 and merchantGold() < state.balance then
        showMessage('sBarterDialog2', "The merchant doesn't have enough gold.")
        return
    end
    -- The merchant takes back what the player stole from them, and the conversation ends, as in the engine's window.
    local stolen = stolenFromMerchant()
    if stolen then
        local message = gmst('sNotifyMessage49', '%s was stolen from this merchant.')
        ui.showMessage((message:gsub('%%s', function()
            return info.name(stolen)
        end)))
    end
    state.waiting = true
    state.stolenBack = stolen ~= nil
    core.sendGlobalEvent('ModernMorrowBarter', {
        player = self.object,
        merchant = state.merchant,
        setup = setup(state.balance),
    })
end

local function close()
    I.UI.removeMode('Barter')
end

barter.handler = {
    onButton = function(name)
        if state == nil or state.waiting then
            return
        end
        local entry = focusedEntry()
        if name == 'a' then
            if entry then
                changeDeal(entry, entry.inDeal > 0 and -entry.inDeal or entry.available)
            end
        elseif name == 'x' then
            makeOffer()
        elseif name == 'y' then
            if currentList():nextGroup(buildRows()) then
                click()
                redraw()
            end
        elseif name == 'lb' or name == 'rb' then
            state.selling = not state.selling
            click()
            redraw()
        elseif name == 'rs' then
            state.balance = state.offer
            redraw()
        elseif name == 'b' then
            close()
        end
    end,
    onMove = function(dx, dy)
        if state == nil or state.waiting then
            return
        end
        if dy ~= 0 then
            if currentList():move(buildRows(), dy) then
                redraw()
            end
        elseif dx ~= 0 then
            local entry = focusedEntry()
            if entry then
                changeDeal(entry, dx)
            end
        end
    end,
    onStick = function(x, _, dt)
        if state == nil or state.waiting then
            return
        end
        -- The right stick changes the offer while it is held, faster the further it is pushed.
        if math.abs(x) < HAGGLE_THRESHOLD then
            state.haggleTimer = 0
            return
        end
        state.haggleTimer = state.haggleTimer - dt
        if state.haggleTimer > 0 then
            return
        end
        state.haggleTimer = math.abs(x) > 0.85 and HAGGLE_INTERVAL / 3 or HAGGLE_INTERVAL
        haggle(x > 0 and 1 or -1)
    end,
}

-- ---------------------------------------------------------------------------------------------------------------
-- The screen

local function show(merchant)
    if merchant == nil then
        return
    end
    state = {
        merchant = merchant,
        selling = false,
        buyList = list.new(),
        sellList = list.new(),
        deal = { buy = {}, sell = {} },
        offer = 0,
        balance = 0,
        haggleTimer = 0,
        checkTimer = 0,
        waiting = false,
    }
    router.push(barter.handler)
    if element then
        element:destroy()
    end
    element = ui.create(layout())
end

local function hide()
    router.remove(barter.handler)
    if element then
        element:destroy()
        element = nil
    end
    state = nil
end

-- From the global script, after the merchant answered the offer.
function barter.onResult(accepted)
    if state == nil then
        return
    end
    state.waiting = false
    if state.stolenBack then
        -- The merchant took back the stolen item, and won't trade any more.
        close()
        if types.Player.getDialogueActor(self) ~= nil then
            types.Player.sayGoodbye(self)
        end
        return
    end
    if accepted then
        ambient.playSound('Item Gold Up')
        close()
        return
    end
    showMessage('sNotifyMessage9', 'Your offer was refused.')
    -- Event handlers may run on the Lua worker thread, which can't measure text, so the next frame redraws.
    state.dirty = true
end

-- A string that changes when the goods on either side change, such as when a script adds items.
local function signature()
    local parts = {}
    for _, item in ipairs(types.Actor.inventory(self):getAll()) do
        parts[#parts + 1] = item.id .. ':' .. item.count
    end
    for _, item in ipairs(types.Actor.getBarterItems(state.merchant)) do
        parts[#parts + 1] = item.id .. ':' .. item.count
    end
    return table.concat(parts, ',') .. '|' .. merchantGold()
end

-- Redraws when the goods change.
function barter.onFrame()
    if element == nil or state.waiting then
        return
    end
    if state.dirty then
        state.dirty = false
        redraw()
    end
    local now = core.getRealTime()
    if state.signature ~= nil and now < state.checkTimer then
        return
    end
    state.checkTimer = now + CHECK_INTERVAL
    local current = signature()
    if current ~= state.signature then
        local first = state.signature == nil
        state.signature = current
        if not first then
            -- Drop stacks from the deal that are gone, and count the rest again.
            for _, deal in ipairs({ state.deal.buy, state.deal.sell }) do
                for key, entry in pairs(deal) do
                    local total = 0
                    for _, item in ipairs(entry.items) do
                        if isThere(item) then
                            total = total + item.count
                        end
                    end
                    if total == 0 then
                        deal[key] = nil
                    elseif entry.count > total then
                        entry.count = total
                    end
                end
            end
            dealChanged()
            redraw()
        end
    end
end

function barter.register()
    I.UI.registerWindow(barter.window, function(merchant)
        show(merchant)
    end, hide, { capturesInput = true })
end

return barter
