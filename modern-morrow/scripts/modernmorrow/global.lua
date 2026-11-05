-- The global script of the Modern Morrow mod. It does what player scripts may not, such as moving an item out of
-- the inventory.

local async = require('openmw.async')
local core = require('openmw.core')
local types = require('openmw.types')
local world = require('openmw.world')

-- Drops items from the player's inventory, the way the engine's inventory does it.
local function dropItem(data)
    if data.item:isValid() and data.item.count > 0 then
        types.Player.dropItem(data.player, data.item, data.count)
    end
end

-- Takes the book or scroll the player is reading, with the crime rules: one in a container the way the container
-- window takes items, and one in the world the way the book window's Take does.
local function takeBook(data)
    local player, book = data.player, data.book
    if not book:isValid() or book.count == 0 then
        return
    end
    local container = book.parentContainer
    if container ~= nil then
        if container ~= player then
            types.Player.takeItem(player, container, book)
        end
    elseif book.cell ~= nil then
        types.Player.pickUpItem(player, book)
    end
end

-- Calls an engine function that may throw, such as when an offer became impossible since the player script sent it.
-- Returns whether it worked.
local function try(fn, ...)
    local ok, err = pcall(fn, ...)
    if not ok then
        print('Modern Morrow: ' .. tostring(err))
    end
    return ok
end

-- Makes the player's offer to a merchant, and tells the player script whether the merchant accepted it.
local function barter(data)
    local player = data.player
    local function answer(accepted)
        player:sendEvent('ModernMorrowBarterResult', accepted == true)
    end
    if not try(types.Player.barter, player, data.merchant, data.setup, async:callback(answer)) then
        answer(false)
    end
end

local function inventoryOf(object)
    if types.Actor.objectIsInstance(object) then
        return types.Actor.inventory(object)
    end
    return types.Container.inventory(object)
end

-- Takes an item out of a container or a body, with the crime rules of the engine's container window.
local function take(data)
    if data.item:isValid() and data.item.count > 0 then
        try(types.Player.takeItem, data.player, data.container, data.item)
    end
end

local function takeAll(data)
    for _, item in ipairs(inventoryOf(data.container):getAll()) do
        if item.count > 0 then
            try(types.Player.takeItem, data.player, data.container, item)
        end
    end
end

-- Takes everything from a body, then disposes of the corpse, as the container window's Dispose of Corpse button does.
-- The corpse of a persistent actor, such as a quest character, stays, and the player script says so.
local function disposeCorpse(data)
    local player, actor = data.player, data.actor
    if not actor:isValid() then
        return
    end
    takeAll({ player = player, container = actor })
    local ok = pcall(types.Actor.disposeCorpse, actor)
    if not ok then
        player:sendEvent('ModernMorrowShowMessage', core.getGMST('sDisposeCorpseFail'))
    end
end

-- Puts one of the player's items into a container or a body. Moving an equipped item takes it off first.
local function putIn(data)
    local item = data.item
    if item:isValid() and item.count > 0 and item.parentContainer == data.player then
        item:moveInto(inventoryOf(data.container))
    end
end

-- Tries to steal an item from an NPC's pockets, and tells the player script whether the NPC noticed.
local function pickpocket(data)
    local player = data.player
    local function answer(detected)
        player:sendEvent('ModernMorrowPickpocketResult', detected)
    end
    if not data.item:isValid() or data.item.count == 0
            or not try(types.Player.pickpocket, player, data.victim, data.item, nil, async:callback(answer)) then
        answer(nil)
    end
end

local function finishPickpocketing(data)
    if data.victim:isValid() then
        try(types.Player.finishPickpocketing, data.player, data.victim)
    end
end

-- The profit that a companion who works for pay counts in their script's `minimumprofit` variable, or nil for a
-- companion who doesn't keep count.
local function companionProfit(actor)
    local script = world.mwscript.getLocalScript(actor)
    if script == nil then
        return nil
    end
    local ok, profit = pcall(function()
        return script.variables.minimumprofit
    end)
    if ok then
        return profit
    end
    return nil
end

local function sendProfit(data)
    data.player:sendEvent('ModernMorrowCompanionProfit', companionProfit(data.companion))
end

