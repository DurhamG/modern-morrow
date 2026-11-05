-- The stats screen. The Character tab shows the character at a glance as tiles, and the Skills and Factions tabs list
-- the rest. A details panel describes whatever is selected, such as how levelling works or what the next faction rank
-- needs, so the player never needs a tooltip.
-- It follows the fork's design in ui-rework/design/stats.md.

local core = require('openmw.core')
local self = require('openmw.self')
local types = require('openmw.types')
local ui = require('openmw.ui')
local util = require('openmw.util')
local ambient = require('openmw.ambient')

local style = require('scripts.modernmorrow.ui.style')
local layer = require('scripts.modernmorrow.ui.layer')
local draw = require('scripts.modernmorrow.ui.draw')
local text = require('scripts.modernmorrow.ui.text')
local hints = require('scripts.modernmorrow.ui.hints')
local list = require('scripts.modernmorrow.ui.list')
local tabs = require('scripts.modernmorrow.ui.tabs')
local hub = require('scripts.modernmorrow.screens.hub')

local l10n = core.l10n('ModernMorrow')

local statsScreen = { window = 'Stats' }

local color = style.color
local size = style.textSize
local space = style.space

local ENTRY_HEIGHT = 50
local HEADER_ROW_HEIGHT = 28
local TILE_GAP = 12
-- How often the screen checks for changes, such as fatigue recovering, in seconds.
local REFRESH_INTERVAL = 0.25

-- The colours of the HUD's health, magicka and fatigue bars.
local BAR_COLORS = {
    health = util.color.rgb(0.78, 0.24, 0.12),
    magicka = util.color.rgb(0.21, 0.33, 0.68),
    fatigue = util.color.rgb(0, 0.59, 0.24),
}
local DYNAMIC = {
    health = { gmst = 'sHealth', fallback = 'Health', description = 'sHealthDesc' },
    magicka = { gmst = 'sMagic', fallback = 'Magicka', description = 'sMagDesc' },
    fatigue = { gmst = 'sFatigue', fallback = 'Fatigue', description = 'sFatDesc' },
}

local TAB = { Character = 1, Skills = 2, Factions = 3 }
local TAB_KEYS = { 'StatsTabCharacter', 'StatsTabSkills', 'StatsTabFactions' }

local SPELL_TYPE = core.magic.SPELL_TYPE

local function L(key)
    return text.escape(l10n(key))
end

local function gmstText(name, fallback)
    return core.getGMST(name) or fallback or ''
end

local function gmst(name, fallback)
    return text.escape(gmstText(name, fallback))
end

-- ---------------------------------------------------------------------------------------------------------------
-- What the screen shows

local function playerRecord()
    return types.NPC.record(self)
end

local function playerClass()
    return types.NPC.classes.records[playerRecord().class]
end

local function playerRace()
    return types.NPC.races.records[playerRecord().race]
end

local function birthSign()
    local id = types.Player.getBirthSign(self)
    if id == nil or id == '' then
        return nil
    end
    return types.Player.birthSigns.records[id]
end

local function attributeStat(id)
    return types.Actor.stats.attributes[id](self)
end

local function skillStat(id)
    return types.NPC.stats.skills[id](self)
end

local function attributeName(id)
    local record = id and core.stats.Attribute.records[id]
    return record and record.name or ''
end

local function skillName(id)
    local record = id and core.stats.Skill.records[id]
    return record and record.name or ''
end

local function dynamicStat(kind)
    local stat = types.Actor.stats.dynamic[kind](self)
    return stat.current, stat.base + stat.modifier
end

-- Green when a stat is raised above its base value, and red when it is lowered.
local function valueColor(modified, base)
    if modified > base then
        return color.better
    elseif modified < base then
        return color.worse
    end
    return color.bright
end

local function levelUpTotal()
    return core.getGMST('iLevelUpTotal') or 10
end

-- The bonus an attribute gets at the next level up, from the skills raised since the last one, such as 3 for "x3".
-- The engine's level-up rule never gives less than one point.
local function levelUpBonus(levelInfo, id)
    return math.max(1, levelInfo.attributeIncreases[id] or 1)
