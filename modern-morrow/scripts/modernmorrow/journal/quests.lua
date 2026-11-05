-- The player's quests, from the journal, with who gave each one, where, and for which faction, when that is known.
--
-- Morrowind's quests have only a name, numbered stages with their text, and a finished flag. This module works out who
-- gave a quest, where, and for which faction, as the fork's journal does (ui-rework/design/journal.md in
-- D:\Code\openmw):
--   * The journal records the actor who added each entry, and the cell the player was in. The first entry that names
--     them gives the quest giver and the place. Saves from before OpenMW recorded them have neither.
--   * Otherwise, the conversation line that sets the quest's earliest stage gives the quest giver. The first cell that
--     the content files place the quest giver in gives the place, and otherwise a conversation that is only for one
--     cell, such as "Balmora".
--   * The faction is the one in the quest's name, such as "Fighters Guild: Exterminator", then the quest giver's
--     faction, then the faction that the conversation is for.

local core = require('openmw.core')
local self = require('openmw.self')
local types = require('openmw.types')
local calendar = require('openmw_aux.calendar')

local quests = {}

-- The names of the cells that journal entries name, by cell ID. A player script can only read the name of the cell
-- that the player is in, so the global script names the others. Missing names are asked for once.
local cellNames = {}
local askedCells = {}
-- Whether names arrived since the quests were last collected.
local namesArrived = false

local function journalEntries()
    return types.Player.journal(self).journalTextEntries
end

-- The start of a cell's name, before a comma or a colon, such as "Balmora" for "Balmora, Guild of Fighters" or
-- "Mournhold" for "Mournhold Temple: Reception Area".
local function townOf(place)
    return (place:match('^([^,:]*)') or place):gsub('%s+$', '')
end

-- The place and town of a cell. Exteriors without a name are in their region.
local function describeCell(cell)
    if cell == nil then
        return nil, nil
    end
    local name = cell.displayName ~= '' and cell.displayName or cell.name
    if cell.isExterior and name == '' then
        local region = cell.region and core.regions.records[cell.region]
        name = region and region.name or ''
    end
    if name == '' then
        return nil, nil
    end
    return name, townOf(name)
end

-- ---------------------------------------------------------------------------------------------------------------
-- Cell names

-- Names the player's cell, which new entries name. From the player script's onFrame.
function quests.update()
    local cell = self.cell
    if cell and cellNames[cell.id] == nil then
        cellNames[cell.id] = describeCell(cell) or ''
    end
end

-- From the global script: the names of cells, by cell ID.
function quests.onCellNames(names)
    for id, name in pairs(names) do
        cellNames[id] = name
    end
    namesArrived = true
end

-- Whether cell names arrived since the quests were collected, so they should be collected again.
function quests.hasNewNames()
    return namesArrived
end

-- Forgets the cell names, such as when another game is loaded.
function quests.reset()
    cellNames = {}
    askedCells = {}
    namesArrived = false
end

