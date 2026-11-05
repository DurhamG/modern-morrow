local testing = require('testing_util')
local matchers = require('matchers')
local menu = require('openmw.menu')

testing.setSetupGlobalTest(function()
    menu.newGame({bypass = true})
    coroutine.yield()
end)

local function isOfType(expectedType)
    return function(actual)
        if type(actual) ~= expectedType then
            return string.format('expected a %s, got %s', expectedType, tostring(actual))
        end
        return ''
    end
end

local function isNumberBetween(min, max)
    return function(actual)
        if type(actual) ~= 'number' or actual < min or actual > max then
            return string.format('expected a number from %s to %s, got %s', min, max, tostring(actual))
        end
        return ''
    end
end

testing.registerMenuTest('save and load', function()
    menu.newGame()
    coroutine.yield()
    menu.saveGame('save and load')
    coroutine.yield()

    local directorySaves = {}
    directorySaves['save_and_load.omwsave'] = {
        playerName = '',
        playerLevel = 1,
        timePlayed = 0,
        description = 'save and load',
        contentFiles = {
            'builtin.omwscripts',
            'template.omwgame',
            'landracer.omwaddon',
            'the_hub.omwaddon',
            'mwscript.omwaddon',
            'test_lua_api.omwscripts',
        },
        creationTime = matchers.isAny(),
        playerHealth = isNumberBetween(1, math.huge),
        playerMaxHealth = isNumberBetween(1, math.huge),
        cellName = isOfType('string'),
        day = isNumberBetween(0, math.huge),
        dayOfMonth = isNumberBetween(1, 31),
        month = isNumberBetween(1, 12),
        year = isOfType('number'),
        gameHour = isNumberBetween(0, 24),
    }
    local expectedAllSaves = {}
    expectedAllSaves[' - 1'] = directorySaves

    testing.expectThat(menu.getAllSaves(), matchers.equalTo(expectedAllSaves))

    menu.loadGame(' - 1', 'save_and_load.omwsave')
    coroutine.yield()

    menu.deleteGame(' - 1', 'save_and_load.omwsave')
    testing.expectThat(menu.getAllSaves(), matchers.equalTo({}))
end)

testing.registerMenuTest('load while teleporting', function()
    menu.newGame()
    coroutine.yield()

    testing.runGlobalTest('load while teleporting - init player')

    menu.saveGame('load while teleporting')
    coroutine.yield()

    testing.runGlobalTest('load while teleporting - teleport')

    menu.loadGame(' - 1', 'load_while_teleporting.omwsave')
    coroutine.yield()

    menu.deleteGame(' - 1', 'load_while_teleporting.omwsave')
end)

return {
    engineHandlers = {
        onFrame = testing.makeUpdateMenu(),
    },
    eventHandlers = testing.menuEventHandlers,
}
