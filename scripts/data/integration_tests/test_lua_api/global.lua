local testing = require('testing_util')
local core = require('openmw.core')
local async = require('openmw.async')
local util = require('openmw.util')
local types = require('openmw.types')
local vfs = require('openmw.vfs')
local world = require('openmw.world')
local I = require('openmw.interfaces')

local function initPlayer(player)
    player = player or world.players[1]
    player:teleport('', util.vector3(4096, 4096, 1745), util.transform.identity)
    coroutine.yield()
    return player
end

testing.setSetupLocalTest(function(player)
    initPlayer(player)
end)

testing.registerGlobalTest('crash in lua coroutine when accessing type (#8757)', function()
    local co = coroutine.wrap(function()
        testing.expectEqual(tostring(world.players[1].type), 'Player')
    end)
    co()
end)

testing.registerGlobalTest('timers', function()
    testing.expectAlmostEqual(core.getGameTimeScale(), 30, 'incorrect getGameTimeScale() result')
    testing.expectAlmostEqual(core.getSimulationTimeScale(), 1, 'incorrect getSimulationTimeScale result')

    local startGameTime = core.getGameTime()
    local startSimulationTime = core.getSimulationTime()

    local ts1, ts2, th1, th2
    local cb = async:registerTimerCallback("tfunc", function(arg)
        if arg == 'g' then
            th1 = core.getGameTime() - startGameTime
        else
            ts1 = core.getSimulationTime() - startSimulationTime
        end
    end)
    async:newGameTimer(36, cb, 'g')
    async:newSimulationTimer(0.5, cb, 's')
    async:newUnsavableGameTimer(72, function()
        th2 = core.getGameTime() - startGameTime
    end)
    async:newUnsavableSimulationTimer(1, function()
        ts2 = core.getSimulationTime() - startSimulationTime
    end)

    while not (ts1 and ts2 and th1 and th2) do
        coroutine.yield()
    end

    testing.expectGreaterOrEqual(th1, 36, 'async:newGameTimer failed')
    testing.expectGreaterOrEqual(ts1, 0.5, 'async:newSimulationTimer failed')
    testing.expectGreaterOrEqual(th2, 72, 'async:newUnsavableGameTimer failed')
    testing.expectGreaterOrEqual(ts2, 1, 'async:newUnsavableSimulationTimer failed')
end)

testing.registerGlobalTest('teleport', function()
    local player = world.players[1]
    player:teleport('', util.vector3(100, 50, 500), util.transform.rotateZ(math.rad(90)))
    coroutine.yield()
    testing.expect(player.cell.isExterior, 'teleport to exterior failed')
    testing.expectEqualWithDelta(player.position.x, 100, 1, 'incorrect position after teleporting')
    testing.expectEqualWithDelta(player.position.y, 50, 1, 'incorrect position after teleporting')
    testing.expectEqualWithDelta(player.position.z, 500, 1, 'incorrect position after teleporting')
    testing.expectEqualWithDelta(player.rotation:getYaw(), math.rad(90), 0.05, 'incorrect yaw rotation after teleporting')
    testing.expectEqualWithDelta(player.rotation:getPitch(), math.rad(0), 0.05, 'incorrect pitch rotation after teleporting')

    local rotationX1, rotationZ1 = player.rotation:getAnglesXZ()
    testing.expectEqualWithDelta(rotationX1, math.rad(0), 0.05, 'incorrect x rotation from getAnglesXZ after teleporting')
    testing.expectEqualWithDelta(rotationZ1, math.rad(90), 0.05, 'incorrect z rotation from getAnglesXZ after teleporting')

    local rotationZ2, rotationY2, rotationX2 = player.rotation:getAnglesZYX()
    testing.expectEqualWithDelta(rotationZ2, math.rad(90), 0.05, 'incorrect z rotation from getAnglesZYX after teleporting')
    testing.expectEqualWithDelta(rotationY2, math.rad(0), 0.05, 'incorrect y rotation from getAnglesZYX after teleporting')
    testing.expectEqualWithDelta(rotationX2, math.rad(0), 0.05, 'incorrect x rotation from getAnglesZYX after teleporting')

    player:teleport('', player.position, {rotation=util.transform.rotateZ(math.rad(-90)), onGround=true})
    coroutine.yield()
    testing.expectEqualWithDelta(player.rotation:getYaw(), math.rad(-90), 0.05, 'options.rotation is not working')
    testing.expectLessOrEqual(player.position.z, 400, 'options.onGround is not working')

    player:teleport('', util.vector3(50, -100, 0))
    coroutine.yield()
    testing.expect(player.cell.isExterior, 'teleport to exterior failed')
    testing.expectEqualWithDelta(player.position.x, 50, 1, 'incorrect position after teleporting')
    testing.expectEqualWithDelta(player.position.y, -100, 1, 'incorrect position after teleporting')
    testing.expectEqualWithDelta(player.rotation:getYaw(), math.rad(-90), 0.05, 'teleporting changes rotation')
end)

testing.registerGlobalTest('getGMST', function()
    testing.expectEqual(core.getGMST('non-existed gmst'), nil)
    testing.expectEqual(core.getGMST('Water_RippleFrameCount'), 4)
    testing.expectEqual(core.getGMST('Inventory_DirectionalDiffuseR'), 0.5)
    testing.expectEqual(core.getGMST('Level_Up_Level2'), 'something')
end)