end

local function skillPercent(id)
    return math.floor(skillStat(id).progress * 100 + 0.5)
end

local function specializationName(specialization)
    if specialization == nil or specialization == '' then
        return ''
    end
    local key = 'sSpecialization' .. specialization:sub(1, 1):upper() .. specialization:sub(2)
    return gmstText(key, specialization)
end

-- The skills of the Skills tab's groups: the class's major and minor skills, then the rest, each in name order.
local function skillGroups()
    local class = playerClass()
    local groups = { {}, {}, {} }
    local grouped = {}
    for i, skills in ipairs({ class.majorSkills, class.minorSkills }) do
        for _, id in ipairs(skills) do
            groups[i][#groups[i] + 1] = id
            grouped[id] = true
        end
    end
    for _, skill in ipairs(core.stats.Skill.records) do
        if not grouped[skill.id] then
            groups[3][#groups[3] + 1] = skill.id
        end
    end
    for _, group in ipairs(groups) do
        table.sort(group, function(a, b)
            return skillName(a) < skillName(b)
        end)
    end
    return groups
end

local function isLevelSkill(id)
    local class = playerClass()
    for _, skills in ipairs({ class.majorSkills, class.minorSkills }) do
        for _, skill in ipairs(skills) do
            if skill == id then
                return true
            end
        end
    end
    return false
end

-- The factions the player belongs to, in name order. Hidden factions don't show, as in the engine's stats window.
local function playerFactions()
    local result = {}
    for _, id in ipairs(types.NPC.getFactions(self)) do
        local faction = core.factions.records[id]
        if faction and not faction.hidden then
            result[#result + 1] = faction
        end
    end
    table.sort(result, function(a, b)
        return a.name < b.name
    end)
    return result
end

local function rankCount(faction)
    local count = 0
    for _, rank in ipairs(faction.ranks) do
        if rank.name ~= '' then
            count = count + 1
        end
    end
    return count
end

local function spellTypeLabel(spell)
    if spell.type == SPELL_TYPE.Ability then
        return l10n('MagicSourceAbility')
    elseif spell.type == SPELL_TYPE.Power then
        return l10n('MagicSourcePower')
    end
    return l10n('MagicSourceSpell')
end

local function currentSignature()
    local level = types.Actor.stats.level(self)
    local levelInfo = types.Player.getLevelUpInfo(self)
    local parts = { level.current, level.progress }
    for _, kind in ipairs({ 'health', 'magicka', 'fatigue' }) do
        local current, max = dynamicStat(kind)
        parts[#parts + 1] = math.floor(current) .. ',' .. math.floor(max)
    end
    for _, attribute in ipairs(core.stats.Attribute.records) do
        parts[#parts + 1] = math.floor(attributeStat(attribute.id).modified) .. 'x'
            .. levelUpBonus(levelInfo, attribute.id)
    end
    for _, skill in ipairs(core.stats.Skill.records) do
        parts[#parts + 1] = math.floor(skillStat(skill.id).modified) .. ',' .. skillPercent(skill.id)
    end
    for _, id in ipairs(types.NPC.getFactions(self)) do
        parts[#parts + 1] = id .. types.NPC.getFactionRank(self, id)
            .. (types.NPC.isExpelled(self, id) and 'x' or '')
    end
    parts[#parts + 1] = types.Player.getCrimeLevel(self)
    parts[#parts + 1] = types.NPC.stats.reputation(self).current
    return table.concat(parts, '/')
end

-- ---------------------------------------------------------------------------------------------------------------
-- State

local element = nil
local signature = ''
local refreshAt = 0

local state = {}

local function resetState()
    state.tab = TAB.Character
    state.lists = { list.new(), list.new(), list.new() }
    -- The tile selected on the Character tab.
    state.tile = 1
end
resetState()

-- The Character tab's tiles, as `{ info, x, y, w, h }`. An info is a table with a `kind`, and an `id` for attributes,
-- skills and factions.
local function characterTiles(width)
    local top = space.contentTop
    local panelW = math.max(320, math.floor(width * 0.36))
    local leftW = width - 2 * space.margin - panelW - 16
    local x = space.margin
    local tiles = {}
    local y = top + 38
    local function row(infos, h)
        local tileW = math.floor((leftW - (#infos - 1) * TILE_GAP) / #infos)
        for i, info in ipairs(infos) do
            tiles[#tiles + 1] = { info = info, x = x + (i - 1) * (tileW + TILE_GAP), y = y, w = tileW, h = h }
        end
        y = y + h + TILE_GAP
    end
    row({ { kind = 'race' }, { kind = 'class' }, { kind = 'birthSign' } }, 64)
    row({ { kind = 'level' } }, 70)
    row({ { kind = 'health' }, { kind = 'magicka' }, { kind = 'fatigue' } }, 70)
    local attributesY = y
    y = y + 22
    local attributes = {}
    for _, attribute in ipairs(core.stats.Attribute.records) do
        attributes[#attributes + 1] = { kind = 'attribute', id = attribute.id }
    end
    local half = math.floor(#attributes / 2)
    row({ table.unpack(attributes, 1, half) }, 84)
    row({ table.unpack(attributes, half + 1) }, 84)
    return tiles, attributesY
end

-- The entries of the Skills or the Factions tab, with group headers.
local function buildRows()
    local rows = {}
    if state.tab == TAB.Skills then
        local headers = {
            gmst('sSkillClassMajor', 'Major Skills'),
            gmst('sSkillClassMinor', 'Minor Skills'),
            gmst('sSkillClassMisc', 'Misc Skills'),
        }
        for i, group in ipairs(skillGroups()) do
            if #group > 0 then
                rows[#rows + 1] = { header = headers[i] }
                for _, id in ipairs(group) do
                    rows[#rows + 1] = { info = { kind = 'skill', id = id } }
                end
            end
        end
        return rows
    end
    rows[#rows + 1] = { header = L('StatsStanding') }
    rows[#rows + 1] = { info = { kind = 'reputation' } }
    rows[#rows + 1] = { info = { kind = 'bounty' } }
    rows[#rows + 1] = { header = gmst('sFaction', 'Faction') }
    local factions = playerFactions()
    if #factions == 0 then
        rows[#rows + 1] = { note = L('StatsNoFactions') }
    end
    for _, faction in ipairs(factions) do
        rows[#rows + 1] = { info = { kind = 'faction', id = faction.id } }
    end
    return rows
end

-- ---------------------------------------------------------------------------------------------------------------
-- Drawing

local function drawTile(canvas, tile, focused, levelInfo)
    local x, y, w, h = tile.x, tile.y, tile.w, tile.h
    local info = tile.info
    canvas:rect(x, y, w, h, color.panel, 0.9)
    if focused then
        canvas:rect(x, y, w, h, color.accent, 0.16)
        canvas:rect(x, y, 3, h, color.accent)
    end
    local nameColor = focused and color.active or color.bright
    local kind = info.kind
    if kind == 'race' or kind == 'class' or kind == 'birthSign' then
        local label, record
        if kind == 'race' then
            label, record = gmst('sRace', 'Race'), playerRace()
        elseif kind == 'class' then
            label, record = gmst('sClass', 'Class'), playerClass()
        else
            label, record = gmst('sBirthSign', 'Sign'), birthSign()
        end
        canvas:text(x + 14, y + 6, w - 28, 22, label, size.small, color.dim)
        canvas:text(x + 14, y + 28, w - 28, 28, record and text.fit(record.name, w - 28, size.normal) or '',
            size.normal, nameColor)
    elseif kind == 'level' then
        local level = types.Actor.stats.level(self)
        canvas:text(x + 14, y + 6, 120, 22, gmst('sLevel', 'Level'), size.small, color.dim)
        canvas:text(x + 14, y + 26, 120, 36, tostring(level.current), size.title, nameColor)
        -- Progress toward the next level, in skill increases.
        local barX = x + 140
        local barW = w - 140 - 16
        local total = levelUpTotal()
        canvas:text(barX, y + 10, barW, 24, string.format('%d / %d  %s', level.progress, total,
            L('StatsSkillIncreases')), size.small, color.text)
        canvas:bar(barX, y + 40, barW, 10, total > 0 and level.progress / total or 0, color.accent)
    elseif DYNAMIC[kind] then
        local current, max = dynamicStat(kind)
        canvas:text(x + 14, y + 8, w - 28, 22, gmst(DYNAMIC[kind].gmst, DYNAMIC[kind].fallback), size.small,
            color.dim)
        canvas:text(x + 14, y + 8, w - 28, 22, string.format('%d / %d', math.floor(current), math.floor(max)),
            size.normal, nameColor, ui.ALIGNMENT.End)
        canvas:bar(x + 14, y + 42, w - 28, 12, max > 0 and current / max or 0, BAR_COLORS[kind])
    elseif kind == 'attribute' then
        local record = core.stats.Attribute.records[info.id]
        local stat = attributeStat(info.id)
        if record.icon and record.icon ~= '' then
            canvas:image(x + 12, y + 12, 36, 36, draw.texture(record.icon))
        end
        canvas:text(x + 56, y + 8, w - 64, 22, text.escape(record.name), size.small, color.dim)
        canvas:text(x + 56, y + 32, w - 64, 40, tostring(math.floor(stat.modified)), size.title,
            valueColor(stat.modified, stat.base))
        -- The bonus at the next level up, when it is more than the usual one point.
        local bonus = levelUpBonus(levelInfo, info.id)
        if bonus > 1 then
            canvas:text(x + w - 60, y + 40, 48, 28, 'x' .. bonus, size.large, color.new, ui.ALIGNMENT.End)
        end
    end
end

local function drawEntry(canvas, x, y, w, h, info, focused)
    if focused then
        canvas:rect(x, y, w, h, color.accent, 0.16)
        canvas:rect(x, y, 3, h, color.accent)
    end
    local nameColor = focused and color.active or color.bright
    local valueW = 120
    if info.kind == 'skill' then
        local record = core.stats.Skill.records[info.id]
        local stat = skillStat(info.id)
        local iconSize = h - 12
        if record.icon and record.icon ~= '' then
            canvas:image(x + 8, y + 6, iconSize, iconSize, draw.texture(record.icon))
        end
        local textX = x + 8 + iconSize + 12
        canvas:text(textX, y + 2, x + w - textX - valueW, 26, text.escape(record.name), size.normal, nameColor)
        canvas:text(x + w - valueW - 8, y + 2, valueW, 26, tostring(math.floor(stat.modified)), size.normal,
            valueColor(stat.modified, stat.base), ui.ALIGNMENT.End)
        canvas:text(textX, y + 26, x + w - textX - valueW - 140, 20, text.escape(attributeName(record.attribute)),
            size.small, color.dim)
        -- Progress toward the skill's next increase.
        local percent = skillPercent(info.id)
        canvas:bar(x + w - 8 - 220, y + 33, 160, 6, percent / 100, color.accent)
        canvas:text(x + w - 8 - 52, y + 26, 52, 20, percent .. '%', size.small, color.dim, ui.ALIGNMENT.End)
    elseif info.kind == 'reputation' or info.kind == 'bounty' then
        local reputation = info.kind == 'reputation'
        canvas:text(x + 16, y, w - 32, h, reputation and gmst('sReputation', 'Reputation') or gmst('sBounty', 'Bounty'),
            size.normal, nameColor)
        local value = reputation and types.NPC.stats.reputation(self).current or types.Player.getCrimeLevel(self)
        canvas:text(x + w - valueW - 8, y, valueW, h, tostring(value), size.normal,
            (not reputation and value > 0) and color.worse or color.bright, ui.ALIGNMENT.End)
    elseif info.kind == 'faction' then
        local faction = core.factions.records[info.id]
        canvas:text(x + 16, y + 2, w - 32, 26, text.fit(faction.name, w - 32 - valueW, size.normal), size.normal,
            nameColor)
        local second, secondColor = '', color.dim
        if types.NPC.isExpelled(self, info.id) then
            second, secondColor = gmst('sExpelled', 'Expelled'), color.worse
        else
            local rank = math.max(1, types.NPC.getFactionRank(self, info.id))
            local rankRecord = faction.ranks[rank]
            second = rankRecord and text.escape(rankRecord.name) or ''
            canvas:text(x + w - valueW - 8, y + 2, valueW, 26,
                string.format('%s %d / %d', L('StatsRank'), rank, rankCount(faction)), size.small, color.text,
                ui.ALIGNMENT.End)
        end
        canvas:text(x + 16, y + 26, w - 32, 20, second, size.small, secondColor)
    end
end

local function drawList(canvas, x, y, w, h, rows, listState)
    canvas:rect(x, y, w, h, color.panel, 0.9)
    local placed = listState:place(rows, y + 8, y + h - 4, function(row)
        return row.info and ENTRY_HEIGHT or HEADER_ROW_HEIGHT
    end)
    for _, place in ipairs(placed) do
        local row = place.row
        if row.header then
            canvas:text(x + 12, place.y, w - 24, 24, row.header, size.small, color.bright)
            canvas:rect(x + 12, place.y + 23, w - 24, 1, color.accent, 0.25)
        elseif row.note then
            canvas:text(x + 16, place.y, w - 32, 24, row.note, size.normal, color.dim)
        else
            drawEntry(canvas, x + 4, place.y, w - 8, ENTRY_HEIGHT - 2, row.info, place.focused)
        end
    end
end

-- The details panel of a tile or an entry.
local function drawDetails(canvas, x, y, w, h, info, levelInfo, hintList)
    canvas:rect(x, y, w, h, color.panel, 0.9)
    canvas:rect(x, y, w, 1, color.accent, 0.3)
    local bottom = hints.draw(canvas, x + 16, w - 32, y + h, hintList or {}) - 8
    local innerX = x + 16
    local innerW = w - 32
    local ty = y + 14

    local function title(caption, subtitle, icon)
        local textX = innerX
        if icon and icon ~= '' then
            canvas:picture(innerX, y + 16, 64, icon)
            textX = innerX + 76
        end
        canvas:text(textX, y + 14, x + w - 16 - textX, 34, caption, size.large, color.bright)
        canvas:text(textX, y + 46, x + w - 16 - textX, 26, subtitle or '', size.small, color.dim)
        ty = y + ((icon and icon ~= '') and 96 or 84)
    end
    local function heading(caption)
        ty = ty + 6
        canvas:text(innerX, ty, innerW, 24, caption, size.small, color.dim)
        ty = ty + 26
    end
    local function valueRow(label, value, valueColor_)
        if ty + 30 > bottom then
            return
        end
        canvas:text(innerX, ty, innerW, 28, label, size.normal, color.text)
        canvas:text(innerX + innerW - 160, ty, 160, 28, value, size.normal, valueColor_ or color.bright,
            ui.ALIGNMENT.End)
        ty = ty + 30
    end
    local function paragraph(s, textSize, textColor)
        ty = canvas:paragraph(innerX, ty, innerW, s, textSize or size.normal, textColor or color.text, bottom)
    end
    local function spellList(ids)
        for _, id in ipairs(ids) do
            local spell = core.magic.spells.records[id]
            if spell then
                paragraph(spell.name .. '  (' .. spellTypeLabel(spell) .. ')', size.normal, color.new)
            end
        end
    end

    local kind = info.kind
    if kind == 'race' or kind == 'birthSign' then
        local record = kind == 'race' and playerRace() or birthSign()
        if record == nil then
            return
        end
        title(text.escape(record.name), kind == 'race' and gmst('sRace', 'Race') or gmst('sBirthSign', 'Sign'))
        paragraph(record.description)
        if #record.spells > 0 then
            heading(L('StatsAbilitiesAndPowers'))
            spellList(record.spells)
        end
    elseif kind == 'class' then
        local class = playerClass()
        title(text.escape(class.name), gmst('sClass', 'Class'))
        valueRow(gmst('sSpecialization', 'Specialization'), text.escape(specializationName(class.specialization)))
        valueRow(L('StatsFavouredAttributes'), text.escape(attributeName(class.attributes[1]) .. ', '
            .. attributeName(class.attributes[2])))
        local function names(ids)
            local result = {}
            for i, id in ipairs(ids) do
                result[i] = skillName(id)
            end
            return table.concat(result, ', ')
        end
        heading(gmst('sSkillClassMajor', 'Major Skills'))
        paragraph(names(class.majorSkills), size.normal, color.bright)
        heading(gmst('sSkillClassMinor', 'Minor Skills'))
        paragraph(names(class.minorSkills), size.normal, color.bright)
        ty = ty + 8
        paragraph(class.description)
    elseif kind == 'level' then
        local level = types.Actor.stats.level(self)
        title(gmst('sLevel', 'Level') .. ' ' .. level.current, text.escape(playerRecord().name))
        valueRow(L('StatsLevelProgress'), string.format('%d / %d', level.progress, levelUpTotal()))
        heading(L('StatsNextLevelBonus'))
        for _, attribute in ipairs(core.stats.Attribute.records) do
            local bonus = levelUpBonus(levelInfo, attribute.id)
            valueRow(text.escape(attribute.name), 'x' .. bonus, bonus > 1 and color.new or color.dim)
        end
        ty = ty + 8
        paragraph(l10n('StatsLevelHelp'), size.small)
    elseif DYNAMIC[kind] then
        local current, max = dynamicStat(kind)
        title(gmst(DYNAMIC[kind].gmst, DYNAMIC[kind].fallback))
        valueRow(L('StatsValue'), string.format('%d / %d', math.floor(current), math.floor(max)))
        ty = ty + 8
        paragraph(gmstText(DYNAMIC[kind].description))
    elseif kind == 'attribute' then
        local record = core.stats.Attribute.records[info.id]
        local stat = attributeStat(info.id)
        title(text.escape(record.name), L('StatsAttribute'), record.icon)
        valueRow(L('StatsValue'), tostring(math.floor(stat.modified)), valueColor(stat.modified, stat.base))
        if math.floor(stat.modified) ~= math.floor(stat.base) then
            valueRow(L('StatsBase'), tostring(math.floor(stat.base)))
        end
        local bonus = levelUpBonus(levelInfo, info.id)
        valueRow(L('StatsNextLevelBonus'), 'x' .. bonus, bonus > 1 and color.new or color.bright)
        valueRow(L('StatsAttributeIncreases'),
            tostring(types.Actor.stats.level(self).skillIncreasesForAttribute[info.id] or 0))
        ty = ty + 4
        paragraph(record.description)
        heading(L('StatsGoverns'))
        for _, skill in ipairs(core.stats.Skill.records) do
            if skill.attribute == info.id then
                valueRow(text.escape(skill.name), tostring(math.floor(skillStat(skill.id).modified)))
            end
        end
    elseif kind == 'skill' then
        local record = core.stats.Skill.records[info.id]
        local stat = skillStat(info.id)
        title(text.escape(record.name), gmst('sGoverningAttribute', 'Governing Attribute') .. ': '
            .. text.escape(attributeName(record.attribute)), record.icon)
        valueRow(L('StatsValue'), tostring(math.floor(stat.modified)), valueColor(stat.modified, stat.base))
        if math.floor(stat.modified) ~= math.floor(stat.base) then
            valueRow(L('StatsBase'), tostring(math.floor(stat.base)))
        end
        local percent = skillPercent(info.id)
        valueRow(L('StatsProgress'), percent .. '%')
        canvas:bar(innerX, ty, innerW, 8, percent / 100, color.accent)
        ty = ty + 18
        valueRow(gmst('sSpecialization', 'Specialization'), text.escape(specializationName(record.specialization)))
        ty = ty + 4
        paragraph(l10n(isLevelSkill(info.id) and 'StatsCountsToLevel' or 'StatsNotCountsToLevel') .. ' '
            .. l10n('StatsAddsToBonus') .. ' ' .. attributeName(record.attribute) .. '.', size.normal, color.new)
        ty = ty + 4
        paragraph(record.description)
    elseif kind == 'reputation' or kind == 'bounty' then
        local reputation = kind == 'reputation'
        title(reputation and gmst('sReputation', 'Reputation') or gmst('sBounty', 'Bounty'))
        valueRow(L('StatsValue'),
            tostring(reputation and types.NPC.stats.reputation(self).current or types.Player.getCrimeLevel(self)))
        ty = ty + 8
        paragraph(gmstText(reputation and 'sSkillsMenuReputationHelp' or 'sCrimeHelp'))
    elseif kind == 'faction' then
        local faction = core.factions.records[info.id]
        local rank = math.max(1, types.NPC.getFactionRank(self, info.id))
        local expelled = types.NPC.isExpelled(self, info.id)
        local rankRecord = faction.ranks[rank]
        title(text.escape(faction.name), expelled and gmst('sExpelled', 'Expelled')
            or (rankRecord and text.escape(rankRecord.name) or ''))
        local reputation = types.NPC.getFactionReputation(self, info.id)
        valueRow(gmst('sReputation', 'Reputation'), tostring(reputation))
        if expelled then
            return
        end
        local nextRank = faction.ranks[rank + 1]
        if nextRank == nil or nextRank.name == '' then
            ty = ty + 8
            paragraph(l10n('StatsHighestRank'))
            return
        end

        -- What the next rank needs, green where the player meets it, and red where they don't.
        heading(gmst('sNextRank', 'Next Rank') .. ' ' .. text.escape(nextRank.name))
        local function need(label, have, needed)
            valueRow(label, string.format('%d / %d', have, needed), have >= needed and color.better or color.worse)
        end
        for i = 1, 2 do
            local id = faction.attributes[i]
            if id then
                need(text.escape(attributeName(id)), math.floor(attributeStat(id).base), nextRank.attributeValues[i])
            end
        end
        need(gmst('sReputation', 'Reputation'), reputation, nextRank.factionReputation)
        if nextRank.primarySkillValue > 0 then
            local skills = l10n('StatsOneFavouredSkill') .. ' ' .. nextRank.primarySkillValue
            if nextRank.favouredSkillValue > 0 then
                skills = skills .. ', ' .. l10n('StatsTwoFavouredSkills') .. ' ' .. nextRank.favouredSkillValue
            end
            ty = ty + 4
            paragraph(skills, size.normal,
                types.NPC.hasSkillsForRank(self, info.id, rank + 1) and color.better or color.worse)
        end
        heading(gmst('sFavoriteSkills', 'Favorite Skills'))
        for _, id in ipairs(faction.skills) do
            if core.stats.Skill.records[id] then
                valueRow(text.escape(skillName(id)), tostring(math.floor(skillStat(id).base)))
            end
        end
    end
end

local function drawCharacter(canvas, width, height, levelInfo)
    local top = space.contentTop
    local bottom = height - space.contentBottomMargin
    local panelW = math.max(320, math.floor(width * 0.36))
    local leftW = width - 2 * space.margin - panelW - 16
    canvas:text(space.margin + 4, top - 4, leftW, 36, text.escape(playerRecord().name), size.title, color.bright)

    local tiles, attributesY = characterTiles(width)
    canvas:text(space.margin + 4, attributesY - 4, leftW, 26, L('StatsAttributes'), size.small, color.dim)
    state.tile = math.max(1, math.min(#tiles, state.tile))
    for i, tile in ipairs(tiles) do
        drawTile(canvas, tile, i == state.tile, levelInfo)
    end
    drawDetails(canvas, width - space.margin - panelW, top, panelW, bottom - top, tiles[state.tile].info, levelInfo)
end

local function layout()
    local layerSize = layer.size(layer.menu)
    local width, height = layerSize.x, layerSize.y
    local canvas = draw.canvas()
    canvas:rect(0, space.topBarHeight, width, height - space.topBarHeight, color.black, 0.97)
    hub.drawTopBar(canvas, width, statsScreen)

    local captions = {}
    for i, key in ipairs(TAB_KEYS) do
        captions[i] = L(key)
    end
    tabs.draw(canvas, width, captions, state.tab,
        gmst('sLevel', 'Level') .. '  ' .. types.Actor.stats.level(self).current)

    local levelInfo = types.Player.getLevelUpInfo(self)
    if state.tab == TAB.Character then
        drawCharacter(canvas, width, height, levelInfo)
    else
        local top = space.contentTop
        local bottom = height - space.contentBottomMargin
        local panelW = math.max(320, math.floor(width * 0.36))
        local listW = width - 2 * space.margin - panelW - 16
        local rows = buildRows()
        local listState = state.lists[state.tab]
        drawList(canvas, space.margin, top, listW, bottom - top, rows, listState)
        local px = width - space.margin - panelW
        local row = listState:focused(rows)
        if row then
            local hintList = {}
            if listState:groupCount(rows) > 1 then
                hintList[1] = { 'y', L('MagicNextGroup') }
            end
            drawDetails(canvas, px, top, panelW, bottom - top, row.info, levelInfo, hintList)
        else
            canvas:rect(px, top, panelW, bottom - top, color.panel, 0.9)
        end
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

-- ---------------------------------------------------------------------------------------------------------------
-- Input

local function click()
    ambient.playSound('menu click')
end

-- Moves to the nearest tile in the pressed direction, preferring tiles in line with the current one.
local function moveOnTiles(dx, dy)
    local tiles = characterTiles(layer.size(layer.menu).x)
    local from = tiles[math.max(1, math.min(#tiles, state.tile))]
    local fromX = from.x + from.w / 2
    local fromY = from.y + from.h / 2
    local best = math.huge
    for i, to in ipairs(tiles) do
        local offsetX = to.x + to.w / 2 - fromX
        local offsetY = to.y + to.h / 2 - fromY
        local along = dx ~= 0 and offsetX * dx or offsetY * dy
        -- How far the tile is to the side, as the gap between the two tiles' edges. Tiles that overlap the current
        -- one to the side count as in line, such as the wide level tile below the race tile.
        local across
        if dx ~= 0 then
            across = math.max(0, to.y - (from.y + from.h), from.y - (to.y + to.h))
        else
            across = math.max(0, to.x - (from.x + from.w), from.x - (to.x + to.w))
        end
        if along > 1 and not (dx ~= 0 and across > 0) then
            local score = along + 2 * across
            if score < best then
                best = score
                state.tile = i
            end
        end
    end
end

local function move(dx, dy)
    if state.tab == TAB.Character then
        if dx ~= 0 and dy ~= 0 then
            dy = 0
        end
        moveOnTiles(dx, dy)
    elseif dy ~= 0 then
        state.lists[state.tab]:move(buildRows(), dy)
    end
    redraw()
end

local function changeTab(delta)
    state.tab = (state.tab - 1 + delta) % #TAB_KEYS + 1
    click()
    redraw()
end

local function nextGroup()
    if state.tab == TAB.Character then
        return
    end
    if state.lists[state.tab]:nextGroup(buildRows()) then
        click()
        redraw()
    end
end

statsScreen.handler = {
    onButton = function(name)
        if name == 'b' then
            hub.close()
        elseif name == 'y' then
            nextGroup()
        elseif name == 'lb' then
            changeTab(-1)
        elseif name == 'rb' then
            changeTab(1)
        elseif name == 'lt' then
            hub.cycle(-1)
        elseif name == 'rt' then
            hub.cycle(1)
        end
    end,
    onMove = move,
}

-- ---------------------------------------------------------------------------------------------------------------
-- The screen

function statsScreen.title()
    return gmst('sStats', 'Stats')
end

function statsScreen.show()
    signature = currentSignature()
    refreshAt = core.getRealTime() + REFRESH_INTERVAL
    if element then
        element:destroy()
    end
    element = ui.create(layout())
end

function statsScreen.hide()
    if element then
        element:destroy()
        element = nil
    end
end

-- Stats change while the screen is shown, such as when fatigue recovers, so the screen checks a few times a second.
-- The menus pause the game, so it counts real time.
function statsScreen.onFrame()
    if element == nil then
        return
    end
    local now = core.getRealTime()
    if now < refreshAt then
        return
    end
    refreshAt = now + REFRESH_INTERVAL
    local newSignature = currentSignature()
    if newSignature ~= signature then
        signature = newSignature
        redraw()
    end
end

-- Forgets the screen's state, such as when another game is loaded.
function statsScreen.reset()
    resetState()
end

return statsScreen
