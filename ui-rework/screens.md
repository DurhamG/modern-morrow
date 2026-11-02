# Morrowind's UI, piece by piece

This lists every part of the in-game UI, so the rework can go one piece at a time. It was built from the windows that `apps/openmw/mwgui/windowmanagerimp.cpp` creates, the UI modes in `apps/openmw/mwgui/mode.hpp`, and the layout files in `files/data/mygui/`.

Pieces are grouped by how often a player sees them. Code paths are relative to `apps/openmw/mwgui/`, and layout files are in `files/data/mygui/`. "Controller menus" means the existing `GUI.controller menus` setting, which some windows already support with gamepad navigation and button hints.

## Seen all the time

### 1. HUD: the first-person view with no windows open

- Health, magicka and fatigue bars, and the equipped weapon and spell icons with their condition and charge.
- The minimap, the cell name that appears on entering a cell, the crosshair, and the enemy health bar.
- Active spell effect icons, the sneak indicator, and the drowning bar.
- The name of whatever is under the crosshair, such as an NPC, door or item. This is drawn by the tooltip code.
- Messages at the bottom of the screen, such as "Your journal has been updated".
- Screen fades for hits, blindness and werewolf vision.

Code: `hud`, `tooltips`, `messagebox` (the message box manager), `screenfader`, `spellicons`. Layouts: `openmw_hud`, `openmw_tooltips`, `openmw_messagebox`.

### 2. The inventory hub

This opens with the inventory key, or B on a gamepad. Morrowind shows four windows at once, each of which the player can move and resize. With controller menus on, the triggers switch between them, and the window shown fills the screen. Each of the four is its own piece:

- **2a. Inventory.** The paper doll, the item grid with category filters, equipped items, the weight bar, and drag and drop to equip or drop items. Stacks open a dialog for choosing a count. Code: `inventorywindow`, `itemview`, `itemwidget`, `inventoryitemmodel`, `sortfilteritemmodel`, `countdialog`, `draganddrop`. Layouts: `openmw_inventory_window`, `openmw_count_window`.
- **2b. Character sheet.** Health, magicka and fatigue; level and progress; race, class and birthsign; attributes; major, minor and misc skills; faction ranks and reputation. Code: `statswindow`. Layout: `openmw_stats_window`.
- **2c. Magic.** Powers, spells and enchanted items, with the selected one marked, and active effects. Code: `spellwindow`, `spellview`, `spellmodel`. Layout: `openmw_spell_window`.
- **2d. Map.** The local map and the world map, with door markers and the player's own notes. Code: `mapwindow`. Layouts: `openmw_map_window`, `openmw_edit_note`.

With controller menus on, an overlay also shows the tabs for the four windows (`inventorytabsoverlay`, layout `openmw_inventory_tabs`) and a bar of button hints (`controllerbuttonsoverlay`, layout `openmw_controllerbuttons`).

### 3. Dialogue

The NPC's name and disposition, the conversation text, the clickable topic list, and choices that scripts can add. It also has buttons for an NPC's services, which open the screens in piece 6, and a persuasion dialog: admire, intimidate, taunt and bribe. Code: `dialogue`. Layouts: `openmw_dialogue_window`, `openmw_persuasion_dialog`.

### 4. Containers and looting

The container's item grid next to the player's inventory, with "take all". This also covers looting bodies, pickpocketing, and sharing items with a companion, which adds a profit counter. Code: `container`, `containeritemmodel`, `pickpocketitemmodel`, `companionwindow`, `companionitemmodel`. Layouts: `openmw_container_window`, `openmw_companion_window`.

### 5. Item and spell tooltips

These appear everywhere items or spells are shown. They give the name, weight, value, condition, charge, damage, armor rating and magic effects. On the HUD the same code names what is under the crosshair. Code: `tooltips`. Layout: `openmw_tooltips`.

## Seen often

### 6. Merchant and trainer services, opened from dialogue

- **Barter:** the merchant's items next to the player's inventory, with price haggling and gold totals. Code: `tradewindow`, `tradeitemmodel`. Layout: `openmw_trade_window`.
- **Spell buying:** a list of spells with prices. Code: `spellbuyingwindow`. Layout: `openmw_spell_buying_window`.
- **Training:** three skills with prices. Code: `trainingwindow`. Layout: `openmw_trainingwindow`.
- **Travel:** a list of destinations with prices. Code: `travelwindow`. Layout: `openmw_travel_window`.
- **Repair by a merchant:** a list of damaged items with prices. Code: `merchantrepair`. Layout: `openmw_merchantrepair`.

### 7. Journal

A book with quest entries by date, an index of topics and quests, and options to show active or all quests. Code: `journalwindow`, `journalviewmodel`, `journalbooks`, `bookpage`. Layout: `openmw_journal`.

### 8. Quick keys

The quick keys menu assigns items or spells to the number keys 1 to 9. Morrowind's version is a grid of slots with a picker dialog. On a gamepad, this is the natural place for a quick-select wheel. Code: `quickkeysmenu`. Layouts: `openmw_quickkeys_menu`, `openmw_quickkeys_menu_assign`, `openmw_magicselection_dialog`, `openmw_itemselection_dialog`.

