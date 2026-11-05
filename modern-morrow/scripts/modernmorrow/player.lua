-- The player script of the Modern Morrow mod: it sets up the layers, registers the screens and passes the
-- controller input on.

local ui = require('openmw.ui')

local layer = require('scripts.modernmorrow.ui.layer')
local router = require('scripts.modernmorrow.input.router')
local hub = require('scripts.modernmorrow.screens.hub')
local recentItems = require('scripts.modernmorrow.items.recent')
local recentSpells = require('scripts.modernmorrow.magic.recent')
local inventory = require('scripts.modernmorrow.screens.inventory')
local magic = require('scripts.modernmorrow.screens.magic')
local stats = require('scripts.modernmorrow.screens.stats')
local journal = require('scripts.modernmorrow.screens.journal')
local reading = require('scripts.modernmorrow.screens.reading')
local barter = require('scripts.modernmorrow.screens.barter')
local loot = require('scripts.modernmorrow.screens.loot')
local companion = require('scripts.modernmorrow.screens.companion')
local services = require('scripts.modernmorrow.screens.services')
local tools = require('scripts.modernmorrow.screens.tools')
local alchemy = require('scripts.modernmorrow.screens.alchemy')
local effectcraft = require('scripts.modernmorrow.screens.effectcraft')
local rest = require('scripts.modernmorrow.screens.rest')
local levelup = require('scripts.modernmorrow.screens.levelup')
local jail = require('scripts.modernmorrow.screens.jail')
local dialogue = require('scripts.modernmorrow.screens.dialogue')
local messages = require('scripts.modernmorrow.screens.messages')
local quests = require('scripts.modernmorrow.journal.quests')
local question = require('scripts.modernmorrow.ui.question')
local keyboard = require('scripts.modernmorrow.ui.keyboard')
local chargenFlow = require('scripts.modernmorrow.chargen.flow')
local chargenName = require('scripts.modernmorrow.chargen.name')
local chargenRace = require('scripts.modernmorrow.chargen.race')
local chargenClasses = require('scripts.modernmorrow.chargen.classes')
local chargenCreateClass = require('scripts.modernmorrow.chargen.createclass')
local chargenBirth = require('scripts.modernmorrow.chargen.birth')
local chargenReview = require('scripts.modernmorrow.chargen.review')
local framing = require('scripts.modernmorrow.camera.framing')
local hud = require('scripts.modernmorrow.hud.hud')

layer.setup()

-- The triggers switch between the screens in this order.
hub.register(inventory)
hub.register(magic)
hub.register(stats)
hub.register(journal)
reading.register()
barter.register()
loot.register()
companion.register()
services.register()
tools.register()
alchemy.register()
effectcraft.register()
rest.register()
levelup.register()
jail.register()
dialogue.register()
messages.register()
chargenName.register()
chargenRace.register()
chargenClasses.register()
chargenCreateClass.register()
chargenBirth.register()
chargenReview.register()

local function onFrame()
    recentSpells.update()
    quests.update()
    inventory.onFrame()
    magic.onFrame()
    stats.onFrame()
    journal.onFrame()
    barter.onFrame()
    loot.onFrame()
    companion.onFrame()
    services.onFrame()
    tools.onFrame()
    alchemy.onFrame()
    effectcraft.onFrame()
    rest.onFrame()
    jail.onFrame()
    dialogue.onFrame()
    chargenClasses.onFrame()
    chargenReview.onFrame()
    question.onFrame()
    keyboard.onFrame()
    messages.onFrame()
    framing.onFrame()
    hud.onFrame()
    router.onFrame()
end

return {
    engineHandlers = {
        onInit = function()
            hud.reset()
            recentItems.load(nil)
            recentSpells.load(nil)
            chargenFlow.load(nil)
        end,
        onLoad = function(data)
            recentItems.load(data and data.recent)
            recentSpells.load(data and data.recentSpells)
            quests.reset()
            chargenFlow.load(data and data.chargen or {})
            inventory.reset()
            magic.reset()
            stats.reset()
            journal.reset()
            hub.reset()
            hud.reset()
        end,
        onSave = function()
            return {
                recent = recentItems.save(),
                recentSpells = recentSpells.save(),
                chargen = chargenFlow.save(),
            }
        end,
        onFrame = onFrame,
        onUpdate = framing.onUpdate,
        onControllerButtonPress = router.onButtonPress,
        onItemAdded = function(item, count)
            recentItems.onItemAdded(item, count)
            loot.onItemAdded(item)
        end,
    },
    eventHandlers = {
        -- Opens the menus on one screen, given by the name of the window it replaces, such as 'Magic'. The harness
        -- and the console use it too.
        ModernMorrowOpen = function(window)
            hub.open(window)
        end,
        ModernMorrowBarterResult = barter.onResult,
        ModernMorrowPickpocketResult = loot.onPickpocketResult,
        ModernMorrowCompanionProfit = companion.onProfit,
        ModernMorrowCellNames = quests.onCellNames,
        ModernMorrowBrewResult = alchemy.onBrewResult,
        ModernMorrowEnchantResult = effectcraft.onEnchantResult,
        ModernMorrowRestResult = rest.onResult,
        -- The engine's events while the hours of a rest and the days of a jail term pass.
        RestProgress = rest.onProgress,
        JailProgress = jail.onProgress,
        -- A message from the global script, such as that a corpse can't be disposed of.
        ModernMorrowShowMessage = function(message)
            ui.showMessage(message)
        end,
        ModernMorrowOpenInventory = function()
            hub.open('Inventory')
        end,
        -- The built-in music script's actors say whom they fight. The HUD keeps its bars on screen during combat.
        OMWMusicCombatTargetsChanged = hud.onCombatTargetsChanged,
    },
}
