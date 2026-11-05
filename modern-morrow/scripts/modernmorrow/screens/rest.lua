-- Resting and waiting. It replaces the engine's WaitDialog with the fork's rest panel
-- (ui-rework/design/other-screens.md in the fork): a panel in the middle of the screen with the hours in large text,
-- when the rest ends, and how long resting until healed takes. Left and right change the hours, LB picks one hour, RB a
-- whole day, A rests and X rests until healed.
--
-- The engine decides whether the player sleeps or only waits, and closes the menu where they can't rest at all. The
-- global script rests through types.Player.rest, which runs the engine's rest with its fade, autosave, interruptions
-- and level-up. While the hours pass, the screen shows its own progress panel in place of the engine's
-- WaitDialogProgressBar window, and fills it from the RestProgress events that the engine sends.

local core = require('openmw.core')
local self = require('openmw.self')
local types = require('openmw.types')
local ui = require('openmw.ui')
local ambient = require('openmw.ambient')
local I = require('openmw.interfaces')
local calendar = require('openmw_aux.calendar')

local style = require('scripts.modernmorrow.ui.style')
local layer = require('scripts.modernmorrow.ui.layer')
local draw = require('scripts.modernmorrow.ui.draw')
local text = require('scripts.modernmorrow.ui.text')
local hints = require('scripts.modernmorrow.ui.hints')
local screen = require('scripts.modernmorrow.ui.screen')
local router = require('scripts.modernmorrow.input.router')

local l10n = core.l10n('ModernMorrow')
local calendarL10n = core.l10n('Calendar')

local rest = {}

local color = style.color
local size = style.textSize

local WIDTH = 560
local HEIGHT = 330
local MAX_HOURS = 24

local function L(key)
    return text.escape(l10n(key))
end

local function gmst(name, fallback)
    return text.escape(core.getGMST(name) or fallback)
end

local element = nil
local state = nil
-- The rest that the player started, until the global script says it is over. Saving the game at the start of the rest
-- closes the menu and opens it again, so this outlives the menu's state. `passed` is nil until the hours start to
-- pass, after the engine's fade.
local running = nil

-- A time of day, such as "6 p.m.".
local function hourText(hour)
    local hour12 = hour % 12
    if hour12 == 0 then
        hour12 = 12
    end
    return calendarL10n('timeFormat', {
        hour = hour,
        hour12 = hour12,
        minute = 0,
        period = calendarL10n(hour >= 12 and 'pm' or 'am'),
    })
end

-- The date and the time now, as the engine's rest menu writes them, such as "16 Last Seed (Day 2) 10 a.m.".
local function dateText()
    local now = calendar.formatGameTime('*t')
    local day = math.floor(core.getGameTime() / (24 * 3600))
    return string.format('%d %s (%s %d) %s', now.day, calendarL10n('monthInGenitive' .. now.month),
        calendarL10n('day'), day, hourText(now.hour))
end

local function hoursText(hours)
    return hours .. ' ' .. L(hours == 1 and 'RestHour' or 'RestHours')
end

-- Whether resting until healed makes sense: the player sleeps here and lacks some health or magicka.
local function canHeal()
    if not state.status.canSleep then
        return false
    end
    local dynamic = types.Actor.stats.dynamic
    for _, stat in ipairs({ dynamic.health(self), dynamic.magicka(self) }) do
        if stat.current < stat.base + stat.modifier then
            return true
        end
    end
    return false
end

