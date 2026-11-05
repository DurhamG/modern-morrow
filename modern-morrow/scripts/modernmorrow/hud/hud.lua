-- The HUD, as the fork redraws it: one compact group in the bottom left corner, sized relative to the screen height.
-- The health, magicka and fatigue bars sit above the icons of the spell effects on the player, with the health bar of
-- the last actor the player hit above them. The weapon, spell and sneak boxes sit to the right of the bars.
--
-- Elements fade out when they have nothing to tell: a bar while it is full, the weapon and spell boxes a few seconds
-- after the weapon or spell changed, unless it is ready, and the effect icons while there are none. During combat, the
-- bars and both boxes stay on screen. Faded elements keep their place, so the rest doesn't move.
--
-- The mod hides the engine's own versions of these elements, and asks the engine when it would show some of them: the
-- enemy's bar follows the actor and the time of the engine's enemy health bar, and the sneak box shows while the
-- engine's sneak indicator would, while the player sneaks unseen. The minimap with the cell name, and the breath meter,
-- stay the engine's. The whole HUD hides while a menu or a conversation is open, as in the fork.

local core = require('openmw.core')
local self = require('openmw.self')
local types = require('openmw.types')
local ui = require('openmw.ui')
local util = require('openmw.util')
local I = require('openmw.interfaces')

local style = require('scripts.modernmorrow.ui.style')
local layer = require('scripts.modernmorrow.ui.layer')
local draw = require('scripts.modernmorrow.ui.draw')
local castables = require('scripts.modernmorrow.magic.castables')
local info = require('scripts.modernmorrow.items.info')

local hud = {}

local color = style.color
local v2 = util.vector2

-- The elements the mod draws itself.
local REPLACED = { 'DynamicStats', 'EnemyHealth', 'Weapon', 'Spell', 'WeaponSpellName', 'Sneak', 'ActiveEffects' }
-- The elements that stay the engine's, but hide in menus.
local KEPT = { 'Minimap', 'CellName', 'DrowningBar' }

local LEFT = 24
local BOTTOM = 696
local BAR_W = 192
local BAR_H = 20
local BAR_GAP = 4
local ENEMY_H = 14
local ICON = 36
local ICON_GAP = 4
local BOX = 64
local BOX_BAR_H = 8
local BOX_GAP = 8

local FADE_TIME = 0.4
-- How long the weapon and spell boxes stay after a change, in seconds.
local CHANGE_TIME = 5

local BARS = {
    { stat = 'health', color = util.color.rgb(0.78, 0.16, 0.12) },
    { stat = 'magicka', color = util.color.rgb(0.22, 0.36, 0.86) },
    { stat = 'fatigue', color = util.color.rgb(0.20, 0.62, 0.22) },
}
local ENEMY_COLOR = util.color.rgb(0.85, 0.70, 0.18)
local CONDITION_COLOR = util.color.rgb(0.78, 0.22, 0.16)
local CHARGE_COLOR = util.color.rgb(0.40, 0.36, 0.86)
local BOX_BACK = util.color.rgb(0, 0, 0)

local HAND_TO_HAND_ICON = 'icons/k/stealth_handtohand.dds'
local WEREWOLF_HAND_ICON = 'icons/k/tx_werewolf_hand.dds'
local SNEAK_ICON = 'icons/k/stealth_sneak.dds'

local element = nil
local shownKey = nil
-- Whether the engine's elements need hiding again, as after a game is loaded.
local needsSetup = true
local keptHidden = false

-- How visible each element is, from 0 to 1, keyed by name.
local alphas = {}

-- The actors who fight the player, by their id, from the built-in music script's event.
local fighting = {}

-- What the weapon and spell boxes showed last, and how long they stay after a change.
local lastWeapon = nil
local lastSpell = nil
local weaponTimer = 0
local spellTimer = 0
-- Changes in the first moments after a game is loaded don't count, while the engine sets up the player.
local settleTime = 0
local SETTLE_TIME = 1

local function fade(name, visible, dt)
    local alpha = alphas[name] or 0
    local step = dt / FADE_TIME
    if visible then
        alpha = math.min(1, alpha + step)
    else
        alpha = math.max(0, alpha - step)
    end
    alphas[name] = alpha
    return alpha