local function askForCellNames(ids)
    local missing = {}
    for id in pairs(ids) do
        if cellNames[id] == nil and not askedCells[id] then
            askedCells[id] = true
            missing[#missing + 1] = id
        end
    end
    if #missing > 0 then
        core.sendGlobalEvent('ModernMorrowCellNames', { player = self.object, ids = missing })
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- The conversations that start quests

local sources = nil

-- Calls fn(questId, stage) for every Journal command in a dialogue script, such as "Journal FG_RatHunt 10" or
-- "player->Journal, "A1_1_FindSpymaster", 1".
local function forEachJournalCommand(script, fn)
    for line in script:lower():gmatch('[^\r\n]+') do
        line = line:gsub(';.*$', ''):gsub('[,"]', ' ')
        local tokens = {}
        for token in line:gmatch('%S+') do
            tokens[#tokens + 1] = token
        end
        for i = 1, #tokens - 2 do
            local command = tokens[i]
            if command == 'journal' or command:sub(-9) == '->journal' then
                local stage = tonumber(tokens[i + 2]:match('^%-?%d+'))
                if stage then
                    fn(tokens[i + 1], stage)
                end
            end
        end
    end
end

local function nonEmpty(s)
    if s == nil or s == '' then
        return nil
    end
    return s
end

-- For each quest ID, the conversation line that sets its earliest stage, with the speaker, faction and cell that the
-- line is for. Content can't change while the game runs, so this is worked out once.
local function conversationSources()
    if sources then
        return sources
    end
    sources = {}
    for _, kind in ipairs({ 'topic', 'greeting', 'persuasion', 'voice' }) do
        for _, record in pairs(core.dialogue[kind].records) do
            for _, info in ipairs(record.infos) do
                local script = info.resultScript
                if script and script:lower():find('journal', 1, true) then
                    local actor = nonEmpty(info.filterActorId)
                    local faction = nonEmpty(info.filterActorFaction)
                    local cell = nonEmpty(info.filterPlayerCell)
                    if actor or faction or cell then
                        forEachJournalCommand(script, function(quest, stage)
                            local source = sources[quest]
                            if stage > 0 and (source == nil or stage < source.stage) then
                                sources[quest] = { stage = stage, actor = actor, faction = faction, cell = cell }
                            end
                        end)
                    end
                end
            end
        end
    end
    return sources
end

-- ---------------------------------------------------------------------------------------------------------------
-- Collecting the quests

-- Guild and House quests have the faction in their name, such as "Fighters Guild: Exterminator" or "House Hlaalu: The
-- Twin Lamps", where the faction is Great House Hlaalu.
local function factionFromName(name)
    local prefix = name:match('^([^:]*):')
    if prefix == nil or prefix == '' then
        return nil
    end
    prefix = prefix:lower()
    for _, faction in pairs(core.factions.records) do
        local factionName = faction.name:lower()
        if factionName == prefix
            or (#factionName > #prefix + 1 and factionName:sub(-#prefix) == prefix
                and factionName:sub(-#prefix - 1, -#prefix - 1) == ' ') then
            return faction.id
        end
    end
    return nil
end

-- A name for a quest that has none, from the start of its first entry.
local function untitledName(text)
    local MAX_LENGTH = 48
    local sentence = text:match('^([^%.!%?]*)') or text
    if #sentence > MAX_LENGTH then
        local cut = sentence:sub(1, MAX_LENGTH):match('^(.*) ') or sentence:sub(1, MAX_LENGTH)
        sentence = cut .. '...'
    end
    return sentence
end

local function questName(id)
    local record = id and core.dialogue.journal.records[id]
    return record and record.questName or ''
end

local function npcRecord(id)
    return id and types.NPC.records[id]
end

local function findSource(quest)
    -- What the journal recorded comes first.
    for _, entry in ipairs(quest.entries) do
        quest.giver = quest.giver or entry.actorRecordId
        local name = entry.cellId and cellNames[entry.cellId]
        if quest.place == nil and name and name ~= '' then
            quest.place, quest.town = name, townOf(name)
        end
    end

    -- Otherwise, the conversation that sets the earliest stage of any of the quest's IDs.
    local source = nil
    for _, id in ipairs(quest.ids) do
        local candidate = conversationSources()[id]
        if candidate and (source == nil or candidate.stage < source.stage) then
            source = candidate
        end
    end
    if source then
        quest.giver = quest.giver or source.actor
    end
    -- The first cell that the content files place the quest giver in.
    if quest.place == nil and quest.giver then
        local cell = core.contentFiles.getPlacedCells(quest.giver)[1]
        if cell and cell.name ~= '' then
            quest.place, quest.town = cell.name, townOf(cell.name)
        end
    end
    if source then
        if quest.place == nil and source.cell then
            -- The conversation is for cells whose names start with this, such as "Balmora".
            quest.place, quest.town = source.cell, townOf(source.cell)
        end
    end

    quest.faction = factionFromName(quest.name)
    if quest.faction == nil then
        local npc = npcRecord(quest.giver)
        quest.faction = npc and nonEmpty(npc.primaryFaction)
    end
    if quest.faction == nil and source then
        quest.faction = source.faction
    end
end

-- The player's quests, the most recently updated first. Each quest has:
--   name        its name, or the start of its first entry if it has none
--   finished    whether it is finished
--   entries     its journal entries, oldest first, each with `text`, `day`, `month`, `dayOfMonth`, and the
--               `actorRecordId` and `cellId` that the journal recorded, if any
--   giver       the record ID of the actor who gave it, or nil
--   place, town where it was given, or nil
--   faction     the ID of the faction it is for, or nil
-- Quest IDs with the same name are one quest, as in the journal book. Quests without a name are kept apart by their ID.
function quests.collect()
    namesArrived = false
    local list = {}
    local byKey = {}
    local cells = {}
    for i, entry in ipairs(journalEntries()) do
        local id = entry.questId
        local name = questName(id)
        local key = name ~= '' and name:lower() or ('#' .. tostring(id or i))
        local quest = byKey[key]
        if quest == nil then
            quest = { name = name ~= '' and name or untitledName(entry.text), ids = {}, idSet = {}, entries = {},
                finished = false }
            byKey[key] = quest
            list[#list + 1] = quest
        end
        if id and not quest.idSet[id] then
            quest.idSet[id] = true
            quest.ids[#quest.ids + 1] = id
        end
        quest.entries[#quest.entries + 1] = {
            text = entry.text,
            day = entry.day,
            month = entry.month,
            dayOfMonth = entry.dayOfMonth,
            actorRecordId = entry.actorRecordId,
            cellId = entry.cellId,
        }
        if entry.cellId then
            cells[entry.cellId] = true
        end
        quest.lastActivity = i
    end

    -- A quest is finished when any of its IDs is, as in the journal book.
    for id, state in pairs(types.Player.quests(self)) do
        if state.finished then
            local name = questName(id)
            local quest = byKey[name ~= '' and name:lower() or ('#' .. id)]
            if quest then
                quest.finished = true
            end
        end
    end

    askForCellNames(cells)
    for _, quest in ipairs(list) do
        findSource(quest)
    end
    table.sort(list, function(a, b)
        return a.lastActivity > b.lastActivity
    end)
    return list
end

function quests.giverName(quest)
    if quest.giver == nil then
        return ''
    end
    local record = types.NPC.records[quest.giver] or types.Creature.records[quest.giver]
    return record and record.name or ''
end

function quests.factionName(quest)
    local record = quest.faction and core.factions.records[quest.faction]
    return record and record.name or ''
end

-- When an entry was written, such as "16 Last Seed (Day 1)".
function quests.timestamp(entry)
    return string.format('%d %s (%s %d)', entry.dayOfMonth, calendar.monthName(entry.month),
        core.getGMST('sDay') or 'Day', entry.day)
end

return quests