local function drawChoice(canvas, x, y, innerX, innerW)
    local sleeping = state.status.canSleep
    canvas:text(innerX, y + 16, innerW, 36, sleeping and gmst('sRest', 'Rest') or gmst('sWait', 'Wait'), size.title,
        color.bright)
    canvas:text(innerX, y + 16, innerW, 36, text.escape(dateText()), size.small, color.dim, ui.ALIGNMENT.End)
    if not sleeping then
        local reason = types.NPC.isWerewolf(self) and core.getGMST('sWerewolfRestMessage')
            or core.getGMST('sRestIllegal')
        screen.lines(canvas, innerX, y + 58, innerW, reason or '', size.normal, color.text, 2, 26)
    end

    -- The hours, with arrows for the d-pad, and when the rest ends.
    local ty = y + 128
    canvas:glyph(innerX, ty + 10, 26, 'dpad')
    canvas:glyph(innerX + innerW - 26, ty + 10, 26, 'dpad')
    canvas:text(innerX, ty, innerW, 46, hoursText(state.hours), size.title, color.active, ui.ALIGNMENT.Center)
    ty = ty + 54
    canvas:rect(innerX, ty, innerW, 8, color.black, 0.6)
    canvas:rect(innerX, ty, math.floor(innerW * state.hours / MAX_HOURS), 8, color.accent)
    ty = ty + 18
    local now = calendar.formatGameTime('*t')
    local endHour = now.hour + state.hours
    local ends = L('RestUntil') .. ' ' .. text.escape(hourText(endHour % 24))
    if endHour >= 24 then
        ends = ends .. ', ' .. L(endHour >= 48 and 'RestInTwoDays' or 'RestTomorrow')
    end
    canvas:text(innerX, ty, innerW, 26, ends, size.normal, color.text, ui.ALIGNMENT.Center)

    -- Resting until healed, when the player is hurt and can sleep here.
    ty = ty + 36
    local heal = canHeal()
    if heal then
        canvas:text(innerX, ty, innerW, 26, L('RestHealedIn') .. ' ' .. hoursText(state.status.hoursUntilHealed),
            size.small, color.dim, ui.ALIGNMENT.Center)
    end

    local list_ = { { 'a', sleeping and gmst('sRest', 'Rest') or gmst('sWait', 'Wait') } }
    if heal then
        list_[#list_ + 1] = { 'x', gmst('sUntilHealed', 'Until Healed') }
    end
    list_[#list_ + 1] = { 'lb', L('RestOneHour') }
    list_[#list_ + 1] = { 'rb', L('RestAllDay') }
    hints.draw(canvas, innerX, innerW, y + HEIGHT - 4, list_)
end

local function layout()
    local layerSize = layer.size(layer.menu)
    local canvas = draw.canvas()
    if state.resting then
        -- The screen stays empty while the engine fades it out, until the hours start to pass.
        local resting = state.resting
        if resting.passed then
            screen.drawProgress(canvas, resting.passed .. ' / ' .. hoursText(resting.hours),
                resting.passed / resting.hours)
        end
    else
        canvas:rect(0, 0, layerSize.x, layerSize.y, color.black, 0.4)
        local x = math.floor((layerSize.x - WIDTH) / 2)
        local y = math.floor((layerSize.y - HEIGHT) / 2)
        canvas:rect(x, y, WIDTH, HEIGHT, color.modal)
        canvas:rect(x, y, WIDTH, 1, color.accent, 0.6)
        drawChoice(canvas, x, y, x + 28, WIDTH - 56)
    end
    local root = canvas:layout({ size = layerSize })
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

local function setHours(hours)
    hours = math.max(1, math.min(MAX_HOURS, hours))
    if hours ~= state.hours then
        state.hours = hours
        ambient.playSound('menu click')
        redraw()
    end
end

local function startRest(hours)
    ambient.playSound('menu click')
    running = { hours = hours }
    state.resting = running
    core.sendGlobalEvent('ModernMorrowRest', { player = self.object, hours = hours })
    redraw()
end

rest.handler = {
    onButton = function(name)
        -- The engine ends the rest, and closes the menu, when the hours have passed or something wakes the player.
        if state == nil or state.resting then
            return
        end
        if name == 'a' then
            startRest(state.hours)
        elseif name == 'x' then
            if canHeal() then
                startRest(state.status.hoursUntilHealed)
            end
        elseif name == 'lb' then
            setHours(1)
        elseif name == 'rb' then
            setHours(MAX_HOURS)
        elseif name == 'b' then
            I.UI.removeMode('Rest')
        end
    end,
    onMove = function(dx, _)
        if state == nil or state.resting or dx == 0 then
            return
        end
        setHours(state.hours + dx)
    end,
}

-- From the global script, when the rest is over. A rest that never started, such as one the engine refused, goes back
-- to choosing the hours.
function rest.onResult(finished)
    local stopped = running
    running = nil
    if state == nil or state.resting ~= stopped then
        return
    end
    if not finished and stopped.passed == nil then
        state.resting = nil
        -- Event handlers may run on the Lua worker thread, which can't measure text, so the next frame redraws.
        state.dirty = true
    end
end

local function show(bed)
    state = { hours = 1, status = types.Player.getRestStatus(self, bed), resting = running }
    router.push(rest.handler)
    if element then
        element:destroy()
    end
    element = ui.create(layout())
end

local function hide()
    router.remove(rest.handler)
    if element then
        element:destroy()
        element = nil
    end
    state = nil
    -- The menu closes for good once the hours have started to pass, when the rest ends.
    if running and running.passed then
        running = nil
    end
end

-- From the RestProgress event, when the hours start to pass and after each hour. A rest that a script started, rather
-- than this screen, shows its progress too.
function rest.onProgress(data)
    if running == nil then
        running = { hours = data.hours }
    end
    running.hours = data.hours
    running.passed = data.hoursPassed
    if state then
        state.resting = running
        -- Event handlers may run on the Lua worker thread, which can't measure text, so the next frame redraws.
        state.dirty = true
    end
end

function rest.onFrame()
    if element == nil or state == nil then
        return
    end
    if state.dirty then
        state.dirty = false
        redraw()
    end
end

function rest.register()
    -- The engine opens the menu with the bed that the player uses, if any.
    I.UI.registerWindow('WaitDialog', show, hide, { capturesInput = true })
    -- The progress panel takes the place of the engine's progress bar, so its replacement draws nothing itself.
    I.UI.registerWindow('WaitDialogProgressBar', function() end, function() end)
end

return rest