testing.registerGlobalTest('MWScript', function()
    local variableStoreCount = 20
    local variableStore = world.mwscript.getGlobalVariables(player)
    testing.expectEqual(variableStoreCount, #variableStore)

    variableStore.year = 5
    testing.expectEqual(5, variableStore.year)
    variableStore.year = 1
    local indexCheck = 0
    for index, value in ipairs(variableStore) do
        testing.expectEqual(variableStore[index], value)
        indexCheck = indexCheck + 1
    end
    testing.expectEqual(variableStoreCount, indexCheck)
    indexCheck = 0
    for index, value in pairs(variableStore) do
        testing.expectEqual(variableStore[index], value)
        indexCheck = indexCheck + 1
    end
    testing.expectEqual(variableStoreCount, indexCheck)
end)

local function testRecordStore(store, storeName, skipPairs)
    testing.expect(store.records)
    local firstRecord = store.records[1]
    if not firstRecord then
        return
    end
    testing.expectEqual(firstRecord.id, store.records[firstRecord.id].id)
    local status, _ = pcall(function()
        for index, value in ipairs(store.records) do
            if value.id == firstRecord.id then
                testing.expectEqual(index, 1, storeName)
                break
            end
        end
    end)

    testing.expectEqual(status, true, storeName)
end

testing.registerGlobalTest('record stores', function()
    for key, type in pairs(types) do
        if type.records then
            testRecordStore(type, key)
        end
    end
    testRecordStore(core.magic.enchantments, "enchantments")
    testRecordStore(core.magic.effects, "effects", true)
    testRecordStore(core.magic.spells, "spells")

    testRecordStore(core.stats.Attribute, "Attribute")
    testRecordStore(core.stats.Skill, "Skill")

    testRecordStore(core.sound, "sound")
    testRecordStore(core.factions, "factions")
    testRecordStore(core.regions, "regions")
    testRecordStore(core.dialogue.greeting, "dialogue greeting")
    testRecordStore(core.dialogue.topic, "dialogue topic")
    testRecordStore(core.dialogue.journal, "dialogue journal")
    testRecordStore(core.dialogue.persuasion, "dialogue persuasion")
    testRecordStore(core.dialogue.voice, "dialogue voice")

    testRecordStore(types.NPC.classes, "classes")
    testRecordStore(types.NPC.races, "races")
    testRecordStore(types.Player.birthSigns, "birthSigns")
end)

testing.registerGlobalTest('record creation', function()
    local newLight = {
        isCarriable = true,
        isDynamic = true,
        isFire =false,
        isFlicker = false,
        isFlickerSlow = false,
        isNegative = false,
        isOffByDefault = false,
        isPulse = false,
        weight = 1,
        value = 10,
        duration = 12,
        radius = 30,
        color = util.color.hex('123456'),
        name = "TestLight",
        model = "meshes/marker_door.dae"
    }
    local draft = types.Light.createRecordDraft(newLight)
    local record = world.createRecord(draft)
    for key, value in pairs(newLight) do
        testing.expectEqual(record[key], value)
    end
end)

testing.registerGlobalTest('weapon skill', function()
    local skills = {
        [types.Weapon.TYPE.ShortBladeOneHand] = 'shortblade',
        [types.Weapon.TYPE.LongBladeTwoHand] = 'longblade',
        [types.Weapon.TYPE.BluntTwoWide] = 'bluntweapon',
        [types.Weapon.TYPE.AxeOneHand] = 'axe',
        [types.Weapon.TYPE.SpearTwoWide] = 'spear',
        [types.Weapon.TYPE.MarksmanCrossbow] = 'marksman',
        [types.Weapon.TYPE.Arrow] = 'marksman',
    }
    for weaponType, skill in pairs(skills) do
        local draft = types.Weapon.createRecordDraft({
            template = types.Weapon.record('basic_dagger1h'),
            type = weaponType,
        })
        testing.expectEqual(draft.skill, skill, 'Wrong skill for weapon type ' .. weaponType)
    end
end)

local function findCostlyEffect()
    -- load.lua gives this effect a cost, because the example suite's effects cost nothing.
    local effect = core.magic.effects.records[core.magic.EFFECT_TYPE.FireDamage]
    testing.expectGreaterThan(effect.baseCost, 0, 'Expected the test effect to have a cost')
    return effect
end

local function createTestSpell(fields)
    local effect = findCostlyEffect()
    fields.name = 'Test spell'
    fields.effects = {
        { id = effect.id, range = core.magic.RANGE.Self, magnitudeMin = 10, magnitudeMax = 10, duration = 10 },
    }
    return world.createRecord(core.magic.spells.createRecordDraft(fields))
end

testing.registerGlobalTest('spell cost', function()
    local fixed = createTestSpell({ type = core.magic.SPELL_TYPE.Spell, cost = 7, isAutocalc = false })
    testing.expectEqual(core.magic.spells.getCost(fixed), 7, 'A spell without autocalc should cost its cost field')
    testing.expectEqual(core.magic.spells.getCost(fixed.id), 7, 'getCost should accept a record id')

    local autocalc = createTestSpell({ type = core.magic.SPELL_TYPE.Spell, cost = 1, isAutocalc = true })
    testing.expectGreaterThan(core.magic.spells.getCost(autocalc), 1,
        'An autocalc spell should cost what its effects cost, not its cost field')
end)

testing.registerGlobalTest('effect description', function()
    local function gmst(name)
        return core.getGMST(name) or ''
    end
    local spell = createTestSpell({ type = core.magic.SPELL_TYPE.Spell, cost = 5, isAutocalc = false })
    local effect = spell.effects[1]
    local name = effect.effect.name
    testing.expectEqual(core.magic.effects.getDescription(effect),
        name .. ' 10 ' .. gmst('spoints') .. ' ' .. gmst('sfor') .. ' 10 ' .. gmst('sseconds') .. ' '
            .. gmst('sonword') .. ' ' .. gmst('sRangeSelf'),
        'The description should give the magnitude, the duration and the range')
    testing.expectEqual(core.magic.effects.getDescription(effect, { isConstant = true }),
        name .. ' 10 ' .. gmst('spoints'), 'A constant effect should have no duration or range')
    testing.expectEqual(core.magic.effects.getDescription(effect, { isConstant = true, noMagnitude = true }), name,
        'An effect without magnitude should only have a name')

    local area = core.magic.effects.getDescription({
        id = effect.id,
        range = core.magic.RANGE.Target,
        area = 15,
        magnitudeMin = 1,
        magnitudeMax = 5,
        duration = 1,
    }, { noTarget = true })
    testing.expectEqual(area,
        name .. ' 1 ' .. gmst('sTo') .. ' 5 ' .. gmst('spoints') .. ' ' .. gmst('sfor') .. ' 1 ' .. gmst('ssecond')
            .. ' ' .. gmst('sin') .. ' 15 ' .. gmst('sfootarea'),
        'getDescription should accept a table, and give a range of magnitudes and the area')
end)

testing.registerGlobalTest('spell success chance', function()
    local player = initPlayer()
    local spell = createTestSpell({ type = core.magic.SPELL_TYPE.Spell, cost = 5, isAutocalc = false })
    local power = createTestSpell({ type = core.magic.SPELL_TYPE.Power, cost = 0, isAutocalc = false })
    types.Actor.spells(player):add(spell)
    types.Actor.spells(player):add(power)
    coroutine.yield()
    testing.runLocalTest(player, 'spell success chance on self', { spell = spell.id, power = power.id })
end)

testing.registerGlobalTest('enchantment cast cost', function()
    local player = initPlayer()
    local effect = findCostlyEffect()
    local enchantment = world.createRecord(core.magic.enchantments.createRecordDraft({
        type = core.magic.ENCHANTMENT_TYPE.CastOnUse,
        cost = 50,
        charge = 100,
        isAutocalc = false,
        effects = {
            { id = effect.id, range = core.magic.RANGE.Self, magnitudeMin = 10, magnitudeMax = 10, duration = 10 },
        },
    }))
    testing.runLocalTest(player, 'enchantment cast cost on self', enchantment.id)
end)

testing.registerGlobalTest('barter prices', function()
    local player = initPlayer()
    local merchant = world.createObject('landracer')
    merchant:addScript('idle.lua')
    merchant:teleport(player.cell, player.position + util.vector3(0, 500, 0))
    local record = world.createRecord(types.Weapon.createRecordDraft({
        template = types.Weapon.record('basic_dagger1h'),
        value = 100,
        health = 40,
    }))
    local item = world.createObject(record.id, 1)
    local value = types.Weapon.record(item).value
    testing.expectGreaterThan(value, 1, 'The test needs an item that is worth something')
    testing.expectEqual(types.Item.getValue(item), value, 'A weapon should be worth its record value')

    testing.expectEqual(types.Actor.getBuyingPrice(merchant, item), value,
        'A creature should sell an item at its value')
    testing.expectEqual(types.Actor.getSellingPrice(merchant, item, 3), 3 * value,
        'A creature should buy items at their value')

    local health = types.Weapon.record(item).health
    local condition = math.floor(health / 2)
    types.Item.itemData(item).condition = condition
    coroutine.yield()
    testing.expectEqualWithDelta(types.Actor.getBuyingPrice(merchant, item), value * condition / health, 1,
        'A worn item should be worth less')

    item:remove()
    merchant:remove()
end)

testing.registerGlobalTest('service prices', function()
    local player = initPlayer()
    local merchant = world.createObject('landracer')
    merchant:addScript('idle.lua')
    merchant:teleport(player.cell, player.position + util.vector3(0, 500, 0))

    local spell = createTestSpell({ type = core.magic.SPELL_TYPE.Spell, cost = 7, isAutocalc = false })
    local spellValueMult = core.getGMST('fSpellValueMult')
    testing.expectEqual(types.Actor.getSpellPrice(merchant, spell), math.max(1, math.floor(7 * spellValueMult)),
        'A creature should sell a spell for its cost times fSpellValueMult')
    testing.expectEqual(types.Actor.getSpellPrice(merchant, spell.id), types.Actor.getSpellPrice(merchant, spell),
        'getSpellPrice should accept a record id')

    local record = world.createRecord(types.Weapon.createRecordDraft({
        template = types.Weapon.record('basic_dagger1h'),
        value = 100,
        health = 40,
    }))
    local item = world.createObject(record.id, 1)
    types.Item.itemData(item).condition = 30
    coroutine.yield()
    -- An item worth more than its condition costs fRepairMult per point of condition lost.
    local repairPrice = math.max(1, math.floor(10 * core.getGMST('fRepairMult')))
    testing.expectEqual(types.Actor.getRepairPrice(merchant, item), repairPrice,
        'A creature should repair an item for the condition it lost')
    types.Item.itemData(item).condition = 40
    coroutine.yield()
    testing.expectEqual(pcall(types.Actor.getRepairPrice, merchant, item), false,
        'A merchant should have no repair price for an item in full condition')

    item:remove()
    merchant:remove()
end)

testing.registerGlobalTest('training', function()
    local player = initPlayer()
    local trainer = world.createObject(types.NPC.record(player).id)
    trainer:teleport(player.cell, player.position + util.vector3(0, 500, 0))
    coroutine.yield()

    local skills = types.NPC.getTrainingSkills(trainer)
    testing.expectEqual(#skills, 3, 'A trainer should teach three skills')
    local function trainerSkill(id)
        return types.NPC.stats.skills[id](trainer).modified
    end
    local taught = {}
    for i, id in ipairs(skills) do
        taught[id] = true
        if i > 1 then
            testing.expectGreaterOrEqual(trainerSkill(skills[i - 1]), trainerSkill(id),
                'The skills should come best first')
        end
    end
    for _, skill in ipairs(core.stats.Skill.records) do
        if not taught[skill.id] then
            testing.expectGreaterOrEqual(trainerSkill(skills[3]), trainerSkill(skill.id),
                'A trainer should teach their best skills')
        end
    end

    testing.runLocalTest(player, 'training checks on self', { trainer = trainer, skill = skills[1] })
    trainer:remove()
end)

local function removeGold(player)
    local gold = types.Actor.inventory(player):find('gold_001')
    if gold then
        gold:remove()
        coroutine.yield()
    end
end

testing.registerGlobalTest('buying training', function()
    local player = initPlayer()
    removeGold(player)
    local trainer = world.createObject(types.NPC.record(player).id)
    trainer:teleport(player.cell, player.position + util.vector3(0, 500, 0))
    coroutine.yield()
    local skillId = types.NPC.getTrainingSkills(trainer)[1]
    testing.runLocalTest(player, 'prepare training on self', { trainer = trainer, skill = skillId })
    testing.expectEqual(types.NPC.canTrain(trainer, skillId), true, 'The player should be ready to train')

    local price = types.NPC.getTrainingPrice(trainer, skillId)
    testing.expectEqual(pcall(types.Player.buyTraining, player, trainer, skillId), false,
        'The player should not train without enough gold')

    world.createObject('gold_001', price):moveInto(player)
    coroutine.yield()
    local skill = types.NPC.stats.skills[skillId](player)
    local oldSkill = skill.base
    local trainerGold = types.Actor.getBarterGold(trainer)
    local time = core.getGameTime()
    types.Player.buyTraining(player, trainer, skillId)
    for _ = 1, 10 do
        coroutine.yield()
        if skill.base > oldSkill then
            break
        end
    end
    testing.expectEqual(skill.base, oldSkill + 1, 'Training should raise the skill by one point')
    testing.expectEqual(types.Actor.inventory(player):countOf('gold_001'), 0, 'The player should pay for training')
    testing.expectEqual(types.Actor.getBarterGold(trainer), trainerGold + price, 'The trainer should get the gold')
    testing.expectGreaterOrEqual(core.getGameTime() - time, 2 * 3600, 'Training should take two hours')

    testing.runLocalTest(player, 'restore training on self', skillId)
    trainer:remove()
end)

testing.registerGlobalTest('skill books', function()
    local player = initPlayer()
    local record = world.createRecord(types.Book.createRecordDraft({
        name = 'Test skill book',
        skill = 'acrobatics',
        text = 'A test book.',
    }))
    world.createObject(record.id, 1):moveInto(player)
    coroutine.yield()
    local book = types.Actor.inventory(player):find(record.id)
    testing.expectEqual(types.Player.hasReadSkillBook(player, book), false, 'The player should not have read the book')
    testing.expectEqual(pcall(types.Player.hasReadSkillBook, player, 'basic_dagger1h'), false,
        'A weapon should not count as a book')

    testing.runLocalTest(player, 'read a book on self', book)
    testing.expectEqual(types.Player.hasReadSkillBook(player, book), true, 'The player should have read the book')
    testing.expectEqual(types.Player.hasReadSkillBook(player, record.id), true,
        'The book should count as read by its record id')
    book:remove()
end)

testing.registerGlobalTest('buying spells and repairs', function()
    local player = initPlayer()
    removeGold(player)
    local merchant = world.createObject('landracer')
    merchant:addScript('idle.lua')
    merchant:teleport(player.cell, player.position + util.vector3(0, 500, 0))
    local spell = createTestSpell({ type = core.magic.SPELL_TYPE.Spell, cost = 7, isAutocalc = false })
    types.Actor.spells(merchant):add(spell)
    local record = world.createRecord(types.Weapon.createRecordDraft({
        template = types.Weapon.record('basic_dagger1h'),
        value = 100,
        health = 40,
    }))
    world.createObject(record.id, 1):moveInto(player)
    coroutine.yield()
    local inventory = types.Actor.inventory(player)
    local item = inventory:find(record.id)
    types.Item.itemData(item).condition = 30
    coroutine.yield()

    local function isForSale()
        for _, forSale in ipairs(types.Actor.getSpellsForSale(merchant)) do
            if forSale.id == spell.id then
                return true
            end
        end
        return false
    end
    testing.expect(isForSale(), 'A merchant should sell a spell they know')
    local spellPrice = types.Actor.getSpellPrice(merchant, spell)
    local repairPrice = types.Actor.getRepairPrice(merchant, item)
    testing.expectEqual(pcall(types.Player.buySpell, player, merchant, spell), false,
        'The player should not buy a spell without enough gold')

    world.createObject('gold_001', spellPrice + repairPrice):moveInto(player)
    coroutine.yield()
    local merchantGold = types.Actor.getBarterGold(merchant)
    types.Player.buySpell(player, merchant, spell)
    types.Player.buyRepair(player, merchant, item)
    -- The checks run again when the purchases happen, so buying the same things twice in a frame charges once.
    types.Player.buySpell(player, merchant, spell)
    types.Player.buyRepair(player, merchant, item)
    coroutine.yield()
    testing.expect(types.Actor.spells(player)[spell.id] ~= nil, 'The player should learn the spell')
    testing.expect(not isForSale(), 'A merchant should not sell a spell the player knows')
    testing.expectEqual(types.Item.itemData(item).condition, 40, 'The item should be repaired')
    testing.expectEqual(inventory:countOf('gold_001'), 0, 'The player should pay for the spell and the repair')
    testing.expectEqual(types.Actor.getBarterGold(merchant), merchantGold + spellPrice + repairPrice,
        'The merchant should get the gold')
    testing.expectEqual(pcall(types.Player.buyRepair, player, merchant, item), false,
        'A merchant should not repair an item in full condition')

    types.Actor.spells(player):remove(spell)
    item:remove()
    merchant:remove()
end)

testing.registerGlobalTest('travel', function()
    local player = initPlayer()
    local merchant = world.createObject('landracer')
    merchant:addScript('idle.lua')
    merchant:teleport(player.cell, player.position + util.vector3(0, 500, 0))
    coroutine.yield()
    -- The example suite has no merchant with travel destinations, and Lua can't add them to a record.
    testing.expectEqual(#types.Creature.record(merchant).travelDestinations, 0, 'The creature should not offer travel')
    testing.expectEqual(pcall(types.Actor.getTravelPrice, merchant, 1), false,
        'A merchant should have no price for a destination they do not offer')
    testing.expectEqual(pcall(types.Actor.getTravelHours, merchant, 1), false,
        'A merchant should have no travel time for a destination they do not offer')
    testing.expectEqual(pcall(types.Player.buyTravel, player, merchant, 1), false,
        'The player should not travel to a destination the merchant does not offer')
    merchant:remove()
end)

-- Waits for the outcome that an action gives its callback.
local function waitForOutcome(start)
    local result = nil
    start(async:callback(function(outcome)
        result = outcome
    end))
    for _ = 1, 10 do
        coroutine.yield()
        if result ~= nil then
            break
        end
    end
    return result
end

testing.registerGlobalTest('repairing and recharging', function()
    local player = initPlayer()
    local inventory = types.Actor.inventory(player)
    local soul = world.createRecord(types.Creature.createRecordDraft({
        template = types.Creature.record('landracer'),
        soulValue = 100,
    }))
    local enchantment = world.createRecord(core.magic.enchantments.createRecordDraft({
        type = core.magic.ENCHANTMENT_TYPE.CastOnUse,
        cost = 5,
        charge = 100,
        isAutocalc = false,
        effects = {
            { id = findCostlyEffect().id, range = core.magic.RANGE.Self, magnitudeMin = 1, magnitudeMax = 1 },
        },
    }))
    local record = world.createRecord(types.Weapon.createRecordDraft({
        template = types.Weapon.record('basic_dagger1h'),
        health = 40,
        enchant = enchantment.id,
    }))
    world.createObject(record.id, 1):moveInto(player)
    world.createObject('OMW_Test_RepairTool', 1):moveInto(player)
    world.createObject('OMW_Test_SoulGem', 1):moveInto(player)
    coroutine.yield()
    local item = inventory:find(record.id)
    local tool = inventory:find('OMW_Test_RepairTool')
    local gem = inventory:find('OMW_Test_SoulGem')
    types.Item.itemData(item).condition = 10
    types.Item.itemData(item).enchantmentCharge = 10
    types.Item.itemData(gem).soul = soul.id
    testing.runLocalTest(player, 'set skill on self', { skill = 'armorer', value = 100 })
    testing.runLocalTest(player, 'set skill on self', { skill = 'enchant', value = 100 })

    local repairChance = types.Player.getRepairChance(player)
    testing.expectGreaterThan(repairChance, 0, 'A player with 100 Armorer should be able to repair')
    testing.expectLessOrEqual(repairChance, 100, 'The chance should be at most 100')
    testing.expectEqual(pcall(types.Player.repairItem, player, item, gem), false,
        'The player should not repair with something that is not a repair tool')
    local repaired = waitForOutcome(function(callback)
        types.Player.repairItem(player, item, tool, callback)
    end)
    testing.expect(repaired ~= nil, 'The callback should receive whether the item was repaired')
    if repaired then
        testing.expectGreaterThan(types.Item.itemData(item).condition, 10, 'A repair should restore some condition')
    else
        testing.expectEqual(types.Item.itemData(item).condition, 10, 'A failed repair should restore nothing')
    end
    testing.expectEqual(types.Item.itemData(tool).condition, 9, 'Each repair should use the tool once')

    local rechargeChance = types.Player.getRechargeChance(player)
    testing.expectGreaterThan(rechargeChance, 0, 'A player with 100 Enchant should be able to recharge')
    testing.expectLessOrEqual(rechargeChance, 100, 'The chance should be at most 100')
    local recharged = waitForOutcome(function(callback)
        types.Player.rechargeItem(player, item, gem, callback)
    end)
    testing.expect(recharged ~= nil, 'The callback should receive whether the item was recharged')
    if recharged then
        testing.expectGreaterOrEqual(types.Item.itemData(item).enchantmentCharge, 10,
            'Recharging should not take charge away')
    else
        testing.expectEqual(types.Item.itemData(item).enchantmentCharge, 10, 'A failed recharge should restore nothing')
    end
    testing.expectEqual(inventory:countOf('OMW_Test_SoulGem'), 0, 'Recharging should use up the soul gem')

    testing.runLocalTest(player, 'restore skill on self', 'armorer')
    testing.runLocalTest(player, 'restore skill on self', 'enchant')
    item:remove()
    tool:remove()
end)

testing.registerGlobalTest('enchanting', function()
    local player = initPlayer()
    removeGold(player)
    local inventory = types.Actor.inventory(player)
    local soul = world.createRecord(types.Creature.createRecordDraft({
        template = types.Creature.record('landracer'),
        soulValue = 400,
    }))
    local record = world.createRecord(types.Weapon.createRecordDraft({
        template = types.Weapon.record('basic_dagger1h'),
        name = 'Plain dagger',
        enchantCapacity = 100,
    }))
    world.createObject(record.id, 1):moveInto(player)
    world.createObject('OMW_Test_SoulGem', 1):moveInto(player)
    coroutine.yield()
    local item = inventory:find(record.id)
    local gem = inventory:find('OMW_Test_SoulGem')
    types.Item.itemData(gem).soul = soul.id
    local enchanter = world.createObject(types.NPC.record(player).id)
    enchanter:teleport(player.cell, player.position + util.vector3(0, 500, 0))
    local known = createTestSpell({ type = core.magic.SPELL_TYPE.Spell })
    coroutine.yield()

    local setup = {
        item = item,
        soulGem = gem,
        effects = {
            {
                id = findCostlyEffect().id,
                range = core.magic.RANGE.Touch,
                magnitudeMin = 1,
                magnitudeMax = 5,
                duration = 1,
            },
        },
    }
    testing.expectEqual(pcall(types.Player.getEnchantingInfo, player, setup), false,
        'The player should not use an effect from a spell they do not know')
    types.Actor.spells(player):add(known)
    coroutine.yield()
    local listed = false
    for _, effect in ipairs(types.Player.getEnchantingEffects(player)) do
        listed = listed or effect.id == findCostlyEffect().id
    end
    testing.expect(listed, 'The player should be able to use the effects of the spells they know')
    local info = types.Player.getEnchantingInfo(player, setup)
    testing.expectEqual(info.type, core.magic.ENCHANTMENT_TYPE.CastOnStrike,
        'A dagger should cast when it strikes, unless the player picks another type')
    local canCastOnUse = false
    for _, castType in ipairs(info.types) do
        canCastOnUse = canCastOnUse or castType == core.magic.ENCHANTMENT_TYPE.CastOnUse
    end
    testing.expect(canCastOnUse, 'A dagger should be able to cast when it is used')
    testing.expectGreaterThan(info.points, 0, 'The effect should need enchantment points')
    testing.expectGreaterOrEqual(info.maxPoints, info.points, 'The effect should fit in the dagger')
    testing.expectEqual(info.charge, 400, 'The charge should be the strength of the soul')
    testing.expectEqual(info.price, 0, 'Enchanting an item yourself should cost nothing')
    testing.expectEqual(pcall(types.Player.getEnchantingInfo, player,
        { item = item, type = core.magic.ENCHANTMENT_TYPE.CastOnce }), false,
        'A dagger should not be enchanted to cast once')

    setup.enchanter = enchanter
    setup.name = 'Enchanted dagger'
    info = types.Player.getEnchantingInfo(player, setup)
    testing.expectEqual(info.chance, 100, 'An enchanter should never fail')
    testing.expectGreaterThan(info.price, 0, 'An enchanter should ask for gold')
    testing.expectEqual(pcall(types.Player.enchantItem, player, setup), false,
        'The player should not pay an enchanter without enough gold')

    world.createObject('gold_001', info.price):moveInto(player)
    coroutine.yield()
    local enchanterGold = types.Actor.getBarterGold(enchanter)
    local enchanted = waitForOutcome(function(callback)
        types.Player.enchantItem(player, setup, callback)
    end)
    testing.expectEqual(enchanted, true, 'An enchanter should enchant the item')
    testing.expectEqual(inventory:countOf(record.id), 0, 'The plain dagger should be gone')
    testing.expectEqual(inventory:countOf('OMW_Test_SoulGem'), 0, 'Enchanting should use up the soul gem')
    testing.expectEqual(inventory:countOf('gold_001'), 0, 'The player should pay the enchanter')
    testing.expectEqual(types.Actor.getBarterGold(enchanter), enchanterGold + info.price,
        'The enchanter should get the gold')
    local result = nil
    for _, weapon in ipairs(inventory:getAll(types.Weapon)) do
        if types.Weapon.record(weapon).name == 'Enchanted dagger' then
            result = weapon
        end
    end
    testing.expect(result ~= nil, 'The player should get the enchanted dagger')
    testing.expect(types.Weapon.record(result).enchant ~= nil, 'The new dagger should be enchanted')

    types.Actor.spells(player):remove(known)
    result:remove()
    enchanter:remove()
end)

testing.registerGlobalTest('alchemy', function()
    local player = initPlayer()
    local inventory = types.Actor.inventory(player)
    local function createIngredient(name)
        return world.createRecord(types.Ingredient.createRecordDraft({
            name = name,
            weight = 1,
            value = 1,
            effects = { { id = findCostlyEffect().id } },
        }))
    end
    local root = createIngredient('Test root')
    local petal = createIngredient('Test petal')
    world.createObject(root.id, 3):moveInto(player)
    world.createObject(petal.id, 2):moveInto(player)
    world.createObject('OMW_Test_Mortar', 1):moveInto(player)
    coroutine.yield()
    local ingredients = { inventory:find(root.id), inventory:find(petal.id) }
    testing.runLocalTest(player, 'set skill on self', { skill = 'alchemy', value = 100 })

    local known = types.Player.getKnownIngredientEffects(player, ingredients[1])
    testing.expectEqual(#known, 1, 'A player with 100 Alchemy should know the effect of the root')
    testing.expectEqual(known[1].id, findCostlyEffect().id, 'The known effect should be the effect of the root')

    local info = types.Player.getAlchemyInfo(player, { ingredients = ingredients, name = 'Test potion' })
    testing.expectEqual(#info.effects, 1, 'The potion should have the effect that both ingredients share')
    testing.expectEqual(info.effects[1].id, findCostlyEffect().id, 'The potion should have the shared effect')
    testing.expectEqual(info.effects[1].isKnown, true, 'A player with 100 Alchemy should know the effect')
    testing.expectEqual(info.maxCount, 2, 'The scarcest ingredient should limit the number of potions')
    testing.expectGreaterOrEqual(#info.apparatus, 1, 'The player should brew with the mortar they carry')
    local twice = { ingredients = { ingredients[1], ingredients[1] } }
    testing.expectEqual(pcall(types.Player.getAlchemyInfo, player, twice), false,
        'The same kind of ingredient should not go in twice')
    testing.expectEqual(pcall(types.Player.brewPotion, player, { ingredients = { ingredients[1] }, name = 'Potion' }),
        false, 'The player should not brew with one ingredient')

    local brewed = waitForOutcome(function(callback)
        types.Player.brewPotion(player, { ingredients = ingredients, name = 'Test potion', count = 2 }, callback)
    end)
    testing.expectEqual(brewed, 2, 'A player with 100 Alchemy should brew every potion')
    testing.expectEqual(inventory:countOf(root.id), 1, 'Each potion should use up one root')
    testing.expectEqual(inventory:countOf(petal.id), 0, 'Each potion should use up one petal')
    local potions = 0
    for _, potion in ipairs(inventory:getAll(types.Potion)) do
        if types.Potion.record(potion).name == 'Test potion' then
            potions = potions + potion.count
            potion:remove()
        end
    end
    testing.expectEqual(potions, 2, 'The player should get the potions')

    testing.runLocalTest(player, 'restore skill on self', 'alchemy')
    inventory:find(root.id):remove()
    inventory:find('OMW_Test_Mortar'):remove()
end)

testing.registerGlobalTest('spellmaking', function()
    local player = initPlayer()
    removeGold(player)
    local merchant = world.createObject(types.NPC.record(player).id)
    merchant:teleport(player.cell, player.position + util.vector3(0, 500, 0))
    local known = createTestSpell({ type = core.magic.SPELL_TYPE.Spell })
    local effect = findCostlyEffect()
    local spells = types.Actor.spells(player)
    coroutine.yield()

    local params = {
        id = effect.id,
        range = core.magic.RANGE.Target,
        magnitudeMin = 1,
        magnitudeMax = 10,
        duration = 5,
    }
    local setup = { merchant = merchant, name = 'Custom spell', effects = { params } }
    testing.expectEqual(pcall(types.Player.getSpellmakingInfo, player, setup), false,
        'The player should not use an effect from a spell they do not know')

    spells:add(known)
    coroutine.yield()
    local listed = false
    for _, listedEffect in ipairs(types.Player.getSpellmakingEffects(player)) do
        listed = listed or listedEffect.id == effect.id
    end
    testing.expect(listed, 'The player should be able to use the effects of the spells they know')
    local info = types.Player.getSpellmakingInfo(player, setup)
    testing.expectGreaterThan(info.cost, 0, 'The spell should cost magicka')
    testing.expectGreaterThan(info.price, 0, 'The merchant should ask for gold')
    testing.expectGreaterOrEqual(info.chance, 0, 'The chance should be at least 0')
    testing.expectLessOrEqual(info.chance, 100, 'The chance should be at most 100')

    local tooStrong = { id = effect.id, range = core.magic.RANGE.Target, magnitudeMin = 1, magnitudeMax = 101 }
    local strong = { merchant = merchant, effects = { tooStrong } }
    testing.expectEqual(pcall(types.Player.getSpellmakingInfo, player, strong), false,
        'The magnitude should be at most 100')
    local twice = { merchant = merchant, effects = { params, params } }
    testing.expectEqual(pcall(types.Player.getSpellmakingInfo, player, twice), false,
        'An effect should not be there twice')
    local onSelf = { id = effect.id, range = core.magic.RANGE.Self, magnitudeMin = 1, magnitudeMax = 10, duration = 5 }
    testing.expectEqual(pcall(types.Player.getSpellmakingInfo, player, { merchant = merchant, effects = { onSelf } }),
        true, 'An effect on self should be allowed')
    onSelf.area = 5
    testing.expectEqual(pcall(types.Player.getSpellmakingInfo, player, { merchant = merchant, effects = { onSelf } }),
        false, 'An effect on self should have no area')
    testing.expectEqual(pcall(types.Player.makeSpell, player, setup), false,
        'The player should not make a spell without enough gold')

    world.createObject('gold_001', info.price):moveInto(player)
    coroutine.yield()
    local merchantGold = types.Actor.getBarterGold(merchant)
    local spellId = waitForOutcome(function(callback)
        types.Player.makeSpell(player, setup, callback)
    end)
    testing.expect(spellId ~= nil, 'The callback should receive the new spell')
    local spell = core.magic.spells.records[spellId]
    testing.expectEqual(spell.name, 'Custom spell', 'The spell should have its name')
    testing.expectEqual(spell.cost, info.cost, 'The spell should cost what the info said')
    testing.expect(spells[spellId] ~= nil, 'The player should know the new spell')
    testing.expectEqual(types.Actor.inventory(player):countOf('gold_001'), 0, 'The player should pay for the spell')
    testing.expectEqual(types.Actor.getBarterGold(merchant), merchantGold + info.price,
        'The merchant should get the gold')

    spells:remove(spellId)
    spells:remove(known)
    merchant:remove()
end)

testing.registerGlobalTest('dialogue services', function()
    local player = initPlayer()
    local npcRecord = world.createRecord(types.NPC.createRecordDraft({
        template = types.NPC.record(player),
        isAutocalc = false,
        servicesOffered = { Barter = true, Training = true, Spells = true },
    }))
    local npc = world.createObject(npcRecord.id)
    testing.expectEqual(table.concat(types.Actor.getDialogueServices(npc), ' '), 'persuasion barter spells training',
        'An NPC should offer persuasion and the services in their record, in the order of the Dialogue window')

    local creatureRecord = world.createRecord(types.Creature.createRecordDraft({
        template = types.Creature.record('landracer'),
        servicesOffered = { Repair = true },
    }))
    local creature = world.createObject(creatureRecord.id)
    testing.expectEqual(table.concat(types.Actor.getDialogueServices(creature), ' '), 'repair',
        'A creature should not offer persuasion')

    local item = world.createObject('basic_dagger1h')
    testing.expectEqual(pcall(types.Actor.getDialogueServices, item), false, 'Only actors should offer services')

    item:remove()
    creature:remove()
    npc:remove()
end)

testing.registerGlobalTest('stolen items', function()
    local player = initPlayer()
    local owner = world.createRecord(types.NPC.createRecordDraft({
        template = types.NPC.record(player),
        name = 'Item owner',
    }))
    local weapon = world.createRecord(types.Weapon.createRecordDraft({
        template = types.Weapon.record('basic_dagger1h'),
        name = 'Owned dagger',
    }))
    local item = world.createObject(weapon.id, 1)
    item:teleport(player.cell, player.position + util.vector3(0, 100, 0))
    coroutine.yield()
    item.owner.recordId = owner.id
    testing.expectEqual(#types.Item.getStolenFrom(item), 0, 'An item the player has not taken should not be stolen')

    item:activateBy(player)
    local stolen = nil
    for _ = 1, 10 do
        coroutine.yield()
        stolen = types.Actor.inventory(player):find(weapon.id)
        if stolen then
            break
        end
    end
    testing.expect(stolen ~= nil, 'The player should pick up the item')

    local owners = types.Item.getStolenFrom(stolen)
    testing.expectEqual(#owners, 1, 'The item should be stolen from one owner')
    testing.expectEqual(owners[1].recordId, owner.id, 'The item should be stolen from its owner')
    testing.expectEqual(owners[1].factionId, nil, 'The owner should not be a faction')
    testing.expectEqual(owners[1].count, 1, 'The player should have stolen one item')
    stolen:remove()
end)

testing.registerGlobalTest('gold and soul gems', function()
    local gold = world.createObject('gold_001', 10)
    testing.expectEqual(types.Miscellaneous.isGold(gold), true, 'gold_001 should be gold')
    testing.expectEqual(types.Miscellaneous.isSoulGem(gold), false, 'Gold should not be a soul gem')
    local soulGem = world.createObject('misc_soulgem_omw_test', 1)
    testing.expectEqual(types.Miscellaneous.isSoulGem(soulGem), true, 'A misc_soulgem item should be a soul gem')
    testing.expectEqual(types.Miscellaneous.isGold(soulGem), false, 'A soul gem should not be gold')
    local misc = world.createRecord(types.Miscellaneous.createRecordDraft({ name = 'Test trinket', value = 5 }))
    local trinket = world.createObject(misc.id, 1)
    testing.expectEqual(types.Miscellaneous.isGold(trinket), false, 'A made-up item should not be gold')
    testing.expectEqual(types.Miscellaneous.isSoulGem(trinket), false, 'A made-up item should not be a soul gem')
    local dagger = world.createObject('basic_dagger1h', 1)
    testing.expectEqual(types.Miscellaneous.isGold(dagger), false, 'A weapon should not be gold')
    testing.expectEqual(types.Miscellaneous.isSoulGem(dagger), false, 'A weapon should not be a soul gem')
end)

testing.registerGlobalTest('bartering', function()
    local player = initPlayer()
    removeGold(player)
    local merchant = world.createObject(world.createRecord(types.Creature.createRecordDraft({
        template = types.Creature.record('landracer'),
        servicesOffered = { Weapon = true },
    })).id)
    merchant:addScript('idle.lua')
    merchant:teleport(player.cell, player.position + util.vector3(0, 500, 0))
    local bought = world.createRecord(types.Weapon.createRecordDraft({
        template = types.Weapon.record('basic_dagger1h'),
        name = 'Dagger for sale',
        value = 100,
    }))
    local sold = world.createRecord(types.Weapon.createRecordDraft({
        template = types.Weapon.record('basic_dagger1h'),
        name = 'Dagger to sell',
        value = 50,
    }))
    world.createObject(bought.id, 2):moveInto(merchant)
    world.createObject(sold.id, 1):moveInto(player)
    world.createObject('gold_001', 300):moveInto(player)
    types.Actor.setBarterGold(merchant, 500)
    coroutine.yield()
    local merchantInventory = types.Actor.inventory(merchant)
    local playerInventory = types.Actor.inventory(player)
    local forSale = merchantInventory:find(bought.id)
    local toSell = playerInventory:find(sold.id)
    testing.expectEqual(types.Actor.canBarterItem(merchant, forSale), true, 'A weapon merchant should trade weapons')
    testing.expectEqual(types.Actor.canBarterItem(merchant, playerInventory:find('gold_001')), false,
        'A merchant should not trade gold')

    local setup = { buy = { { item = forSale, count = 1 } }, sell = { { item = toSell } } }
    local offer = types.Player.getBarterOffer(player, merchant, setup)
    local price = types.Actor.getSellingPrice(merchant, toSell) - types.Actor.getBuyingPrice(merchant, forSale)
    testing.expectEqual(offer, price, 'The offer should add up the prices')
    testing.expectEqual(pcall(types.Player.barter, player, merchant, {}), false,
        'The player should not make an offer without items')
    local tooMuch = { buy = setup.buy, sell = setup.sell, gold = -1000 }
    testing.expectEqual(pcall(types.Player.barter, player, merchant, tooMuch), false,
        'The player should not offer more gold than they have')

    local accepted = waitForOutcome(function(callback)
        types.Player.barter(player, merchant, { buy = setup.buy, sell = setup.sell, gold = offer + 10 }, callback)
    end)
    testing.expectEqual(accepted, false, 'A creature should refuse to haggle')
    testing.expectEqual(playerInventory:countOf(bought.id), 0, 'The player should not get items when refused')

    accepted = waitForOutcome(function(callback)
        types.Player.barter(player, merchant, setup, callback)
    end)
    testing.expectEqual(accepted, true, 'A creature should accept its own offer')
    testing.expectEqual(playerInventory:countOf(bought.id), 1, 'The player should get the item they bought')
    testing.expectEqual(merchantInventory:countOf(bought.id), 1, 'The merchant should keep the rest of the stack')
    testing.expectEqual(merchantInventory:countOf(sold.id), 1, 'The merchant should get the item the player sold')
    testing.expectEqual(playerInventory:countOf(sold.id), 0, 'The player should lose the item they sold')
    testing.expectEqual(playerInventory:countOf('gold_001'), 300 + offer, 'The player should pay the difference')
    testing.expectEqual(types.Actor.getBarterGold(merchant), 500 - offer, 'The merchant should get the gold')

    playerInventory:find(bought.id):remove()
    removeGold(player)
    merchant:remove()
end)

testing.registerGlobalTest('taking items from containers', function()
    local player = initPlayer()
    local owner = world.createRecord(types.NPC.createRecordDraft({
        template = types.NPC.record(player),
        name = 'Chest owner',
    }))
    local weapon = world.createRecord(types.Weapon.createRecordDraft({
        template = types.Weapon.record('basic_dagger1h'),
        name = 'Dagger in a chest',
    }))
    local chest = world.createObject(world.createRecord(types.Container.createRecordDraft({
        name = 'Test chest',
        weight = 100,
    })).id)
    world.createObject(weapon.id, 2):moveInto(chest)
    coroutine.yield()
    chest.owner.recordId = owner.id
    testing.expectEqual(types.Player.isAllowedToUse(player, chest), false,
        'The player should not be allowed to take from an owned chest')

    local item = types.Container.inventory(chest):find(weapon.id)
    testing.expectEqual(pcall(types.Player.takeItem, player, chest, item, 3), false,
        'The player should not take more items than the chest holds')
    types.Player.takeItem(player, chest, item, 1)
    local taken = nil
    for _ = 1, 10 do
        coroutine.yield()
        taken = types.Actor.inventory(player):find(weapon.id)
        if taken then
            break
        end
    end
    testing.expect(taken ~= nil, 'The player should get the item')
    testing.expectEqual(types.Actor.inventory(player):countOf(weapon.id), 1, 'The player should take one item')
    testing.expectEqual(types.Container.inventory(chest):countOf(weapon.id), 1, 'One item should stay in the chest')
    local owners = types.Item.getStolenFrom(taken)
    testing.expectEqual(#owners, 1, 'Taking from an owned chest should be theft')
    testing.expectEqual(owners[1].recordId, owner.id, 'The item should be stolen from the owner of the chest')

    chest.owner.recordId = nil
    testing.expectEqual(types.Player.isAllowedToUse(player, chest), true,
        'The player should be allowed to take from a chest nobody owns')
    taken:remove()
    chest:remove()
end)

testing.registerGlobalTest('pickpocketing', function()
    local player = initPlayer()
    removeGold(player)
    local victim = world.createObject(types.NPC.record(player).id)
    victim:teleport(player.cell, player.position + util.vector3(0, 500, 0))
    world.createObject('gold_001', 10):moveInto(victim)
    coroutine.yield()
    local gold = types.Actor.inventory(victim):find('gold_001')
    local victimGold = gold.count

    local chance = types.Player.getPickpocketChance(player, victim, gold)
    testing.expectGreaterOrEqual(chance, 0, 'The chance should be at least 0')
    testing.expectLessOrEqual(chance, 100, 'The chance should be at most 100')
    testing.expectGreaterOrEqual(types.Player.getPickpocketChance(player, victim), chance,
        'Stopping should be no riskier than stealing')

    testing.runLocalTest(player, 'set skill on self', { skill = 'sneak', value = 100 })
    local noticed = false
    for _, item in ipairs(types.Player.getPickpocketItems(player, victim)) do
        noticed = noticed or item == gold
    end
    testing.expect(noticed, 'A player with 100 Sneak should notice every item')

    local detected = waitForOutcome(function(callback)
        types.Player.pickpocket(player, victim, gold, 4, callback)
    end)
    testing.expect(detected ~= nil, 'The callback should receive whether the victim noticed the player')
    local stolen = detected and 0 or 4
    testing.expectEqual(types.Actor.inventory(player):countOf('gold_001'), stolen,
        'The player should get the gold only if the victim did not notice')
    testing.expectEqual(types.Actor.inventory(victim):countOf('gold_001'), victimGold - stolen,
        'The victim should lose the gold only if they did not notice')

    if not detected then
        detected = waitForOutcome(function(callback)
            types.Player.finishPickpocketing(player, victim, callback)
        end)
        testing.expect(detected ~= nil, 'The callback should receive whether the victim noticed the player')
    end

    testing.runLocalTest(player, 'restore skill on self', 'sneak')
    victim:remove()
    removeGold(player)
    types.Player.setCrimeLevel(player, 0)
end)

testing.registerGlobalTest('level up', function()
    local player = initPlayer()
    testing.runLocalTest(player, 'prepare level up on self')
    local level = types.Actor.stats.level(player)
    local oldLevel = level.current
    local multiplier = core.getGMST('iLevelUp10Mult') or 1

    local info = types.Player.getLevelUpInfo(player)
    testing.expectEqual(info.isReady, true, 'The player should be ready to level up')
    testing.expectEqual(info.level, oldLevel + 1, 'The player should reach the next level')
    testing.expectEqual(info.attributeCount, 3, 'The player should raise three attributes')
    testing.expectEqual(info.attributeIncreases.strength, multiplier, 'Strength should gain its multiplier')
    testing.expectEqual(info.attributeIncreases.intelligence, 1, 'Intelligence should stop at 100')
    testing.expectEqual(info.attributeIncreases.luck, 0, 'Luck at 100 should gain nothing')
    testing.expectEqual(type(info.classImage), 'string', 'The class picture should have a name')
    testing.expectEqual(type(info.description), 'string', 'The level should have a text')

    testing.expectEqual(pcall(types.Player.levelUp, player, { 'strength', 'intelligence' }), false,
        'The player should not raise fewer than three attributes')
    testing.expectEqual(pcall(types.Player.levelUp, player, { 'strength', 'strength', 'intelligence' }), false,
        'The player should not raise an attribute twice')
    testing.expectEqual(pcall(types.Player.levelUp, player, { 'strength', 'intelligence', 'luck' }), false,
        'The player should not raise an attribute that is at 100')

    types.Player.levelUp(player, { 'strength', 'intelligence', 'willpower' })
    for _ = 1, 10 do
        coroutine.yield()
        if level.current > oldLevel then
            break
        end
    end
    local attributes = types.Actor.stats.attributes
    testing.expectEqual(level.current, oldLevel + 1, 'The player should gain a level')
    testing.expectEqual(attributes.strength(player).base, 50 + multiplier, 'Strength should gain its multiplier')
    testing.expectEqual(attributes.intelligence(player).base, 100, 'Intelligence should stop at 100')
    testing.expectEqual(attributes.willpower(player).base, 51, 'Willpower should gain one point')
    testing.expectEqual(level.skillIncreasesForAttribute.strength, 0, 'The skill increases should start over')
    testing.expectEqual(types.Player.getLevelUpInfo(player).isReady, false, 'The level-up should use up the progress')

    testing.runLocalTest(player, 'restore level up on self')
end)

testing.registerGlobalTest('rest status', function()
    local player = initPlayer()
    testing.expectEqual(types.Player.getRestStatus(player).canSleep, true,
        'The player should be able to sleep in an exterior cell')
    testing.runLocalTest(player, 'rest status on self')
end)

testing.registerGlobalTest('resting', function()
    local player = initPlayer()
    testing.expectEqual(pcall(types.Player.rest, player, 2), false, 'The player should rest only in the rest menu')
    local time = core.getGameTime()
    testing.runLocalTest(player, 'rest in rest menu on self', 2)
    testing.expectGreaterOrEqual(core.getGameTime() - time, 2 * 3600, 'Two hours should pass')
end)

-- The rest menu pauses the game, and with it the tests, so the player's test step asks for the rest with an event.
testing.globalEventHandlers.restForTest = function(hours)
    local player = world.players[1]
    types.Player.rest(player, hours, async:callback(function(finished)
        player:sendEvent('restForTestStopped', finished)
    end))
end

testing.registerGlobalTest('character creation', function()
    local player = initPlayer()
    testing.expectEqual(pcall(types.Player.setCharGenName, player, 'Too late'), false,
        'Character creation should be over in a new game that skips it')

    -- The game's own scripts end character creation again soon, so each step starts it right before the change.
    local globals = world.mwscript.getGlobalVariables(player)
    globals.chargenstate = 1
    testing.expectEqual(pcall(types.Player.setCharGenName, player, ''), false, 'The name should not be empty')
    testing.expectEqual(pcall(types.Player.setCharGenRace, player, { race = 'omw_unplayable_race' }), false,
        'The race should be playable')
    testing.expectEqual(pcall(types.Player.setCharGenBirthSign, player, 'omw_no_such_sign'), false,
        'The birth sign should exist')
    types.Player.setCharGenName(player, 'Lua Hero')
    types.Player.setCharGenRace(player, { race = 'omw_chargen_race', isMale = false })
    types.Player.setCharGenClass(player, 'omw_chargen_class')
    coroutine.yield()
    local record = types.NPC.record(player)
    testing.expectEqual(record.name, 'Lua Hero')
    testing.expectEqual(record.race, 'omw_chargen_race')
    testing.expectEqual(record.isMale, false)
    testing.expectEqual(record.class, 'omw_chargen_class')
    testing.expect(types.Actor.spells(player)['omw_chargen_power'] ~= nil, 'The player should get the race power')
    testing.expectEqual(#types.Player.getCharGenHeads('omw_chargen_race', false), 0, 'The example suite has no heads')

    local madeUp = {
        name = 'Lua class',
        specialization = 'magic',
        attributes = { 'strength', 'intelligence' },
        majorSkills = { 'longblade', 'destruction', 'alteration', 'mediumarmor', 'block' },
        minorSkills = { 'athletics', 'restoration', 'mysticism', 'enchant', 'heavyarmor' },
    }
    globals.chargenstate = 1
    types.Player.setCharGenClass(player, madeUp)
    coroutine.yield()
    local madeUpId = types.NPC.record(player).class
    testing.expectEqual(types.NPC.classes.record(madeUpId).name, 'Lua class')
    madeUp.name = 'Changed class'
    globals.chargenstate = 1
    types.Player.setCharGenClass(player, madeUp)
    coroutine.yield()
    testing.expectEqual(types.NPC.record(player).class, madeUpId, 'A new made-up class should replace the old one')
    testing.expectEqual(types.NPC.classes.record(madeUpId).name, 'Changed class')
    madeUp.minorSkills[1] = 'longblade'
    globals.chargenstate = 1
    testing.expectEqual(pcall(types.Player.setCharGenClass, player, madeUp), false,
        'A made-up class should not repeat a skill')
    globals.chargenstate = -1

    local questions = types.Player.getCharGenClassQuestions()
    testing.expectEqual(#questions, 10)
    testing.expectEqual(questions[1].question, 'How do you fight?')
    testing.expectEqual(questions[1].answers.combat, 'With a sword')
    testing.expectEqual(questions[1].answers.magic, 'With a spell')
    testing.expectEqual(questions[1].answers.stealth, 'From the shadows')
    testing.expectEqual(types.Player.getCharGenClassForAnswers({ combat = 6, magic = 1, stealth = 3 }):lower(),
        'crusader')
    testing.expectEqual(pcall(types.Player.getCharGenClassForAnswers, { combat = 9 }), false,
        'The answers should add up to the number of questions')

    testing.runLocalTest(player, 'character creation on self')
end)

testing.registerGlobalTest('UTF-8 characters', function()
    testing.expectEqual(utf8.codepoint("😀"), 0x1F600)

    local chars = {}

    for codepoint = 0, 0x10FFFF do
        local char = utf8.char(codepoint)
        local charSize = string.len(char)

        testing.expect(not chars[char], nil, "Duplicate UTF-8 character: " .. char)
        chars[char] = true

        if codepoint <= 0x7F then
            testing.expectEqual(charSize, 1)
        elseif codepoint <= 0x7FF then
            testing.expectEqual(charSize, 2)
        elseif codepoint <= 0xFFFF then
            testing.expectEqual(charSize, 3)
        elseif codepoint <= 0x10FFFF then
            testing.expectEqual(charSize, 4)
        end

        testing.expectEqual(utf8.codepoint(char), codepoint)
        testing.expectEqual(utf8.len(char), 1)
    end
end)

testing.registerGlobalTest('UTF-8 strings', function()
    local utf8str = "Hello, 你好, 🌎!"

    local str = ""
    for utf_char in utf8str:gmatch(utf8.charpattern) do
        str = str .. utf_char
    end
    testing.expectEqual(str, utf8str)

    testing.expectEqual(utf8.len(utf8str), 13)
    testing.expectEqual(utf8.offset(utf8str, 9), 11)
end)

testing.registerGlobalTest('memory limit', function()
    local ok, err = pcall(function()
        local t = {}
        local n = 1
        while true do
            t[n] = n
            n = n + 1
        end
    end)
    testing.expectEqual(ok, false, 'Script reaching memory limit should fail')
    testing.expectEqual(err, 'not enough memory')
end)

testing.registerGlobalTest('vfs', function()
    local file = 'test_vfs_dir/lines.txt'
    local nosuchfile = 'test_vfs_dir/nosuchfile'
    testing.expectEqual(vfs.fileExists(file), true, 'lines.txt should exist')
    testing.expectEqual(vfs.fileExists(nosuchfile), false, 'nosuchfile should not exist')

    local expectedLines = { '1', '2', '', '4' }
    local getLine = vfs.lines(file)
    for _,v in pairs(expectedLines) do
        testing.expectEqual(getLine(), v)
    end
    testing.expectEqual(getLine(), nil, 'All lines should have been read')
    local ok = pcall(function()
        vfs.lines(nosuchfile)
    end)
    testing.expectEqual(ok, false, 'Should not be able to read lines from nonexistent file')

    local getPath = vfs.pathsWithPrefix('test_vfs_dir/')
    testing.expectEqual(getPath(), file)
    testing.expectEqual(getPath(), nil, 'All paths should have been read')

    local handle = vfs.open(file)
    testing.expectEqual(vfs.type(handle), 'file', 'File should be open')
    testing.expectEqual(handle.fileName, file)

    local n1, n2, _, l3, l4 = handle:read("*n", "*number", "*l", "*line", "*l")
    testing.expectEqual(n1, 1)
    testing.expectEqual(n2, 2)
    testing.expectEqual(l3, '')
    testing.expectEqual(l4, '4')

    testing.expectEqual(handle:seek('set', 0), 0, 'Reading should happen from the start of the file')
    testing.expectEqual(handle:read("*a"), '1\n2\n\n4')

    testing.expectEqual(handle:close(), true, 'File should be closeable')
    testing.expectEqual(vfs.type(handle), 'closed file', 'File should be closed')

    handle = vfs.open(nosuchfile)
    testing.expectEqual(handle, nil, 'vfs.open should return nil on nonexistent files')

    getLine = vfs.open(file):lines()
    for _,v in pairs(expectedLines) do
        testing.expectEqual(getLine(), v)
    end
end)

testing.registerGlobalTest('commit crime', function()
    local player = initPlayer()
    testing.expectEqual(player == nil, false, 'A viable player reference should exist to run `commit crime`')
    testing.expectEqual(I.Crimes == nil, false, 'Crimes interface should be available in global contexts')

    -- Reset crime level to have a clean slate
    types.Player.setCrimeLevel(player, 0)
    testing.expectEqual(I.Crimes.commitCrime(player, { type = types.Player.OFFENSE_TYPE.Theft, victim = player, arg = 100}).wasCrimeSeen, false, "Running the crime with the player as the victim should not result in a seen crime")
    testing.expectEqual(I.Crimes.commitCrime(player, { type = types.Player.OFFENSE_TYPE.Theft, arg = 50 }).wasCrimeSeen, false, "Running the crime with no victim and a type shouldn't raise errors")
    testing.expectEqual(I.Crimes.commitCrime(player, { type = types.Player.OFFENSE_TYPE.Murder }).wasCrimeSeen, false, "Running a murder crime should work even without a victim")

    -- Create a mockup target for crimes
    local victim = world.createObject(types.NPC.record(player).id)
    victim:teleport(player.cell, player.position + util.vector3(0, 300, 0))
    coroutine.yield()

    -- Reset crime level for testing with a valid victim
    types.Player.setCrimeLevel(player, 0)
    testing.expectEqual(I.Crimes.commitCrime(player, { victim = victim, type = types.Player.OFFENSE_TYPE.Theft, arg = 50 }).wasCrimeSeen, true, "Running a crime with a valid victim should notify them when the player is not sneaking, even if it's not explicitly passed in")
    testing.expectEqual(types.Player.getCrimeLevel(player), 0, "Crime level should not change if the victim's alarm value is low and there's no other witnesses")
end)

testing.registerGlobalTest('record model property', function()
    local player = world.players[1]
    testing.expectEqual(types.NPC.record(player).model, 'meshes/basicplayer.dae')
end)

testing.registerGlobalTest('player approaches and attacks a creature', function()
    local player = initPlayer()
    world.createObject('basic_dagger1h', 1):moveInto(player)
    testing.runLocalTest(player, 'player approaches and attacks a creature')
end)

testing.registerGlobalTest('equipment slots', function()
    local SLOT = types.Actor.EQUIPMENT_SLOT
    local function slotsOf(recordId)
        return types.Item.getEquipmentSlots(world.createObject(recordId, 1))
    end

    local dagger = slotsOf('basic_dagger1h')
    testing.expectEqual(#dagger, 1, 'A dagger should go in one slot')
    testing.expectEqual(dagger[1], SLOT.CarriedRight, 'A dagger should go in the right hand')

    local arrow = world.createRecord(types.Weapon.createRecordDraft({
        template = types.Weapon.record('basic_dagger1h'),
        type = types.Weapon.TYPE.Arrow,
    }))
    local arrowSlots = slotsOf(arrow.id)
    testing.expectEqual(#arrowSlots, 1, 'An arrow should go in one slot')
    testing.expectEqual(arrowSlots[1], SLOT.Ammunition, 'An arrow should go in the ammunition slot')

    local ring = world.createRecord(types.Clothing.createRecordDraft({
        name = 'Test ring',
        type = types.Clothing.TYPE.Ring,
    }))
    local ringSlots = slotsOf(ring.id)
    testing.expectEqual(#ringSlots, 2, 'A ring should go in either ring slot')
    testing.expectEqual(ringSlots[1], SLOT.LeftRing, 'A ring should go on the left hand first')
    testing.expectEqual(ringSlots[2], SLOT.RightRing, 'A ring should go on the right hand next')

    local potion = world.createRecord(types.Potion.createRecordDraft({ name = 'Test potion' }))
    testing.expectEqual(#slotsOf(potion.id), 0, 'A potion should not go in any slot')
end)

testing.registerGlobalTest('equipped weapon damages a stationary target', function()
    local player = initPlayer()
    local target = world.createObject('landracer')
    target:addScript('idle.lua')
    target:teleport(player.cell, player.position + util.vector3(0, 70, 0))
    world.createObject('basic_dagger1h', 1):moveInto(player)
    testing.runLocalTest(player, 'equipped weapon damages a stationary target', target)
    target:remove()
end)

testing.registerGlobalTest('camera.getFocusRay should report the object under the crosshair', function()
    local player = initPlayer()
    local target = world.createObject(types.NPC.record(player).id)
    target:teleport(player.cell, player.position + util.vector3(0, 150, 0), util.transform.rotateZ(math.rad(180)))
    coroutine.yield()
    coroutine.yield()
    -- The spawned NPC dies with the test.
    local ok, err = pcall(testing.runLocalTest, player, 'camera.getFocusRay should report the object under the crosshair')
    target:remove()
    if not ok then
        error(err, 0)
    end
end)

testing.registerGlobalTestStep('load while teleporting - init player', function()
    local player = world.players[1]
    player:teleport('Museum of Wonders', util.vector3(0, -1500, 111), util.transform.rotateZ(math.rad(180)))
end)

testing.registerGlobalTestStep('load while teleporting - teleport', function()
    local player = world.players[1]
    local landracer = world.createObject('landracer')
    landracer:teleport(player.cell, player.position + util.vector3(0, 500, 0))
    coroutine.yield()

    local door = world.getObjectByFormId(core.getFormId('the_hub.omwaddon', 26))
    door:activateBy(player)
    coroutine.yield()

    landracer:teleport(player.cell, player.position)
end)

testing.registerGlobalTest('world.setGameTimeScale should not accept nan', function()
    local nan = 0.0 / 0.0
    local ok, err = pcall(function() world.setGameTimeScale(nan) end)
    testing.expectEqual(ok, false)
    testing.expectEqual(err, 'Value must be a finite number')
end)

testing.registerGlobalTest('weather.stormDirection should not accept a vector with a nan component', function()
    local nan = 0.0 / 0.0
    local ok, err = pcall(function() core.weather.records[1].stormDirection = util.vector3(nan, nan, nan) end)
    testing.expectEqual(ok, false)
    testing.expectEqual(err, 'Vector must contain finite numbers')
end)

testing.registerGlobalTest('mwscript magic interactions', function()
    local player = world.players[1]
    local script = world.mwscript.getGlobalScript('OpenMW_Tests', player)
    testing.expectEqual(script == nil, false, 'Expected mwscript OpenMW_Tests to be active')
    script.variables.state = 1
    local globals = world.mwscript.getGlobalVariables(player);
    globals.OpenMW_Tests_Failed = 0
    while script.variables.state ~= 0 and globals.OpenMW_Tests_Failed == 0 and script.isRunning do
        coroutine.yield()
    end
    testing.expectEqual(script.isRunning, true, 'OpenMW_Tests should not crash')
    testing.expectEqual(globals.OpenMW_Tests_Failed, 0, 'OpenMW_Tests should run without issue')
end)

testing.registerGlobalTest('openmw.util math conventions', function()
    -- Pins the semantics openmw.util exposes to mods, so any change to the
    -- underlying C++ math implementation or to the utilpackage bindings
    -- that alters observable Lua behaviour is caught here rather than
    -- silently breaking existing mods. See components/lua/utilpackage.cpp.

    -- v1 * v2 returns the DOT PRODUCT (a number). Not component-wise.
    local a = util.vector3(1, 2, 3)
    local b = util.vector3(4, 5, 6)
    testing.expectEqual(a * b, 32, 'util.vector3 * util.vector3 must be the dot product')
    testing.expectEqual(a:dot(b), 32, 'util.vector3:dot must equal *')

    -- v:length() returns the MAGNITUDE, not the component count.
    testing.expectEqualWithDelta(util.vector3(3, 4, 0):length(), 5, 1e-5, 'util.vector3:length is magnitude')
    testing.expectEqual(util.vector3(3, 4, 0):length2(), 25, 'util.vector3:length2 is magnitude squared')

    -- Cross product is exposed as :cross and is right-handed.
    local c = util.vector3(1, 0, 0):cross(util.vector3(0, 1, 0))
    testing.expectEqualWithDelta(c.x, 0, 1e-5, 'cross x')
    testing.expectEqualWithDelta(c.y, 0, 1e-5, 'cross y')
    testing.expectEqualWithDelta(c.z, 1, 1e-5, 'cross z (right-handed)')

    -- Vec2 * Vec2 is also the dot product.
    testing.expectEqual(util.vector2(1, 2) * util.vector2(3, 4), 11, 'util.vector2 * util.vector2 must be the dot product')

    -- Transform composition applies in REVERSE order (right-to-left).
    -- Documented at files/lua_api/openmw/util.lua:608-609.
    local rotPi = util.transform.rotateZ(math.pi)
    local move10x = util.transform.move(util.vector3(10, 0, 0))
    local v = util.vector3(1, 0, 0)
    -- (rotPi * move10x) applied to v: move10x first (v -> (11,0,0)), then rotPi (x flips -> -11).
    local moveThenRotate = (rotPi * move10x) * v
    testing.expectEqualWithDelta(moveThenRotate.x, -11, 1e-5, 'composition applies right operand first')
    -- (move10x * rotPi) applied to v: rotPi first ((1,0,0) -> (-1,0,0)), then move10x (+10 -> 9).
    local rotateThenMove = (move10x * rotPi) * v
    testing.expectEqualWithDelta(rotateThenMove.x, 9, 1e-5, 'composition applies right operand first (reverse case)')

    -- Transform:inverse round-trip.
    local roundTrip = rotPi:inverse() * (rotPi * v)
    testing.expectEqualWithDelta(roundTrip.x, 1, 1e-5, 'inverse round-trip x')
    testing.expectEqualWithDelta(roundTrip.y, 0, 1e-5, 'inverse round-trip y')
end)

testing.registerGlobalTest('load script generated static', function()
    local record = types.Static.records.OMW_Generated_Static
    testing.expectNotEqual(record, nil, 'OMW_Generated_Static should have been generated')
    testing.expectEqual(record.model, 'meshes/generatedonload.nif')
end)

testing.registerGlobalTest('load script generated apparatuses', function()
    local apparatus = types.Apparatus.records.OMW_Generated_Apparatus
    testing.expectNotEqual(apparatus, nil)
    testing.expectEqual(apparatus.name, 'Modified apparatus')
    testing.expectEqual(apparatus.model, 'meshes/a/modified.nif')
    testing.expectEqual(apparatus.icon, 'icons/a/modified.dds')
    testing.expectEqual(apparatus.mwscript, nil)
    testing.expectEqual(apparatus.type, types.Apparatus.TYPE.Calcinator)
    testing.expectEqual(apparatus.quality, 10)
    testing.expectEqual(apparatus.weight, 5.5)
    testing.expectEqual(apparatus.value, 450)

    local templated = types.Apparatus.records.OMW_Templated_Apparatus
    testing.expectEqual(templated.name, 'Templated apparatus')
    testing.expectEqual(templated.quality, apparatus.quality)
    local copied = types.Apparatus.records.OMW_Copied_Apparatus
    testing.expectEqual(copied.name, apparatus.name)
    testing.expectEqual(copied.type, apparatus.type)
    testing.expectEqual(types.Apparatus.records.OMW_Temporary_Apparatus, nil)
    testing.expectEqual(types.Apparatus.records.OMW_Invalid_Apparatus, nil)
end)

return {
    engineHandlers = {
        onUpdate = testing.updateGlobal,
    },
    eventHandlers = testing.globalEventHandlers,
}