end

local function ratio(stat)
    local full = stat.base + stat.modifier
    if full <= 0 then
        return 0
    end
    return math.max(0, math.min(1, stat.current / full))
end

-- The effects on the player that run out or that the player should notice, such as diseases. Abilities and worn gear
-- with a constant effect are left out, as in the fork. One icon per effect.
local function effectIcons()
    local icons = {}
    local seen = {}
    for _, active in pairs(types.Actor.activeSpells(self)) do
        local spell = core.magic.spells.records[active.id]
        local ability = spell ~= nil and spell.type == core.magic.SPELL_TYPE.Ability
        if not active.fromEquipment and not ability then
            for _, effect in ipairs(active.effects) do
                local record = core.magic.effects.records[effect.id]
                if record and record.icon and record.icon ~= '' and not seen[effect.id] then
                    seen[effect.id] = true
                    icons[#icons + 1] = record.icon
                end
            end
        end
    end
    table.sort(icons)
    return icons
end

-- What the weapon box shows: the equipped weapon and its condition, or the hand-to-hand icon.
local function weaponState()
    local weapon = types.Actor.getEquipment(self, types.Actor.EQUIPMENT_SLOT.CarriedRight)
    if weapon and types.Weapon.objectIsInstance(weapon) then
        local health = types.Weapon.record(weapon).health
        local condition = info.condition(weapon)
        local fraction = 1
        if health and health > 0 and condition then
            fraction = condition / health
        end
        return { key = weapon.recordId, item = weapon, fraction = fraction }
    end
    local ok, werewolf = pcall(types.NPC.isWerewolf, self)
    return { key = 'hand', icon = (ok and werewolf) and WEREWOLF_HAND_ICON or HAND_TO_HAND_ICON, fraction = 0 }
end

-- What the spell box shows: the selected spell and the chance to cast it, or the enchanted item and its charge.
local function spellState()
    local item = types.Actor.getSelectedEnchantedItem(self)
    if item then
        local record = item.type.record(item)
        local enchantment = record.enchant and core.magic.enchantments.records[record.enchant]
        local fraction = 0
        if enchantment then
            local maxCharge = core.magic.enchantments.getMaxCharge(enchantment)
            local data = types.Item.itemData(item)
            -- An item whose charge was never used has a full charge.
            local charge = data and data.enchantmentCharge or maxCharge
            if maxCharge > 0 then
                fraction = charge / maxCharge
            end
        end
        return { key = 'item:' .. item.recordId, item = item, fraction = fraction }
    end
    local spell = types.Actor.getSelectedSpell(self)
    if spell then
        local first = spell.effects[1]
        local chance = types.Actor.spells(self):getSuccessChance(spell)
        return {
            key = 'spell:' .. spell.id,
            icon = first and castables.bigIcon(first.effect),
            fraction = math.max(0, math.min(100, chance)) / 100,
        }
    end
    return { key = 'none', fraction = 0 }
end

local function drawBar(canvas, x, y, w, h, fraction, fill, alpha)
    -- A thin gold frame around a dark track, as the boxes have.
    canvas:rect(x, y, w, h, color.accent, 0.45 * alpha)
    canvas:rect(x + 1, y + 1, w - 2, h - 2, color.black, 0.8 * alpha)
    local filled = math.floor((w - 4) * fraction + 0.5)
    if filled > 0 then
        canvas:rect(x + 2, y + 2, filled, h - 4, fill, 0.95 * alpha)
    end
end

local function drawBox(canvas, x, y, alpha)
    canvas:rect(x, y, BOX, BOX, BOX_BACK, 0.55 * alpha)
    canvas:rect(x, y, BOX, 1, color.accent, 0.5 * alpha)
    canvas:rect(x, y + BOX - 1, BOX, 1, color.accent, 0.5 * alpha)
    canvas:rect(x, y, 1, BOX, color.accent, 0.5 * alpha)
    canvas:rect(x + BOX - 1, y, 1, BOX, color.accent, 0.5 * alpha)
end

local function drawPicture(canvas, x, y, path, alpha)
    canvas:image(x + 6, y + 6, BOX - 12, BOX - 12, draw.texture(path), nil, alpha)
end

-- Draws an item's icon in a box, with the background the game uses for magic items.
local function drawItem(canvas, x, y, item, alpha)
    local record = item.type.record(item)
    if record.enchant and record.enchant ~= '' then
        canvas:image(x + 2, y + 2, BOX - 4, BOX - 4,
            draw.texture('textures/menu_icon_magic.dds', v2(0, 0), v2(44, 44)), nil, alpha)
    end
    if record.icon and record.icon ~= '' then
        drawPicture(canvas, x, y, record.icon, alpha)
    end
end

local function layout(view)
    local canvas = draw.canvas()

    local effectsY = BOTTOM - ICON
    if view.effectsAlpha > 0 then
        for i, icon in ipairs(view.effects) do
            local x = LEFT + (i - 1) * (ICON + ICON_GAP)
            canvas:rect(x, effectsY, ICON, ICON, BOX_BACK, 0.45 * view.effectsAlpha)
            canvas:image(x + 2, effectsY + 2, ICON - 4, ICON - 4, draw.texture(icon), nil, view.effectsAlpha)
        end
    end

    local y = effectsY - 16 - BAR_H
    for i = #BARS, 1, -1 do
        local bar = view.bars[i]
        if bar.alpha > 0 then
            drawBar(canvas, LEFT, y, BAR_W, BAR_H, bar.fraction, BARS[i].color, bar.alpha)
        end
        y = y - BAR_GAP - BAR_H
    end
    y = y + BAR_H - ENEMY_H
    if view.enemyAlpha > 0 then
        drawBar(canvas, LEFT, y, BAR_W, ENEMY_H, view.enemyFraction, ENEMY_COLOR, view.enemyAlpha)
    end

    local boxY = BOTTOM - BOX_BAR_H - 3 - BOX
    local x = LEFT + BAR_W + 12
    if view.weaponAlpha > 0 then
        local a = view.weaponAlpha
        drawBox(canvas, x, boxY, a)
        if view.weapon.item then
            drawItem(canvas, x, boxY, view.weapon.item, a)
        else
            drawPicture(canvas, x, boxY, view.weapon.icon, a)
        end
        drawBar(canvas, x, BOTTOM - BOX_BAR_H, BOX, BOX_BAR_H, view.weapon.fraction, CONDITION_COLOR, a)
    end
    x = x + BOX + BOX_GAP
    if view.spellAlpha > 0 then
        local a = view.spellAlpha
        drawBox(canvas, x, boxY, a)
        if view.spell.item then
            drawItem(canvas, x, boxY, view.spell.item, a)
        elseif view.spell.icon then
            drawPicture(canvas, x, boxY, view.spell.icon, a)
        end
        drawBar(canvas, x, BOTTOM - BOX_BAR_H, BOX, BOX_BAR_H, view.spell.fraction, CHARGE_COLOR, a)
    end
    x = x + BOX + BOX_GAP
    if view.sneakAlpha > 0 then
        drawBox(canvas, x, boxY, view.sneakAlpha)
        drawPicture(canvas, x, boxY, SNEAK_ICON, view.sneakAlpha)
    end

    local root = canvas:layout({ size = layer.size(layer.hud) })
    root.layer = layer.hud
    return root
end

-- A string that changes when the drawing would, with the fades in steps of a twentieth.
local function keyOf(view)
    local function a(alpha)
        return math.floor(alpha * 20 + 0.5)
    end
    local parts = { a(view.effectsAlpha), table.concat(view.effects, ','), a(view.enemyAlpha),
        math.floor(view.enemyFraction * 200), a(view.weaponAlpha), view.weapon.key,
        math.floor(view.weapon.fraction * 100), a(view.spellAlpha), view.spell.key,
        math.floor(view.spell.fraction * 100), a(view.sneakAlpha) }
    for _, bar in ipairs(view.bars) do
        parts[#parts + 1] = a(bar.alpha) .. ':' .. math.floor(bar.fraction * 200)
    end
    return table.concat(parts, '|')
end

local function setElementsHidden(names, hidden)
    for _, name in ipairs(names) do
        I.UI.setHudElementVisibility(I.UI.HUD_ELEMENT[name], not hidden)
    end
end

local function hideAll()
    if element and shownKey ~= 'hidden' then
        element.layout.props.visible = false
        element:update()
        shownKey = 'hidden'
    end
end

-- From the player script's onFrame.
function hud.onFrame()
    if needsSetup then
        needsSetup = false
        keptHidden = false
        setElementsHidden(REPLACED, true)
    end
    -- In menus and conversations, the engine's minimap and the rest go too.
    local inMenu = I.UI.getMode() ~= nil
    if inMenu ~= keptHidden then
        keptHidden = inMenu
        setElementsHidden(KEPT, inMenu)
    end
    if inMenu or not I.UI.isHudVisible() then
        hideAll()
        return
    end

    local dt = core.getRealFrameDuration()
    local stance = types.Actor.getStance(self)

    -- The actor whose health the engine's enemy bar shows, and how long until the engine hides it.
    local enemy, enemyTimeLeft = I.UI.getHudEnemy()
    if enemy and not enemy:isValid() then
        enemy = nil
    end
    local enemyFraction = 0
    if enemy then
        enemyFraction = ratio(types.Actor.stats.dynamic.health(enemy))
    end
    local inCombat = next(fighting) ~= nil or enemy ~= nil

    local view = { bars = {}, enemyFraction = enemyFraction }
    for i, bar in ipairs(BARS) do
        local fraction = ratio(types.Actor.stats.dynamic[bar.stat](self))
        view.bars[i] = { fraction = fraction, alpha = fade(bar.stat, fraction < 1 or inCombat, dt) }
    end
    -- The enemy's bar fades out at the end of its time, over the time the game settings give, like the engine's.
    local fadeTime = core.getGMST('fNPCHealthBarFade') or 0.5
    local enemyAlpha = 0
    if enemy then
        enemyAlpha = fadeTime > 0 and math.max(0, math.min(1, enemyTimeLeft / fadeTime)) or 1
    end
    view.enemyAlpha = enemyAlpha

    view.weapon = weaponState()
    if view.weapon.key ~= lastWeapon then
        if lastWeapon ~= nil then
            weaponTimer = CHANGE_TIME
        end
        lastWeapon = view.weapon.key
    end
    view.spell = spellState()
    if view.spell.key ~= lastSpell then
        if lastSpell ~= nil then
            spellTimer = CHANGE_TIME
        end
        lastSpell = view.spell.key
    end
    if settleTime > 0 then
        settleTime = settleTime - dt
        weaponTimer, spellTimer = 0, 0
    end
    weaponTimer = math.max(0, weaponTimer - dt)
    spellTimer = math.max(0, spellTimer - dt)
    view.weaponAlpha = fade('weapon', weaponTimer > 0 or stance == types.Actor.STANCE.Weapon or inCombat, dt)
    view.spellAlpha = fade('spell', spellTimer > 0 or stance == types.Actor.STANCE.Spell or inCombat, dt)
    view.sneakAlpha = fade('sneak', I.UI.isHudElementWanted(I.UI.HUD_ELEMENT.Sneak), dt)
    view.effects = effectIcons()
    view.effectsAlpha = fade('effects', #view.effects > 0, dt)

    local key = keyOf(view)
    if key == shownKey then
        return
    end
    shownKey = key
    if element then
        element.layout = layout(view)
        element:update()
    else
        element = ui.create(layout(view))
    end
end

-- From the player script's onInit and onLoad. The engine shows all its HUD elements again when a game starts or loads.
function hud.reset()
    needsSetup = true
    fighting = {}
    lastWeapon = nil
    lastSpell = nil
    settleTime = SETTLE_TIME
end

-- From the built-in music script's actor event, which says whom an actor fights.
function hud.onCombatTargetsChanged(data)
    if data == nil or data.actor == nil then
        return
    end
    local fightsPlayer = false
    for _, target in pairs(data.targets or {}) do
        fightsPlayer = fightsPlayer or target.id == self.object.id
    end
    fighting[data.actor.id] = fightsPlayer or nil
end

return hud
