local testing = require('testing_util')
local async = require('openmw.async')
local self = require('openmw.self')
local util = require('openmw.util')
local core = require('openmw.core')
local input = require('openmw.input')
local types = require('openmw.types')
local nearby = require('openmw.nearby')
local camera = require('openmw.camera')
local matchers = require('matchers')
local I = require('openmw.interfaces')
local ui = require('openmw.ui')

types.Player.setControlSwitch(self, types.Player.CONTROL_SWITCH.Controls, false)
types.Player.setControlSwitch(self, types.Player.CONTROL_SWITCH.Fighting, false)
types.Player.setControlSwitch(self, types.Player.CONTROL_SWITCH.Jumping, false)
types.Player.setControlSwitch(self, types.Player.CONTROL_SWITCH.Looking, false)
types.Player.setControlSwitch(self, types.Player.CONTROL_SWITCH.Magic, false)
types.Player.setControlSwitch(self, types.Player.CONTROL_SWITCH.VanityMode, false)
types.Player.setControlSwitch(self, types.Player.CONTROL_SWITCH.ViewMode, false)

local function rotate(object, targetPitch, targetYaw)
    local endTime = core.getSimulationTime() + 1
    while core.getSimulationTime() < endTime do
        object.controls.jump = false
        object.controls.run = true
        object.controls.movement = 0
        object.controls.sideMovement = 0
        if targetPitch ~= nil then
            object.controls.pitchChange = util.normalizeAngle(targetPitch - object.rotation:getPitch()) * 0.5
        end
        if targetYaw ~= nil then
            object.controls.yawChange = util.normalizeAngle(targetYaw - object.rotation:getYaw()) * 0.5
        end
        coroutine.yield()
    end
end

local function rotateByYaw(object, target)
    rotate(object, nil, target)
end

local function rotateByPitch(object, target)
    rotate(object, target, nil)
end

testing.registerLocalTest('rotating player with controls.yawChange should change rotation',
    function()
        local initialAlphaXZ, initialGammaXZ = self.rotation:getAnglesXZ()
        local initialAlphaZYX, initialBetaZYX, initialGammaZYX = self.rotation:getAnglesZYX()

        local targetYaw = math.rad(90)
        rotateByYaw(self, targetYaw)

        testing.expectEqualWithDelta(self.rotation:getYaw(), targetYaw, 0.05, 'Incorrect yaw rotation')

        local alpha1, gamma1 = self.rotation:getAnglesXZ()
        testing.expectEqualWithDelta(alpha1, initialAlphaXZ, 0.05, 'Alpha rotation in XZ convention should not change')
        testing.expectEqualWithDelta(gamma1, targetYaw, 0.05, 'Incorrect gamma rotation in XZ convention')

        local alpha2, beta2, gamma2 = self.rotation:getAnglesZYX()
        testing.expectEqualWithDelta(alpha2, targetYaw, 0.05, 'Incorrect alpha rotation in ZYX convention')
        testing.expectEqualWithDelta(beta2, initialBetaZYX, 0.05, 'Beta rotation in ZYX convention should not change')
        testing.expectEqualWithDelta(gamma2, initialGammaZYX, 0.05, 'Gamma rotation in ZYX convention should not change')
    end)

testing.registerLocalTest('rotating player with controls.pitchChange should change rotation',
    function()
        local initialAlphaXZ, initialGammaXZ = self.rotation:getAnglesXZ()
        local initialAlphaZYX, initialBetaZYX, initialGammaZYX = self.rotation:getAnglesZYX()

        local targetPitch = math.rad(90)
        rotateByPitch(self, targetPitch)

        testing.expectEqualWithDelta(self.rotation:getPitch(), targetPitch, 0.05, 'Incorrect pitch rotation')

        local alpha1, gamma1 = self.rotation:getAnglesXZ()
        testing.expectEqualWithDelta(alpha1, targetPitch, 0.05, 'Incorrect alpha rotation in XZ convention')
        testing.expectEqualWithDelta(gamma1, initialGammaXZ, 0.05, 'Gamma rotation in XZ convention should not change')

        local alpha2, beta2, gamma2 = self.rotation:getAnglesZYX()
        testing.expectEqualWithDelta(alpha2, initialAlphaZYX, 0.05, 'Alpha rotation in ZYX convention should not change')
        testing.expectEqualWithDelta(beta2, initialBetaZYX, 0.05, 'Beta rotation in ZYX convention should not change')
        testing.expectEqualWithDelta(gamma2, targetPitch, 0.05, 'Incorrect gamma rotation in ZYX convention')
    end)

testing.registerLocalTest('rotating player with controls.pitchChange and controls.yawChange should change rotation',
    function()
        local targetPitch = math.rad(-30)
        local targetYaw = math.rad(-60)
        rotate(self, targetPitch, targetYaw)

        testing.expectEqualWithDelta(self.rotation:getPitch(), targetPitch, 0.05, 'Incorrect pitch rotation')
        testing.expectEqualWithDelta(self.rotation:getYaw(), targetYaw, 0.05, 'Incorrect yaw rotation')

        local alpha1, gamma1 = self.rotation:getAnglesXZ()
        testing.expectEqualWithDelta(alpha1, targetPitch, 0.05, 'Incorrect alpha rotation in XZ convention')
        testing.expectEqualWithDelta(gamma1, targetYaw, 0.05, 'Incorrect gamma rotation in XZ convention')

        local alpha2, beta2, gamma2 = self.rotation:getAnglesZYX()
        testing.expectEqualWithDelta(alpha2, math.rad(-56), 0.05, 'Incorrect alpha rotation in ZYX convention')
        testing.expectEqualWithDelta(beta2, math.rad(-25), 0.05, 'Incorrect beta rotation in ZYX convention')
        testing.expectEqualWithDelta(gamma2, math.rad(-16), 0.05, 'Incorrect gamma rotation in ZYX convention')
    end)

