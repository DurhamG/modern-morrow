---
-- Provides interfaces to interact with menu elements.
-- @context menu
-- @module menu
-- @usage local menu = require('openmw.menu')

---
-- @type STATE
-- @field [parent=#STATE] NoGame
-- @field [parent=#STATE] Running
-- @field [parent=#STATE] Ended

---
-- All possible game states returned by @{#menu.getState}
-- @field [parent=#menu] #STATE STATE

---
-- Current game state
-- @function [parent=#menu] getState
-- @return #STATE

---
-- A table of parameters for @{#menu.newGame}
-- @type NewGameOptions
-- @field #boolean bypass Skip new game video and character generation (default: false)

---
-- Start a new game
-- @function [parent=#menu] newGame
-- @param #NewGameOptions options An optional table with additional optional arguments

---
-- Load the game from a save slot
-- @function [parent=#menu] loadGame
-- @param #string directory name of the save directory (e. g. character)
-- @param #string slotName name of the save slot

---
-- Delete a saved game
-- @function [parent=#menu] deleteGame
-- @param #string directory name of the save directory (e. g. character)
-- @param #string slotName name of the save slot

---
-- Current save directory
-- @function [parent=#menu] getCurrentSaveDir
-- @return #string

---
-- Save the game
-- @function [parent=#menu] saveGame
-- @param #string description human readable description of the save
-- @param #string slotName name of the save slot

---
-- @type SaveInfo
-- @field #string description
-- @field #string playerName
-- @field #string playerLevel
-- @field #number timePlayed Gameplay time for this saved game. Note: available even with [time played](../modding/settings/saves.html#timeplayed) turned off
-- @field #number creationTime Time at which the game was saved, as a timestamp in seconds. Can be passed as the second argument to `os.data`.
-- @field #list<#string> contentFiles
-- @field #number playerHealth The player's health when the game was saved. It is 0 in saves made by old versions of OpenMW.
-- @field #number playerMaxHealth The player's maximum health when the game was saved. It is 0 in saves made by old versions of OpenMW.
-- @field #string cellName The name of the cell the player was in, translated the same way as @{openmw.core#Cell.displayName}. For an exterior cell without a name, it is the name of the region.
-- @field #number day The number of days passed in the game, from the `DaysPassed` game variable. It is 0 in saves made by old versions of OpenMW.
-- @field #number dayOfMonth The in-game day of the month.
-- @field #number month The in-game month, from 1 to 12.
-- @field #number year The in-game year.
-- @field #number gameHour The in-game hour, from 0 to 24, with a fractional part for the minutes.

---
-- All the saves for the given directory
-- @function [parent=#menu] getSaves
-- @param #string directory name of the save directory (e. g. character)
-- @return #map<#string, #SaveInfo> map with save filenames as keys

---
-- List of all available saves, grouped by directory
-- @function [parent=#menu] getAllSaves
-- @return #map<#string, #map<#string, #SaveInfo>> map with directory names as keys, returning maps with save filenames as keys

---
-- Exit the game
-- @function [parent=#menu] quit

return nil