### 9. Rest and wait, and level-up

- **Rest and wait:** a slider for hours and a progress bar while time passes. Code: `waitdialog`, `timeadvancer`. Layouts: `openmw_wait_dialog`, `openmw_wait_dialog_progressbar`.
- **Level-up:** the class image, the level-up text, and choosing three attributes with their multipliers. Code: `levelupdialog`. Layout: `openmw_levelup_dialog`.

### 10. Books and scrolls

Paged books, and scrolls as a single page. Both can have a "take" button. They show formatted text with images. Code: `bookwindow`, `scrollwindow`, `formatting`. Layouts: `openmw_book`, `openmw_scroll`.

## Seen sometimes

### 11. Crafting

- **Alchemy:** the apparatus, ingredient slots, the effects a potion will have, a name field, and an ingredient list. Code: `alchemywindow`. Layout: `openmw_alchemy_window`.
- **Spellmaking**, at a spellmaker NPC: the effect list, and editing each effect's magnitude, duration, area and range. It also shows cost and success chance. Code: `spellcreationdialog`. Layouts: `openmw_spellcreation_dialog`, `openmw_edit_effect`.
- **Enchanting**, by the player or an NPC: picking the item and soul gem, then the same effect editing as spellmaking, plus cast type and charge. Code: `enchantingdialog`. Layouts: `openmw_enchanting_dialog`, `openmw_itemselection_dialog`.
- **Repair with a tool:** the tool, its uses, and a list of damaged items. Code: `repair`. Layout: `openmw_repair`.
- **Recharge with a soul gem:** a list of enchanted items and their charge. Code: `recharge`, `itemchargeview`. Layout: `openmw_recharge_dialog`.
- **Soul gem use:** asks whether to recharge or enchant. Code: `soulgemdialog`.

### 12. Main menu and in-game menu

- **The menu itself:** new, load, save, options, credits and exit, shown both at startup and when Escape or Start is pressed in game. Code: `mainmenu`. Layout: `openmw_mainmenu`.
- **Save and load:** the list of characters and saves, with a screenshot and details. Code: `savegamedialog`. Layout: `openmw_savegame_dialog`.
- **Options:** tabs for preferences, audio, controls (key and gamepad rebinding), video and language. A "Scripts" tab holds settings pages that Lua builds. Code: `settingswindow`. Lua: `files/data/scripts/omw/settings/`. Layout: `openmw_settings_window`.
- **Confirmation dialogs**, such as "Are you sure you want to quit?". Code: `confirmationdialog`. Layout: `openmw_confirmation_dialog`.

## Seen once or rarely

### 13. Character creation

- name entry,
- race and appearance: face, hair and sex, with a 3D head preview,
- class, which the player can pick from a list, get from answering questions, or create, with dialogs for choosing a specialization, attributes and skills,
- birthsign,
- the final review screen, which can return to any earlier step.

Code: `charactercreation`, `race`, `class`, `birth`, `review`, `textinput`. Layouts: the `openmw_chargen_*` files and `openmw_text_input`.

### 14. Other screens and dialogs

- **The loading screen:** a wallpaper with a progress bar. Code: `loadingscreen`. Layout: `openmw_loading_screen`.
- **Jail:** the time served and skills lost. Code: `jailscreen`. Layout: `openmw_jail_screen`.
- **Script message boxes:** popups with a list of buttons, which many quests and mods use to ask the player a question. Code: `messagebox` (interactive message boxes). Layouts: `openmw_interactive_messagebox` and its `_notransp` variant.
- **Video playback:** intro and ending movies. Code: `videowidget`.

## Developer tools, out of scope

These stay as they are: the console (`console`), the F10 debug window (`debugwindow`) and the post-processing shader HUD (`postprocessorhud`).

## Shared building blocks

Some widgets and systems are shared by several pieces, so changing them changes all of those pieces at once:

- **The item grid** (`itemview`, `itemwidget`, and the item models) is used by the inventory, containers, barter, companions and the item picker dialogs.
- **The spell list** (`spellview`, `spellmodel`) is used by the magic window and the quick keys picker.
- **Tooltips** (`tooltips`) are used everywhere.
- **Drag and drop** (`draganddrop`) moves items between windows with the mouse. A gamepad UI needs another way to do it.
- **Focus and navigation.** Three systems exist today: keyboard navigation (`keyboardnavigation`); controller menus mode, where each window handles gamepad buttons itself (`WindowBase::onControllerButtonEvent`); and the gamepad cursor, which moves a mouse pointer with the stick.
- **Fonts and scaling.** The UI scaling factor and font size settings, plus the fonts in `files/data/fonts/`.
- **Window chrome.** Window frames, title bars, borders and the parchment skins in `files/data/mygui/openmw_*.skin.xml`. They give the UI its look.
- **Lua UI.** The `openmw.ui` API (`components/lua_ui/`) builds the settings pages and any UI that mods create. A new style should eventually cover it too, or Lua-made UI will look out of place.