testing.registerLocalTest('rotating player should not lead to nan rotation',
    function()
        local rotation = math.sqrt(2)
        local endTime = core.getSimulationTime() + 3
        while core.getSimulationTime() < endTime do
            self.controls.jump = false
            self.controls.run = true
            self.controls.movement = 0
            self.controls.sideMovement = 0
            self.controls.pitchChange = rotation
            self.controls.yawChange = rotation
            coroutine.yield()

            local alpha1, gamma1 = self.rotation:getAnglesXZ()
            testing.expectThat(alpha1, matchers.isNotNan(), 'Alpha rotation in XZ convention is nan')
            testing.expectThat(gamma1, matchers.isNotNan(), 'Gamma rotation in XZ convention is nan')

            local alpha2, beta2, gamma2 = self.rotation:getAnglesZYX()
            testing.expectThat(alpha2, matchers.isNotNan(), 'Alpha rotation in ZYX convention is nan')
            testing.expectThat(beta2, matchers.isNotNan(), 'Beta rotation in ZYX convention is nan')
            testing.expectThat(gamma2, matchers.isNotNan(), 'Gamma rotation in ZYX convention is nan')
        end
    end)

testing.registerLocalTest('player forward running',
    function()
        local startPos = self.position
        local endTime = core.getSimulationTime() + 1
        while core.getSimulationTime() < endTime do
            self.controls.jump = false
            self.controls.run = true
            self.controls.movement = 1
            self.controls.sideMovement = 0
            self.controls.yawChange = 0
            coroutine.yield()
        end
        local direction, distance = (self.position - startPos):normalize()
        testing.expectGreaterThan(distance, 0, 'Run forward, distance')
        testing.expectEqualWithDelta(direction.x, 0, 0.1, 'Run forward, X coord')
        testing.expectEqualWithDelta(direction.y, 1, 0.1, 'Run forward, Y coord')
    end)

testing.registerLocalTest('player diagonal walking',
    function()
        local startPos = self.position
        local endTime = core.getSimulationTime() + 1
        while core.getSimulationTime() < endTime do
            self.controls.jump = false
            self.controls.run = false
            self.controls.movement = -1
            self.controls.sideMovement = -1
            self.controls.yawChange = 0
            coroutine.yield()
        end
        local direction, distance = (self.position - startPos):normalize()
        testing.expectGreaterThan(distance, 0, 'Walk diagonally, distance')
        testing.expectEqualWithDelta(direction.x, -0.707, 0.1, 'Walk diagonally, X coord')
        testing.expectEqualWithDelta(direction.y, -0.707, 0.1, 'Walk diagonally, Y coord')
    end)

testing.registerLocalTest('Lua hides a HUD element', function()
    local minimap = I.UI.HUD_ELEMENT.Minimap
    local weapon = I.UI.HUD_ELEMENT.Weapon
    testing.expectEqual(I.UI.isHudElementVisible(minimap), true, 'The minimap should be visible at first')

    I.UI.setHudElementVisibility(minimap, false)
    coroutine.yield()
    testing.expectEqual(I.UI.isHudElementVisible(minimap), false, 'Lua should hide the minimap')
    testing.expectEqual(I.UI.isHudElementVisible(weapon), true, 'The other elements should stay visible')

    -- Opening and closing a menu makes the engine show the minimap again, unless Lua hid it.
    I.UI.setPauseOnMode(I.UI.MODE.Interface, false)
    I.UI.setMode(I.UI.MODE.Interface, { windows = {} })
    while I.UI.getMode() ~= I.UI.MODE.Interface do
        coroutine.yield()
    end
    I.UI.setMode()
    while I.UI.getMode() ~= nil do
        coroutine.yield()
    end
    I.UI.setPauseOnMode(I.UI.MODE.Interface, true)
    testing.expectEqual(I.UI.isHudElementVisible(minimap), false, 'The engine should not show the minimap again')

    I.UI.setHudElementVisibility(minimap, true)
    coroutine.yield()
    testing.expectEqual(I.UI.isHudElementVisible(minimap), true, 'The engine should show the minimap again')
end)

