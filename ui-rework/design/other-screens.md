# The other screens

This covers the screens reworked in one pass while the user was away: the merchant services, questions and confirmations, rest and level up, reading, saving and loading, the count dialog, companion sharing, loading and jail, videos, character creation, and the options. None of them has been reviewed yet. Quick keys and crafting were left out on purpose, because they need a design discussion first.

Every one of them works the same way. In controller menus mode, the old window stays hidden and keeps doing the work, and a panel in the style of the other controller screens shows instead. The panel reads from the window, and its buttons call the window's own handlers. So prices, checks and rules don't change.

## Shared pieces

- **`ServicePanel`** draws the merchant services as a panel on the right side of the screen, like the barter panel.
- **`ControllerDialog`** asks a question with a list of answers in the middle of the screen, over a darkened view. It is on its own layer, `ControllerModal`, above the controller menus. Each dialog goes to the front of that layer when it shows.
- **`ChoiceScreen`** draws a full screen choice from a list, with a picture and details. Character creation uses it for the class and birthsign lists.
- **`ReadingPanel`** shows a book or scroll as one tall page.
- **`ControllerUi::drawProgressPanel`** draws a label and a progress bar, for resting, training, loading and jail.

## The screens

| Screen | What it looks like now | Buttons |
| --- | --- | --- |
| Spells, training, travel, repair | A list of what the merchant offers with prices, and details beside it: a spell's effects, cost and chance, a skill's values and limits, the travel time, an item's condition. Entries the player can't pick say why in red. | A buy, B back |
| Short messages, such as "You failed casting the spell" | Boxes in the panel style, in large text, stacked above the bottom of the screen. They scale with the screen and fade out at the end. | None |
| Persuasion | The question dialog, with the person's name, their disposition and the player's gold, and the choices: admire, intimidate, taunt, and the bribes the player can afford. | Up and down, A tries |
| Message boxes, confirmations, class questions | The message, then the answers as rows. Long answers wrap. | Up and down, A pick, B cancel where there is a cancel answer |
| Rest and wait | The hours in large text, when the rest ends, and how long resting until healed takes. | Left and right change the hours, L1 one hour, R1 a day, A rest, X until healed, B cancel |
| Level up | The class image and level text on the left, and the attributes as tiles with their bonuses. The highlighted attribute's description and the skills that raise it are below. | D-pad moves, A chooses, X finishes |
| Books and scrolls | One tall page with the title above it, and a scroll bar. | Either stick or the d-pad scrolls, A takes, B closes |
| Save and load | The saves on the left with place and date, and the screenshot and details on the right. Saving starts with a new save. When OpenMW is installed, tabs switch between Modern Morrow's saves and OpenMW's. | A save or load, X delete, L1 and R1 switch between Modern Morrow and OpenMW, L2 and R2 switch characters, B back |
| Count | The action, the item and the count in large text, with a bar. | Left and right change it, L1 one, R1 all, A OK, B cancel |
| Companion sharing | The loot panel's look, with tabs for their items and yours. | L1 and R1 switch, A take or give, B close |
| Loading and jail | The label and bar as a panel that scales with the screen. | None |
| Videos | Unchanged, except that a controller can skip them. | A, B or Start skips |
| Name and other text | The text so far, and an on-screen keyboard. | D-pad moves, A types, B deletes, X space, Y shift, Start done |
| Race | The races on the left, a large head in the middle, and the description, skill bonuses and specials on the right. | Up and down race, Y sex, R1 face, L1 hair, right stick turns, X done, B back |
| Custom class | The class on the left, one line each: name, description, specialization, the two favorite attributes, and the five major and five minor skills. The right side lists the choices for the highlighted line, with the current one marked, skills grouped by specialization, and describes the highlighted choice. Picking a skill or attribute already on the class swaps the two. | Up and down, right or A to choose, A types the name and description, X done |
| Class and birthsign | A list, a picture, and the details: specialization, attributes and skills, or the sign's spells. The class the questions work out shows as a list of one. | Up and down, X done, B back |
| Review | The four choices, health, magicka and fatigue on the left, the attributes and specials in the middle, and every skill on the right. | Up and down, A change, X done, B back |
| Options | A row for each setting on the tab, with inner tabs as headings. | Up and down, left and right change, A presses, L1 and R1 tabs, B close |

## OpenMW's saves

Players move games between OpenMW and Modern Morrow without copying files by hand. The save and load screens have a tab for Modern Morrow's own saves and one for OpenMW's.

- **Finding OpenMW.** The game looks in OpenMW's usual folder for the platform, such as `Documents\My Games\OpenMW` on Windows. On Linux, it also looks in the Flatpak's folder, `~/.var/app/org.openmw.OpenMW`, which is how the Steam Deck's Discover installs OpenMW. If more than one is installed, it shows the one saved to most recently. The `[Saves] openmw saves folder` setting can name the folder instead.
- **Loading.** The load screen shows OpenMW's tab when it has saves. It always opens on Modern Morrow's tab, and when that has no saves, it says the OpenMW tab has OpenMW's. Loading from it just loads the save. The game then goes on in Modern Morrow's character folder of the same name, which the first save creates if it is missing. A save there at format 42 or above is refused as too new, since in Modern Morrow's own folder 42 and 43 mean its earlier saves.
- **Saving.** The save screen shows OpenMW's tab whenever OpenMW is installed. Saving there writes into OpenMW's character folder with the same name as the current one. Saving over an OpenMW save asks first, then copies the old save to `backups/OpenMW/<character>/` beside Modern Morrow's saves, with the time in its name. If the copy fails, the save isn't overwritten. The pinned quests file goes beside OpenMW's saves too, so pins survive OpenMW saving the game again.
- **No deleting.** X doesn't delete OpenMW's saves, since one may be the only copy of a game played there.
- **Only in controller menus mode.** The mouse-and-keyboard save window still shows only Modern Morrow's saves.

- **No Morrowind tab.** Morrowind's own `.ess` saves aren't shown. OpenMW's importer is a separate program rather than code the game could call, and it drops the journal, so the controller journal of an imported game would be empty. Players convert a Morrowind save with OpenMW's `openmw-essimporter` themselves, then load it from the OpenMW tab or copy it into Modern Morrow's saves.

## Not done yet

- **Crafting and quick keys** were left out at first, on purpose. They have their own designs now, in [crafting.md](crafting.md) and [quick-keys.md](quick-keys.md).
- **The options' key bindings and scripts' pages** still show the old window, because they are built in code rather than in the layout.
- **The loading panel and video skipping** couldn't be checked with the harness. The harness can't answer while loading runs or a video plays.
- **Mouse input.** The panels only respond to a gamepad, like the other controller screens.
