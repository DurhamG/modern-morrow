-- Replaces the engine's travel window with a Lua window.
-- It lists the merchant's destinations with their price and the hours the trip takes. Clicking a destination asks
-- the global script to make the trip, because only global scripts can buy travel.

local async = require('openmw.async')
local core = require('openmw.core')
local self = require('openmw.self')
local types = require('openmw.types')
local ui = require('openmw.ui')
local util = require('openmw.util')
local I = require('openmw.interfaces')

local function gmstColor(name)
    return util.color.commaString(core.getGMST(name))
end

local normalColor = gmstColor('FontColor_color_normal')
local overColor = gmstColor('FontColor_color_normal_over')
local disabledColor = gmstColor('FontColor_color_disabled')

local nameWidth = 300
local priceWidth = 90
local rowHeight = 24

local window = nil
local merchant = nil
local travelling = false

local function playerGold()
    return types.Actor.inventory(self):countOf('gold_001')
end

local function text(value, props)
    local merged = { text = value }
    for k, v in pairs(props or {}) do
        merged[k] = v
    end
    return { template = I.MWUI.templates.textNormal, props = merged }
end

-- A row for one destination: its name on the left, its price on the right.
local function destinationRow(index, destination, gold)
    local price = types.Actor.getTravelPrice(merchant, index)
    local hours = types.Actor.getTravelHours(merchant, index)
    local affordable = price <= gold
    local color = affordable and normalColor or disabledColor
    local name = destination.cellName
    if hours > 0 then
        name = string.format('%s (%d h)', name, hours)
    end
    local nameText = text(name, { textColor = color })
    local priceText = text(tostring(price) .. ' gold', {
        textColor = color,
        relativePosition = util.vector2(1, 0),
        anchor = util.vector2(1, 0),
    })
    local function setColor(c)
        nameText.props.textColor = c
        priceText.props.textColor = c
        window:update()
    end
    local events = {}
    if affordable then
        events = {
            mouseClick = async:callback(function()
                if travelling then
                    return
                end
                travelling = true
                core.sendGlobalEvent('TravelDemoBuy', { player = self.object, merchant = merchant, destination = index })
            end),
            focusGain = async:callback(function() setColor(overColor) end),
            focusLoss = async:callback(function() setColor(normalColor) end),
        }
    end
    return {
        props = { size = util.vector2(nameWidth + priceWidth, rowHeight) },
        events = events,
        content = ui.content { nameText, priceText },
    }
end

local function show(actor)
    if not actor then
        return
    end
    merchant = actor
    travelling = false
    local gold = playerGold()
    local rows = {
        { template = I.MWUI.templates.textHeader, props = { text = 'Travel' } },
        { template = I.MWUI.templates.interval },
    }
    for index, destination in ipairs(merchant.type.record(merchant).travelDestinations) do
        table.insert(rows, destinationRow(index, destination, gold))
    end
    table.insert(rows, { template = I.MWUI.templates.interval })
    table.insert(rows, text(string.format('Gold: %d', gold)))
    window = ui.create {
        layer = 'Windows',
        template = I.MWUI.templates.boxSolid,
        props = {
            relativePosition = util.vector2(0.5, 0.5),
            anchor = util.vector2(0.5, 0.5),
        },
        content = ui.content {
            {
                template = I.MWUI.templates.padding,
                content = ui.content {
                    { type = ui.TYPE.Flex, content = ui.content(rows) },
                },
            },
        },
    }
end

local function hide()
    if window then
        window:destroy()
        window = nil
    end
    merchant = nil
end

I.UI.registerWindow('Travel', show, hide)

return {
    eventHandlers = {
        -- The trip is done. Close the travel window and the conversation, as the engine's travel window does.
        TravelDemoArrived = function()
            I.UI.setMode()
        end,
    },
}