testing.registerLocalTest('Lua window replacement sees the requested windows', function()
    local shown = nil
    I.UI.registerWindow('Map', function(_, windows)
        shown = { windows = windows }
    end, function() end)
    I.UI.setPauseOnMode(I.UI.MODE.Interface, false)

    I.UI.setMode(I.UI.MODE.Interface, { windows = { 'Map' } })
    while shown == nil do
        coroutine.yield()
    end
    testing.expectEqual(shown.windows and #shown.windows, 1, 'The replacement should get the requested windows')
    testing.expectEqual(shown.windows and shown.windows[1], 'Map', 'The replacement should get the map window')

    I.UI.setMode()
    while I.UI.getMode() ~= nil do
        coroutine.yield()
    end
    shown = nil
    I.UI.setMode(I.UI.MODE.Interface)
    while shown == nil do
        coroutine.yield()
    end
    testing.expectEqual(shown.windows, nil, 'A mode opened without a list of windows should give nil')

    I.UI.setMode()
    while I.UI.getMode() ~= nil do
        coroutine.yield()
    end
    I.UI.setPauseOnMode(I.UI.MODE.Interface, true)
end)

testing.registerLocalTest('GUI input capture applies only in menus', function()
    input.setGuiInputCaptured(true)
    testing.expectEqual(input.isGuiInputCaptured(), false, 'The capture should have no effect outside menus')

    I.UI.setPauseOnMode(I.UI.MODE.Interface, false)
    I.UI.setMode(I.UI.MODE.Interface, { windows = {} })
    while I.UI.getMode() ~= I.UI.MODE.Interface do
        coroutine.yield()
    end
    testing.expectEqual(input.isGuiInputCaptured(), true, 'The capture should apply while a menu is open')

    input.setGuiInputCaptured(false)
    testing.expectEqual(input.isGuiInputCaptured(), false, 'Clearing the capture should end it at once')

    I.UI.setMode()
    while I.UI.getMode() ~= nil do
        coroutine.yield()
    end
    I.UI.setPauseOnMode(I.UI.MODE.Interface, true)
end)

testing.registerLocalTest('Lua replaces the main menu', function()
    local shown = false
    I.UI.registerWindow(I.UI.WINDOW.MainMenu, function() shown = true end, function() shown = false end,
        { capturesInput = true })
    I.UI.setPauseOnMode(I.UI.MODE.MainMenu, false)
    I.UI.setMode(I.UI.MODE.MainMenu)
    while I.UI.getMode() ~= I.UI.MODE.MainMenu do
        coroutine.yield()
    end
    testing.expectEqual(shown, true, 'The replacement should be shown with the main menu')
    testing.expectEqual(ui._isWindowVisible(I.UI.WINDOW.MainMenu), false, "The engine's main menu should stay hidden")
    testing.expectEqual(input.isGuiInputCaptured(), false,
        'The main menu should not capture the input, because player scripts get no input events then')

    I.UI.setMode()
    while I.UI.getMode() ~= nil do
        coroutine.yield()
    end
    I.UI.setPauseOnMode(I.UI.MODE.MainMenu, true)
    testing.expectEqual(shown, false, 'The replacement should be hidden when the main menu closes')
end)

testing.registerLocalTest('Lua replaces a character creation window', function()
    testing.expectThat(I.UI.getWindowsForMode(I.UI.MODE.ChargenRace), matchers.equalTo({ ChargenRace = 'ChargenRace' }))
    testing.expectThat(I.UI.getWindowsForMode(I.UI.MODE.ChargenClassReview),
        matchers.equalTo({ ChargenReview = 'ChargenReview' }))

    I.UI.setPauseOnMode(I.UI.MODE.ChargenBirth, false)
    I.UI.setMode(I.UI.MODE.ChargenBirth)
    while I.UI.getMode() ~= I.UI.MODE.ChargenBirth do
        coroutine.yield()
    end
    testing.expectEqual(ui._isWindowVisible(I.UI.WINDOW.ChargenBirth), true, "The engine's window should be shown")
    I.UI.setMode()
    while I.UI.getMode() ~= nil do
        coroutine.yield()
    end
    I.UI.setPauseOnMode(I.UI.MODE.ChargenBirth, true)
    testing.expectEqual(ui._isWindowVisible(I.UI.WINDOW.ChargenBirth), false, "The engine's window should be hidden")

    local shown = false
    I.UI.registerWindow(I.UI.WINDOW.ChargenName, function() shown = true end, function() shown = false end)
    I.UI.setPauseOnMode(I.UI.MODE.ChargenName, false)
    I.UI.setMode(I.UI.MODE.ChargenName)
    while I.UI.getMode() ~= I.UI.MODE.ChargenName do
        coroutine.yield()
    end
    testing.expectEqual(shown, true, 'The replacement should be shown with its mode')
    testing.expectEqual(ui._isWindowVisible(I.UI.WINDOW.ChargenName), false, "The engine's window should stay hidden")

    I.UI.setMode()
    while I.UI.getMode() ~= nil do
        coroutine.yield()
    end
    I.UI.setPauseOnMode(I.UI.MODE.ChargenName, true)
    testing.expectEqual(shown, false, 'The replacement should be hidden when its mode closes')
end)

testing.registerLocalTest('Lua answers an interactive message', function()
    local asked = nil
    local answered = false
    I.UI.registerInteractiveMessageHandler(function(message, buttons, answer)
        asked = { message = message, buttons = buttons }
        -- The test itself doesn't run while the game is paused, so a real time timer answers.
        async:newUnsavableRealTimeTimer(0.1, function()
            asked.paused = core.isWorldPaused()
            asked.inputCaptured = input.isGuiInputCaptured()
            answer(1)
            answered = true
        end)
    end)
    I.UI.showInteractiveMessage('Test message')
    while not answered do
        coroutine.yield()
    end
    I.UI.registerInteractiveMessageHandler(nil)

    testing.expectEqual(asked.message, 'Test message')
    testing.expectThat(asked.buttons, matchers.equalTo({ 'OK' }))
    testing.expectEqual(asked.paused, true, 'The game should be paused while the message waits for an answer')
    testing.expectEqual(asked.inputCaptured, true, 'The menu input should be captured until the answer')
    testing.expectEqual(input.isGuiInputCaptured(), false, 'The answer should release the menu input')
end)

testing.registerLocalTest('Lua shows plain messages', function()
    local shown = {}
    I.UI.registerMessageHandler(function(message)
        shown[#shown + 1] = message
    end)
    -- The handler takes over in the next frame.
    coroutine.yield()
    ui.showMessage('Test plain message')
    for _ = 1, 10 do
        coroutine.yield()
        if #shown > 0 then
            break
        end
    end
    I.UI.registerMessageHandler(nil)

    testing.expectThat(shown, matchers.equalTo({ 'Test plain message' }), 'The handler should get the message')
end)

testing.registerLocalTest('Interactive message with buttons and a callback', function()
    local askedButtons = nil
    local chosen = nil
    I.UI.registerInteractiveMessageHandler(function(message, buttons, answer)
        askedButtons = buttons
        answer(2)
    end)
    I.UI.showInteractiveMessage('Test question', {
        buttons = { 'First', 'Second' },
        callback = function(button) chosen = button end,
    })
    while chosen == nil do
        coroutine.yield()
    end
    I.UI.registerInteractiveMessageHandler(nil)

    testing.expectThat(askedButtons, matchers.equalTo({ 'First', 'Second' }))
    testing.expectEqual(chosen, 2, 'The callback should get the button that the handler chose')
end)

testing.registerLocalTest('UI layer with a fixed height', function()
    ui.layers.insertAfter('HUD', 'TestFixedHeightLayer', { height = 720 })
    coroutine.yield()
    local layer = ui.layers[ui.layers.indexOf('TestFixedHeightLayer')]
    testing.expectEqual(layer.size.y, 720, 'The layer should have the height it was given')
    local screen = ui.screenSize()
    testing.expectEqualWithDelta(layer.size.x, 720 * screen.x / screen.y, 1,
        "The layer's width should follow the screen's aspect ratio")
    testing.expectEqual(pcall(ui.layers.insertAfter, 'HUD', 'TestInvalidHeightLayer', { height = 0 }), false,
        'A height that is not positive should be an error')
end)

testing.registerLocalTest('screen fades', function()
    ui.fadeScreenOut(0)
    ui.fadeScreenTo(50, 0)
    ui.fadeScreenIn(0)
    testing.expectEqual(pcall(ui.fadeScreenOut, -1), false, 'A negative duration should be an error')
    testing.expectEqual(pcall(ui.fadeScreenIn, 1, -1), false, 'A negative delay should be an error')
    testing.expectEqual(pcall(ui.fadeScreenTo, 101, 1), false, 'More than 100 percent should be an error')
end)

testing.registerLocalTest('measure text', function()
    local short = ui.measureText('Hi', { textSize = 16 })
    local long = ui.measureText('Hello there, outlander', { textSize = 16 })
    testing.expectGreaterThan(short.x, 0, 'Text should have a width')
    testing.expectGreaterThan(long.x, short.x, 'Longer text should be wider')
    testing.expectEqual(long.y, short.y, 'One line of text should have the same height')
    testing.expectGreaterThan(ui.measureText('Hi', { textSize = 32 }).x, short.x, 'Bigger text should be wider')

    local wrapped = ui.measureText('Hello there, outlander', { textSize = 16, wordWrap = true, width = long.x / 2 })
    testing.expectGreaterThan(wrapped.y, long.y, 'Wrapped text should take more lines')
    testing.expectLessOrEqual(wrapped.x, long.x / 2, 'Wrapped text should fit the width')

    local ok = pcall(ui.measureText, 'Hi', { wordWrap = true })
    testing.expectEqual(ok, false, 'Wrapping without a width should be an error')

    testing.expectGreaterThan(ui.measureText('Hi', { textSize = 16, font = 'ScrollFont' }).x, 0,
        'Text in another font should have a width')
    testing.expectEqual(pcall(ui.measureText, 'Hi', { font = 'NoSuchFont' }), false,
        'An unknown font should be an error')
end)

testing.registerLocalTest('Dialogue outside a conversation', function()
    testing.expectEqual(types.Player.getDialogueActor(self), nil, 'The player should not be talking to anyone')
    testing.expectThat(types.Player.getDialogueTopics(self), matchers.equalTo({}), 'There should be no topics')
    testing.expectThat(types.Player.getDialogueTranscript(self), matchers.equalTo({}), 'There should be no lines')
    testing.expectThat(types.Player.getDialogueChoices(self), matchers.equalTo({}), 'There should be no choices')
    testing.expectEqual(types.Player.mustSayGoodbye(self), false, 'Nothing should ask the player to leave')
    testing.expectEqual(pcall(types.Player.askDialogueTopic, self, 'background'), false,
        'The player should not be able to ask about a topic')
    testing.expectEqual(pcall(types.Player.answerDialogueChoice, self, 1), false,
        'The player should not be able to answer a question')
    testing.expectEqual(pcall(types.Player.sayGoodbye, self), false, 'The player should not be able to say goodbye')
    testing.expectEqual(types.Player.canPersuadeDialogueActor(self, 'admire'), false,
        'The player should not be able to persuade anyone')
    testing.expectEqual(pcall(types.Player.persuadeDialogueActor, self, 'admire'), false,
        'The player should not be able to persuade anyone')
    testing.expectEqual(pcall(types.Player.canPersuadeDialogueActor, self, 'flatter'), false,
        'An unknown way to persuade should be an error')
    testing.expectEqual(pcall(types.Player.requestDialogueService, self, 'barter'), false,
        'The player should not be able to ask for a service')

    local shown = types.Player.findTopicLinks(self, 'Ask about @Balmora# in Balmora.')
    testing.expectEqual(shown.text, 'Ask about Balmora in Balmora.', 'The link marks should be removed')
    testing.expectThat(shown.links, matchers.equalTo({}), 'No topic should be available to link to')
end)

testing.registerLocalTest('findPath',
    function()
        local src = util.vector3(4096, 4096, 1745)
        local dst = util.vector3(4500, 4500, 1745.95263671875)
        local options = {
            agentBounds = types.Actor.getPathfindingAgentBounds(self),
            includeFlags = nearby.NAVIGATOR_FLAGS.Walk + nearby.NAVIGATOR_FLAGS.Swim,
            areaCosts = {
                water = 1,
                door = 2,
                ground = 1,
                pathgrid = 1,
            },
            destinationTolerance = 1,
        }
        local status, path = nearby.findPath(src, dst, options)
        testing.expectEqual(status, nearby.FIND_PATH_STATUS.Success, 'Status')
        testing.expectLessOrEqual((path[#path] - dst):length(), 1,
            'Last path point '  .. testing.formatActualExpected(path[#path], dst))
        testing.expectThat(path, matchers.equalTo({
            matchers.closeToVector(util.vector3(4096, 4096, 1746.27099609375), 1e-1),
            matchers.closeToVector(util.vector3(4500, 4500, 1745.95263671875), 1e-1),
        }))
    end)

testing.registerLocalTest('findPath with checkpoints',
    function()
        local src = util.vector3(4096, 4096, 1745)
        local dst = util.vector3(4500, 4500, 1745.95263671875)
        local options = {
            agentBounds = types.Actor.getPathfindingAgentBounds(self),
            includeFlags = nearby.NAVIGATOR_FLAGS.Walk + nearby.NAVIGATOR_FLAGS.Swim,
            areaCosts = {
                water = 1,
                door = 2,
                ground = 1,
                pathgrid = 1,
            },
            destinationTolerance = 1,
            checkpoints = {
                util.vector3(4200, 4100, 1750),
            },
        }
        local status, path = nearby.findPath(src, dst, options)
        testing.expectEqual(status, nearby.FIND_PATH_STATUS.Success, 'Status')
        testing.expectLessOrEqual((path[#path] - dst):length(), 1,
            'Last path point '  .. testing.formatActualExpected(path[#path], dst))
        testing.expectThat(path, matchers.equalTo({
            matchers.closeToVector(util.vector3(4096, 4096, 1746.27099609375), 1e-1),
            matchers.closeToVector(util.vector3(4200, 4100, 1749.5076904296875), 1e-1),
            matchers.closeToVector(util.vector3(4500, 4500, 1745.95263671875), 1e-1),
        }))
    end)

testing.registerLocalTest('findRandomPointAroundCircle',
    function()
        local position = util.vector3(4096, 4096, 1745.95263671875)
        local maxRadius = 100
        local options = {
            agentBounds = types.Actor.getPathfindingAgentBounds(self),
            includeFlags = nearby.NAVIGATOR_FLAGS.Walk,
        }
        local result = nearby.findRandomPointAroundCircle(position, maxRadius, options)
        testing.expectGreaterThan((result - position):length(), 1,
            'Random point ' .. testing.formatActualExpected(result, position))
    end)

testing.registerLocalTest('castNavigationRay',
    function()
        local src = util.vector3(4096, 4096, 1745)
        local dst = util.vector3(4500, 4500, 1745.95263671875)
        local options = {
            agentBounds = types.Actor.getPathfindingAgentBounds(self),
            includeFlags = nearby.NAVIGATOR_FLAGS.Walk + nearby.NAVIGATOR_FLAGS.Swim,
        }
        local result = nearby.castNavigationRay(src, dst, options)
        testing.expectLessOrEqual((result - dst):length(), 1,
            'Navigation hit point ' .. testing.formatActualExpected(result, dst))
    end)

testing.registerLocalTest('findNearestNavMeshPosition',
    function()
        local position = util.vector3(4096, 4096, 2000)
        local options = {
            agentBounds = types.Actor.getPathfindingAgentBounds(self),
            includeFlags = nearby.NAVIGATOR_FLAGS.Walk + nearby.NAVIGATOR_FLAGS.Swim,
            searchAreaHalfExtents = util.vector3(1000, 1000, 1000),
        }
        local result = nearby.findNearestNavMeshPosition(position, options)
        local expected = util.vector3(4096, 4096, 1746.27099609375)
        testing.expectLessOrEqual((result - expected):length(), 1,
            'Navigation mesh position ' .. testing.formatActualExpected(result, expected))
    end)

testing.registerLocalTestStep('camera.getFocusRay should report the object under the crosshair',
    function()
        local target = nil
        for _, actor in ipairs(nearby.actors) do
            if actor.id ~= self.id and (actor.position - self.position):length() < 200 then
                target = actor
            end
        end
        testing.expect(target, 'An NPC should stand ahead of the player')

        local ray = camera.getFocusRay()
        testing.expect(ray.hit, 'Focus ray should hit the NPC ahead')
        testing.expect(ray.hitObject, 'Focus ray should report the NPC it hits')
        testing.expectEqual(ray.hitObject.id, target.id, 'Focus ray should hit the NPC ahead')
        testing.expectLessOrEqual((ray.hitPos - target.position):length(), 200,
            'Focus ray hit position ' .. testing.formatActualExpected(ray.hitPos, target.position))
        testing.expectGreaterThan((ray.hitPos - self.position):length(), 50,
            'Focus ray hit position ' .. testing.formatActualExpected(ray.hitPos, self.position))
    end)

testing.registerLocalTest('player memory limit',
    function()
        local ok, err = pcall(function()
            local str = 'a'
            while true do
                str = str .. str
            end
        end)
        testing.expectEqual(ok, false, 'Script reaching memory limit should fail')
        testing.expectEqual(err, 'not enough memory')
    end)

testing.registerLocalTestStep('player approaches and attacks a creature',
    function()
        camera.setMode(camera.MODE.ThirdPerson)

        local options = {
            agentBounds = types.Actor.getPathfindingAgentBounds(self),
        }
        local duration = 10
        local endTime = core.getSimulationTime() + duration
        local nextTime = 0
        local use = self.ATTACK_TYPE.NoAttack

        local attributes = {}
        for k, v in pairs(types.Actor.stats.attributes) do
            attributes[k] = v(self).base
        end

        types.Actor.stats.attributes.speed(self).base = 100
        types.Actor.stats.attributes.strength(self).base = 1000
        types.Actor.stats.attributes.agility(self).base = 1000

        local weaponId = 'basic_dagger1h'
        local weapon = types.Actor.inventory(self):find(weaponId)

        local isWeapon = function(actual)
            if actual == nil then
                return weaponId .. ' is not found'
            end
            if actual.recordId ~= weaponId then
                return 'found weapon recordId does not match expected: actual=' .. tostring(actual.id)
                       .. ', expected=' .. weaponId
            end
            return ''
        end
        testing.expectThat(weapon, isWeapon)

        types.Actor.setEquipment(self, {[types.Actor.EQUIPMENT_SLOT.CarriedRight] = weapon})

        coroutine.yield()

        testing.expectThat(types.Actor.getEquipment(self, types.Actor.EQUIPMENT_SLOT.CarriedRight), isWeapon)

        types.Actor.setStance(self, types.Actor.STANCE.Weapon)

        local previousHealth = nil
        local targetActor = nil

        while true do
            local time = core.getSimulationTime()
            testing.expectLessOrEqual(time, endTime, 'Did not damage any targets in ' .. duration .. ' seconds')

            if targetActor ~= nil and types.Actor.stats.dynamic.health(targetActor).current < previousHealth then
                print('Dealt ' .. (previousHealth - types.Actor.stats.dynamic.health(targetActor).current) .. ' damage to ' .. tostring(targetActor))
                local enemy, secondsLeft = I.UI.getHudEnemy()
                testing.expectEqual(enemy, targetActor, 'The enemy health bar should show the creature that the player hit')
                testing.expectGreaterThan(secondsLeft, 0, 'The engine should show the bar for a while')
                break
            end

            local minDistance = nil
            for i, actor in ipairs(nearby.actors) do
                if actor.id ~= self.id then
                    local distance = (actor.position - self.position):length()
                    if minDistance == nil or minDistance > distance then
                        minDistance = distance
                        targetActor = actor
                    end
                end
            end
            testing.expectNotEqual(targetActor, nil, 'No attack targets found')

            previousHealth = types.Actor.stats.dynamic.health(targetActor).current

            local destination = nil
            if minDistance > 100 then
                local status, path = nearby.findPath(self.position, targetActor.position, options)

                testing.expectEqual(status, nearby.FIND_PATH_STATUS.Success,
                    'Failed to find path from ' .. tostring(self.position) .. ' to ' .. tostring(targetActor.position))

                destination = path[2]
                use = self.ATTACK_TYPE.NoAttack

                self.controls.run = true
                self.controls.movement = 1
            else
                local halfExtents = types.Actor.getPathfindingAgentBounds(targetActor).halfExtents
                destination = targetActor.position - util.vector3(0, 0, halfExtents.z)

                if nextTime < time then
                    if use == 0 then
                        use = self.ATTACK_TYPE.Any
                        nextTime = time + 0.5
                    else
                        use = self.ATTACK_TYPE.NoAttack
                    end
                end
            end
            self.controls.use = use

            local direction = destination - self.position
            direction = direction:normalize()
            self.controls.yawChange = util.normalizeAngle(math.atan2(direction.x, direction.y) - self.rotation:getYaw())
            self.controls.pitchChange = util.normalizeAngle(math.asin(util.clamp(-direction.z, -1, 1)) - self.rotation:getPitch())

            coroutine.yield()
        end

        for k, v in pairs(types.Actor.stats.attributes) do
            v(self).base = attributes[k]
        end
    end)

testing.registerLocalTestStep('equipped weapon damages a stationary target',
    function(targetActor)
        camera.setMode(camera.MODE.ThirdPerson)
        self.controls.movement = 0
        self.controls.sideMovement = 0
        self.controls.use = self.ATTACK_TYPE.NoAttack

        local attributes = types.Actor.stats.attributes
        local strength = attributes.strength(self).base
        local agility = attributes.agility(self).base
        attributes.strength(self).base = 1000
        attributes.agility(self).base = 1000

        local weaponId = 'basic_dagger1h'
        local setupDeadline = core.getRealTime() + 10
        local weapon
        while weapon == nil do
            testing.expectLessOrEqual(core.getRealTime(), setupDeadline, 'Attack setup timed out')
            weapon = types.Actor.inventory(self):find(weaponId)
            coroutine.yield()
        end

        types.Actor.setEquipment(self, {[types.Actor.EQUIPMENT_SLOT.CarriedRight] = weapon})
        types.Actor.setStance(self, types.Actor.STANCE.Weapon)

        while not targetActor:isValid()
            or not types.Actor.isOnGround(self)
            or not types.Actor.isOnGround(targetActor)
            or types.Actor.getEquipment(self, types.Actor.EQUIPMENT_SLOT.CarriedRight) ~= weapon
            or types.Actor.getStance(self) ~= types.Actor.STANCE.Weapon do
            testing.expectLessOrEqual(core.getRealTime(), setupDeadline, 'Attack setup timed out')
            coroutine.yield()
        end

        local previousHealth = types.Actor.stats.dynamic.health(targetActor).current
        local endTime = core.getSimulationTime() + 10
        local nextTime = 0
        local use = self.ATTACK_TYPE.NoAttack
        while types.Actor.stats.dynamic.health(targetActor).current >= previousHealth do
            local time = core.getSimulationTime()
            testing.expectLessOrEqual(time, endTime, 'Attack did not damage the target')
            testing.expectLessOrEqual((targetActor.position - self.position):length(), 100, 'Target left melee reach')

            if nextTime < time then
                if use == self.ATTACK_TYPE.NoAttack then
                    use = self.ATTACK_TYPE.Any
                    nextTime = time + 0.5
                else
                    use = self.ATTACK_TYPE.NoAttack
                end
            end
            self.controls.use = use

            local halfExtents = types.Actor.getPathfindingAgentBounds(targetActor).halfExtents
            local destination = targetActor.position - util.vector3(0, 0, halfExtents.z)
            local direction = destination - self.position
            direction = direction:normalize()
            self.controls.yawChange = util.normalizeAngle(math.atan2(direction.x, direction.y) - self.rotation:getYaw())
            self.controls.pitchChange = util.normalizeAngle(math.asin(util.clamp(-direction.z, -1, 1)) - self.rotation:getPitch())

            coroutine.yield()
        end

        self.controls.use = self.ATTACK_TYPE.NoAttack
        attributes.strength(self).base = strength
        attributes.agility(self).base = agility
    end)

testing.registerLocalTestStep('spell success chance on self', function(ids)
    local spells = types.Actor.spells(self)
    local magicka = types.Actor.stats.dynamic.magicka(self)

    magicka.current = 0
    coroutine.yield()
    testing.expectEqual(spells:getSuccessChance(ids.spell), 0, 'A spell should fail without enough magicka')

    magicka.current = 100
    coroutine.yield()
    local chance = spells:getSuccessChance(ids.spell)
    testing.expect(chance >= 0 and chance <= 100, 'The chance should be between 0 and 100, got ' .. chance)

    testing.expectEqual(spells:getSuccessChance(ids.power), 100, 'An unused power should always succeed')
end)

testing.registerLocalTestStep('enchantment cast cost on self', function(enchantmentId)
    local enchant = types.NPC.stats.skills.enchant(self)
    enchant.base = 10
    coroutine.yield()
    testing.expectEqual(core.magic.enchantments.getEffectiveCastCost(enchantmentId, self), 50,
        'At Enchant 10 the cast cost should be the base cost')
    enchant.base = 60
    coroutine.yield()
    testing.expectEqual(core.magic.enchantments.getEffectiveCastCost(enchantmentId, self), 25,
        'Each point of Enchant above 10 should lower the cost by one percent')
end)

testing.registerLocalTestStep('training checks on self', function(data)
    local trainer = data.trainer
    local skillId = data.skill
    local trainerSkill = types.NPC.stats.skills[skillId](trainer).modified
    testing.expectGreaterThan(trainerSkill, 1, 'The test needs a trainer who is good at the skill')

    local skill = types.NPC.stats.skills[skillId](self)
    local attribute = types.Actor.stats.attributes[core.stats.Skill.record(skillId).attribute](self)
    local oldSkill = skill.base
    local oldAttribute = attribute.base

    skill.base = trainerSkill - 1
    attribute.base = 200
    coroutine.yield()
    testing.expectEqual(types.NPC.canTrain(trainer, skillId), true,
        'The trainer should teach a skill they are better at')
    local price = types.NPC.getTrainingPrice(trainer, skillId)
    testing.expectGreaterThan(price, 0, 'Training should cost gold')

    skill.base = trainerSkill
    coroutine.yield()
    local ok, reason = types.NPC.canTrain(trainer, skillId)
    testing.expectEqual(ok, false, 'The trainer should not teach a skill the player is as good at')
    testing.expectEqual(reason, 'trainerSkillTooLow')
    testing.expectGreaterOrEqual(types.NPC.getTrainingPrice(trainer, skillId), price,
        'Training a higher skill should not cost less')

    skill.base = trainerSkill - 1
    attribute.base = attribute.base - (attribute.modified - skill.base)
    coroutine.yield()
    ok, reason = types.NPC.canTrain(trainer, skillId)
    testing.expectEqual(ok, false, 'The player should not train a skill up to its attribute')
    testing.expectEqual(reason, 'attributeTooLow')

    skill.base = oldSkill
    attribute.base = oldAttribute
end)

local statsBeforeTraining = nil

testing.registerLocalTestStep('prepare training on self', function(data)
    local skill = types.NPC.stats.skills[data.skill](self)
    local attribute = types.Actor.stats.attributes[core.stats.Skill.record(data.skill).attribute](self)
    statsBeforeTraining = { skill = skill.base, attribute = attribute.base }
    skill.base = types.NPC.stats.skills[data.skill](data.trainer).modified - 1
    attribute.base = 200
    coroutine.yield()
end)

testing.registerLocalTestStep('restore training on self', function(skillId)
    types.NPC.stats.skills[skillId](self).base = statsBeforeTraining.skill
    types.Actor.stats.attributes[core.stats.Skill.record(skillId).attribute](self).base = statsBeforeTraining.attribute
    coroutine.yield()
end)

local skillsBeforeTest = {}

testing.registerLocalTestStep('set skill on self', function(data)
    local skill = types.NPC.stats.skills[data.skill](self)
    skillsBeforeTest[data.skill] = skillsBeforeTest[data.skill] or skill.base
    skill.base = data.value
    coroutine.yield()
end)

testing.registerLocalTestStep('restore skill on self', function(skillId)
    types.NPC.stats.skills[skillId](self).base = skillsBeforeTest[skillId]
    skillsBeforeTest[skillId] = nil
    coroutine.yield()
end)

-- Reading a skill book in a test raises nothing, so that the tests don't change the player's skills.
local blockBookSkillGain = false
I.SkillProgression.addSkillLevelUpHandler(function(_, source)
    if blockBookSkillGain and source == I.SkillProgression.SKILL_INCREASE_SOURCES.Book then
        return false
    end
end)

testing.registerLocalTestStep('read a book on self', function(book)
    blockBookSkillGain = true
    I.UI.setPauseOnMode(I.UI.MODE.Book, false)
    core.sendGlobalEvent('UseItem', { object = book, actor = self })
    for _ = 1, 10 do
        coroutine.yield()
        if I.UI.getMode() == I.UI.MODE.Book then
            break
        end
    end
    testing.expectEqual(I.UI.getMode(), I.UI.MODE.Book, 'Using the book should open it')
    I.UI.removeMode(I.UI.MODE.Book)
    for _ = 1, 10 do
        coroutine.yield()
        if I.UI.getMode() == nil then
            break
        end
    end
    I.UI.setPauseOnMode(I.UI.MODE.Book, true)
    blockBookSkillGain = false
end)

local statsBeforeLevelUp = nil

testing.registerLocalTestStep('prepare level up on self', function()
    local level = types.Actor.stats.level(self)
    local attributes = types.Actor.stats.attributes
    statsBeforeLevelUp = {
        level = level.current,
        progress = level.progress,
        health = types.Actor.stats.dynamic.health(self).base,
        attributes = {},
    }
    for _, id in ipairs({ 'strength', 'intelligence', 'willpower', 'luck' }) do
        statsBeforeLevelUp.attributes[id] = attributes[id](self).base
    end
    level.progress = core.getGMST('iLevelUpTotal')
    level.skillIncreasesForAttribute.strength = 10
    level.skillIncreasesForAttribute.intelligence = 10
    attributes.strength(self).base = 50
    attributes.intelligence(self).base = 99
    attributes.willpower(self).base = 50
    attributes.luck(self).base = 100
    coroutine.yield()
end)

testing.registerLocalTestStep('restore level up on self', function()
    local level = types.Actor.stats.level(self)
    level.current = statsBeforeLevelUp.level
    level.progress = statsBeforeLevelUp.progress
    level.skillIncreasesForAttribute.strength = 0
    level.skillIncreasesForAttribute.intelligence = 0
    types.Actor.stats.dynamic.health(self).base = statsBeforeLevelUp.health
    for id, value in pairs(statsBeforeLevelUp.attributes) do
        types.Actor.stats.attributes[id](self).base = value
    end
    coroutine.yield()
end)

testing.registerLocalTestStep('rest status on self', function()
    local status = types.Player.getRestStatus(self)
    testing.expectEqual(type(status.isUnderwater), 'boolean', 'isUnderwater should be a boolean')
    testing.expectEqual(type(status.isInAir), 'boolean', 'isInAir should be a boolean')
    testing.expectEqual(status.isEnemyNearby, false, 'There should be no enemies near the player')
    testing.expectEqual(status.canSleep, true, 'The player should be able to sleep in an exterior cell')
    testing.expectEqual(status.canWait, true, 'The player should be able to wait on the ground')
    testing.expectGreaterOrEqual(status.hoursUntilHealed, 1, 'Healing should take at least one hour')
end)

local restFinished = nil
local restProgress = {}

testing.registerLocalTestStep('rest in rest menu on self', function(hours)
    -- The test steps stop while the rest menu pauses the game, so this asks for the rest in the same frame as it
    -- opens the menu. The global script gets the event in the next frame, when the menu is open.
    local restWindows = {}
    for _, window in pairs(I.UI.getWindowsForMode('Rest')) do
        restWindows[window] = true
    end
    testing.expect(restWindows.WaitDialog and restWindows.WaitDialogProgressBar,
        'The rest menu should have the wait dialog and the progress bar')
    restFinished = nil
    restProgress = {}
    I.UI.addMode('Rest')
    core.sendGlobalEvent('restForTest', hours)
    for _ = 1, 600 do
        coroutine.yield()
        if restFinished ~= nil then
            break
        end
    end
    testing.expectEqual(restFinished, true, 'The rest should last all its hours')
    testing.expectEqual(#restProgress, hours + 1, 'The player should hear when the hours start and after each hour')
    testing.expectEqual(restProgress[#restProgress].hoursPassed, hours, 'The last event should come after all hours')
    testing.expectEqual(restProgress[#restProgress].hours, hours, 'The events should tell the length of the rest')
    testing.expectEqual(I.UI.getMode(), nil, 'The rest menu should close when the rest stops')
end)

testing.localEventHandlers.restForTestStopped = function(finished)
    restFinished = finished
end

testing.localEventHandlers.RestProgress = function(data)
    restProgress[#restProgress + 1] = data
end

local addedItems = {}

testing.registerLocalTestStep('check added items on self', function(data)
    local found = nil
    for _, added in ipairs(addedItems) do
        if added.item.recordId == data.recordId then
            found = added
        end
    end
    testing.expect(found ~= nil, 'onItemAdded should report the item')
    testing.expectEqual(found and found.count, data.count, 'onItemAdded should report how many items were added')
    addedItems = {}
end)

testing.registerLocalTestStep('character creation on self', function()
    -- The player's own scripts may create the character. Character creation is over, so that is what stops them.
    local ok, err = pcall(types.Player.setCharGenName, self, 'Lua Hero')
    testing.expectEqual(ok, false, 'The name should not change after character creation')
    testing.expectNotEqual(tostring(err):find('Character creation is finished'), nil, tostring(err))
end)

return {
    engineHandlers = {
        onFrame = testing.updateLocal,
        onItemAdded = function(item, count)
            addedItems[#addedItems + 1] = { item = item, count = count }
        end,
    },
    eventHandlers = testing.localEventHandlers,
}
