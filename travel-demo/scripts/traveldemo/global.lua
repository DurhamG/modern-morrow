local types = require('openmw.types')

return {
    eventHandlers = {
        TravelDemoBuy = function(data)
            types.Player.buyTravel(data.player, data.merchant, data.destination)
            -- The trip happens at the end of this frame. The player script closes the window after that.
            data.player:sendEvent('TravelDemoArrived')
        end,
    },
}