-- Moves an item between the player and a companion. The profit goes up by the value of what the player gives, and down
-- by the value of what they take, as in the engine's window.
local function companionMove(data)
    local item, companion, player = data.item, data.companion, data.player
    if not item:isValid() or item.count == 0 then
        return
    end
    local profit = companionProfit(companion)
    local value = types.Item.getValue(item) * item.count
    if data.give then
        if item.parentContainer == player then
            item:moveInto(types.Actor.inventory(companion))
        end
    elseif try(types.Player.takeItem, player, companion, item) then
        value = -value
    else
        value = 0
    end
    if profit ~= nil and value ~= 0 then
        world.mwscript.getLocalScript(companion).variables.minimumprofit = profit + value
    end
    sendProfit(data)
end

-- Buys training, a trip, a spell or a repair from a merchant.
local function buyService(data)
    local player, merchant = data.player, data.merchant
    if data.service == 'training' then
        try(types.Player.buyTraining, player, merchant, data.skill)
    elseif data.service == 'travel' then
        try(types.Player.buyTravel, player, merchant, data.destination)
    elseif data.service == 'spell' then
        try(types.Player.buySpell, player, merchant, data.spell)
    elseif data.service == 'repair' then
        try(types.Player.buyRepair, player, merchant, data.item)
    end
end

-- The names of the cells that the journal's entries were added in, by cell ID, as the game shows them: the cell's
-- name, or the region's for an exterior cell without one. Player scripts can't look a cell up by its ID.
local function cellNames(data)
    local names = {}
    for _, id in ipairs(data.ids) do
        local ok, cell = pcall(world.getCellById, id)
        local name = ''
        if ok and cell then
            name = cell.displayName or ''
            if name == '' and cell.region then
                local region = core.regions.records[cell.region]
                name = region and region.name or ''
            end
        end
        names[id] = name
    end
    data.player:sendEvent('ModernMorrowCellNames', names)
end

-- Uses one of the player's repair tools on an item. The engine plays the sound and shows the message.
local function repair(data)
    local item, tool = data.item, data.tool
    if item:isValid() and item.count > 0 and tool:isValid() and tool.count > 0 then
        try(types.Player.repairItem, data.player, item, tool)
    end
end

-- Uses a soul gem to recharge a magic item. The engine plays the sound and shows the message.
local function recharge(data)
    local item, soulGem = data.item, data.soulGem
    if item:isValid() and item.count > 0 and soulGem:isValid() and soulGem.count > 0 then
        try(types.Player.rechargeItem, data.player, item, soulGem)
    end
end

-- Brews potions, and tells the player script how many worked, or -1 when nothing happened.
local function brew(data)
    local player = data.player
    local function answer(count)
        player:sendEvent('ModernMorrowBrewResult', count)
    end
    if not try(types.Player.brewPotion, player, data.setup, async:callback(answer)) then
        answer(-1)
    end
end

-- Enchants an item, and tells the player script whether it worked.
local function enchant(data)
    local player = data.player
    local function answer(enchanted)
        player:sendEvent('ModernMorrowEnchantResult', enchanted == true)
    end
    if not try(types.Player.enchantItem, player, data.setup, async:callback(answer)) then
        answer(false)
    end
end

-- Buys a spell of the player's own.
local function makeSpell(data)
    try(types.Player.makeSpell, data.player, data.setup)
end

local function levelUp(data)
    try(types.Player.levelUp, data.player, data.attributes)
end

-- Rests or waits in the open rest menu, and tells the player script whether the rest lasted all the hours.
local function rest(data)
    local player = data.player
    local function answer(finished)
        player:sendEvent('ModernMorrowRestResult', finished == true)
    end
    if not try(types.Player.rest, player, data.hours, async:callback(answer)) then
        answer(false)
    end
end

return {
    eventHandlers = {
        ModernMorrowRepair = repair,
        ModernMorrowRecharge = recharge,
        ModernMorrowBrew = brew,
        ModernMorrowEnchant = enchant,
        ModernMorrowMakeSpell = makeSpell,
        ModernMorrowLevelUp = levelUp,
        ModernMorrowRest = rest,
        ModernMorrowBuyService = buyService,
        ModernMorrowCellNames = cellNames,
        ModernMorrowCompanionProfit = sendProfit,
        ModernMorrowCompanionMove = companionMove,
        ModernMorrowBarter = barter,
        ModernMorrowTake = take,
        ModernMorrowTakeAll = takeAll,
        ModernMorrowDisposeCorpse = disposeCorpse,
        ModernMorrowPutIn = putIn,
        ModernMorrowPickpocket = pickpocket,
        ModernMorrowFinishPickpocketing = finishPickpocketing,
        ModernMorrowDropItem = dropItem,
        ModernMorrowTakeBook = takeBook,
    },
}
