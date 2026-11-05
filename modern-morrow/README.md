# Modern Morrow as a Lua mod

This mod is not meant for upstream. It rebuilds the controller UI of the Modern Morrow fork
(`ui-rework/design/` in `D:\Code\openmw`) as a Lua mod on top of the engine changes in this branch.
So far it has the inventory, magic, stats and journal screens, the reading of books and scrolls, the conversation with
an NPC and the screens that the world and a conversation open (barter, looting and pickpocketing, sharing with a
companion, and the merchant services: training, travel, buying spells and repairs), the crafting screens (repairing
and recharging with the player's own tools, alchemy, enchanting and spellmaking), resting and waiting, the level-up, the
jail, the message boxes that ask a question, the HUD, and character creation. It only works with a controller.

## Enabling it

Add these lines to `openmw.cfg`, after the lines for the Morrowind content files. Don't enable
`ui-proof-mod` at the same time.

```
data="D:/Code/openmw-upstreaming/modern-morrow"
content=modernmorrow.omwscripts
```

The menus open with the game's inventory button, B on a gamepad by default. They open on the screen the
player used last, and on the inventory the first time. The journal button, LB on a gamepad by default, opens them on
the journal. L2 and R2 switch between the inventory, magic, stats and journal screens. The map stays the engine's
own, so it isn't one of them. The journal is the engine's Journal mode, not a window of the inventory's mode, so
switching to it or away from it changes the mode.

To open the menus on one screen from a script or the console, send the player the `ModernMorrowOpen`
event with the name of the window it replaces, such as `self:sendEvent('ModernMorrowOpen', 'Magic')`.
`I.UI.setMode` with a list of windows opens the last screen instead.

## Inventory controls

| Button | What it does |
| --- | --- |
| LB, RB | Switch tabs. |
| D-pad, left stick | Move the selection. Holding repeats. In the equipment overview, left and right switch columns. In an area, left and right move between the slots and the item list. |
| A | Open an area, use or equip the highlighted item, or unequip it if it's equipped. On a slot, show only the items for that slot. While choosing a ring's hand, put the ring there. |
| B | Go back: cancel a ring choice, show every slot again, return to the overview, or close the inventory. |
| X | Drop the highlighted item, asking how many for a stack. On a slot, unequip it. |
| Y | Cycle the sort order through the list's columns, then the name. |
| Right stick | In the Notes tab, scroll the highlighted book's text. |

## Magic controls

| Button | What it does |
| --- | --- |
| LB, RB | Switch tabs: Recent, Active, Spells, Powers, Items and Scrolls. The screen opens on Active. |
| D-pad, left stick | Move the selection. On the Active tab, left and right switch between the effects and the recent spells. |
| A | Ready the highlighted spell, power, magic item or scroll. Readying a magic item equips it. |
| X | Delete the highlighted spell, after a yes or no question. Spells from the race or birthsign can't be deleted. |
| Y | Jump to the next group, such as the next school of spells. |
| B | Close the menus, or answer no to the question. |

## Stats controls

| Button | What it does |
| --- | --- |
| LB, RB | Switch tabs: Character, Skills and Factions. |
| D-pad, left stick | Move the selection. On the Character tab, it moves to the nearest tile in that direction. |
| Y | Jump to the next group of the Skills or Factions list. |
| B | Close the menus. |

## Journal controls

| Button | What it does |
| --- | --- |
| LB, RB | Switch tabs: Recent, Places and Factions. Recent lists the quests by their last update, Places groups them by the town they started in, and Factions by the faction they are for. Finished quests come last on every tab. |
| D-pad, left stick | Move the selection. |
| Y | Jump to the next group. |
| Right stick | Scroll the highlighted quest's entries. |
| B | Close the menus. |

The journal shows who gave a quest, where, and for which faction, when it knows. The engine records the actor who
added each entry and the cell the player was in. For entries from saves made before the engine recorded them, the
conversation line that starts the quest gives the quest giver, and the first cell that the game's files place the quest
giver in gives the place. Like the fork, the journal has no topic index.

## Reading controls

Books and scrolls show as one tall page, with their pictures, alignment, colours and fonts, such as the Daedric
letters.

| Button | What it does |
| --- | --- |
| Either stick, d-pad | Scroll the text. |
| A | Take the book, when it isn't the player's yet. Taking a book that belongs to someone else is theft, as in the engine. |
| B | Close the book. |

## Character creation

Every step of character creation is the fork's: the name, the race, the class, the birth sign and the review. The
game's own script still opens each step when the story gets there. A step that the player went back to goes on to the
next step itself, and a step that the review sent the player to goes back to the review, as in the engine.

### Name controls

The name, and later the name and the description of a made-up class and the names in crafting, are typed with an
on-screen keyboard. Letters start in upper case at the start of a word.

| Button | What it does |
| --- | --- |
| D-pad, left stick | Move between the keys. |
| A | Type the highlighted key. |
| B | Delete the last letter. |
| X | Type a space. |
| Y | Swap the case of the next letter. |
| Start | Finish. A name can't be empty. |

### Race controls

The races are on the left, and the race's description, skill bonuses and specials on the right. The fork shows the head
in the middle and turns it with the right stick. Lua can't draw the head, so the middle is an empty panel, with the sex,
face and hair below it.

| Button | What it does |
| --- | --- |
| D-pad up and down, left stick | Choose the race. |
| Y | Switch the sex. |
| RB | Next face. |
| LB | Next hair. |
| A | Take the race and go on. |
| B | Take the race and go back to the name. |

### Class controls

The class step first asks how to find the class: by answering questions, from a list, or by making one up. The
questions show one at a time, with their voice, and the class they lead to shows as a list of one, with A to take it
and B to choose another way. The list shows each class's picture, specialization, favorite attributes, skills and
description. A takes the highlighted class and B goes back to the question.

The form for making up a class has a line each for the name, the description, the specialization, the two favorite
attributes and the ten skills, and lists the choices for the highlighted line on the right.

| Button | What it does |
| --- | --- |
| D-pad up and down, left stick | Move between the lines, or the choices. |
| D-pad right, A | Move to the choices. On the name or the description, A types it. |
| A in the choices | Take the choice. An attribute or a skill that the class has already swaps places with it. |
| D-pad left, B in the choices | Go back to the lines. |
| X | Finish the class. Without a name, it asks for one first. |
| B | Go back to the question. |

### Birth sign and review controls

The birth signs show with their picture, and their abilities, powers and spells with each effect. A takes the sign
and B goes back to the class.

The review shows the name, race, class and sign, with health, magicka and fatigue, the attributes and specials, and
every skill.

| Button | What it does |
| --- | --- |
| D-pad up and down, left stick | Move between the name, race, class and sign. |
| A | Change the highlighted one. Its step comes back to the review. |
| X | Finish character creation. |
| B | Go back to the birth sign. |

## Conversation

The conversation is a panel along the bottom of the screen, with the NPC in view above it. In first person, the camera
turns so that the NPC's face sits in the middle of the space above the panel, and turns back when the conversation
ends. The third person camera keeps still, as in the fork.

The left side shows the NPC's name and disposition, then the conversation. Each new answer scrolls so that it starts at
the top, under the topic that was asked. The topics that an answer mentions are blue. The right side has two tabs:

- Topics. The new topics come first: those the answers mention, the newest answer's first, then those this NPC has an
  answer of their own to, then the rest. The asked topics, with nothing new to hear, follow, dimmed.
- Services, with a line about each: persuasion, barter, spells, travel, training, repair, enchanting, spellmaking and
  sharing with a companion, whichever the NPC offers. A service opens the mod's screen for it, which comes back to the
  conversation and keeps the NPC in view beside its panel. Training and travel end the conversation, as in the engine.

When an answer asks a question, only the answers to it show, and the conversation can't end until the player picks one.
When a script ends the conversation, only Goodbye is left. Persuasion asks how to persuade, with the NPC's disposition
and the player's gold, and lists only the bribes the player can afford.

| Button | What it does |
| --- | --- |
| D-pad, left stick | Move the selection. |
| A | Ask about the topic, open the service, or give the answer. |
| LB, RB | Switch between Topics and Services. |
| Y | Jump to the next group of topics. |
| Right stick | Scroll back through the earlier answers. |
| B | Leave the conversation. It does nothing while the NPC waits for an answer. In persuasion, it goes back to the services. |

## Message boxes

A message box that asks a question shows as a panel in the middle of the screen, over a darkened view. That covers a
script's message with buttons, the soul gem's question whether to recharge or enchant, the companion's warning and the
question after the player's death. The message comes first, then the answers as rows.

The short messages, such as "Your journal has been updated", show as the fork's boxes in large text, centred above the
bottom of the screen. Up to three stack up, and each fades out at the end of its time.

| Button | What it does |
| --- | --- |
| D-pad, left stick | Move the selection. |
| A | Give the highlighted answer. |
| B | Give the only answer of a message that has one, such as OK. |

## HUD

The HUD is one compact group in the bottom left corner, sized relative to the screen height. The health, magicka and
fatigue bars sit above the icons of the spell effects on the player. The health of the actor that the player last hit,
with a weapon or a spell, shows above them for as long as the engine's enemy bar would. The weapon, spell and sneak
boxes sit to the right of the bars. Below the weapon is its condition, and below the spell the chance to cast it, or
the magic item's charge. The sneak box shows while the player sneaks and no one sees them.

An element fades out when it has nothing to tell: a bar while it is full, the weapon and spell boxes five seconds after
the weapon or spell changed unless it is ready, and the effect icons while there are none. The effects of abilities, such
as the race's and the birthsign's, and of worn gear with a constant effect, are left out. During combat, the bars and
both boxes stay. The minimap with the cell name, and the breath meter, stay the engine's. The whole HUD hides in the
menus and during a conversation.

## Barter, loot and service screens

These screens are panels on the right side of the screen, so the merchant or the chest stays in view on the left. Barter
and the services open from the conversation, and go back to it.

### Barter controls

| Button | What it does |
| --- | --- |
| LB, RB | Switch between Buy and Sell. Buy lists the merchant's goods, with those in the containers that the merchant owns nearby. Sell lists only what the merchant buys. |
| D-pad up and down, left stick | Move the selection. |
| A | Put the whole stack into the deal, or take it back out. |
| D-pad left and right | Put one more of a stack in, or take one out. |
| Y | Jump to the next group. |
| Right stick left and right | Haggle: right asks for more gold, or offers to pay less. |
| Right stick press | Go back to the merchant's price. |
| X | Make the offer. When the merchant accepts, the screen goes back to the conversation. |
| B | Cancel the deal and go back to the conversation. |

### Loot controls

The loot screen shows a container, a body or an NPC's pockets. On an owned container, the header names the owner and
every entry is marked Stealing. Pickpocketing shows only the items the player noticed, and each try can get the player
caught, which closes the screen. The screen closes on its own after the last item is taken.

| Button | What it does |
| --- | --- |
| D-pad, left stick | Move the selection. |
| A | Take the highlighted item. |
| Y | Take everything. On an owned container, it asks first. |
| X | Take the item and equip it, when it's clear where it goes. |
| Left stick press | Switch to your own items, under the inventory's tabs, to put one in. Press it again, or B, to go back. |
| LB, RB | In your items, switch tabs. |
| View | On a body, take everything and dispose of the corpse. |
| B | Close. |

### Companion controls

| Button | What it does |
| --- | --- |
| LB, RB | Switch between the companion's items and yours. |
| A | Take the item, or give it. |
| Y | Jump to the next group. |
| B | Close. A companion who works for pay warns the player who leaves owing them. |

### Service controls

Training, travel, spells and repairs list what the merchant offers, with the price. The details say why an entry
can't be picked, in red. Training and travel end the conversation, and fade the screen out and in, as the engine's
windows do.

| Button | What it does |
| --- | --- |
| D-pad, left stick | Move the selection. |
| A | Train, travel, buy or repair. |
| B | Go back to the conversation. |

## Crafting screens

The crafting screens fill the screen, with a list on the left and what is being made on the right, as in the fork. They
open the way the engine's windows do: using a repair tool, a soul gem or a mortar and pestle from the inventory, or
asking an enchanter or a spellmaker in a conversation. B closes them and goes back to where they opened from.

### Repair and recharge controls

The items that need the tool are on the left, worst first. The tool is on the right, with what is left of it. When it
runs out, the next one of its kind takes its place.

| Button | What it does |
| --- | --- |
| D-pad, left stick | Move the selection. |
| A | Use the tool once on the highlighted item. The game's messages say whether it worked. |
| Y | Switch to the next repair tool, or the next soul gem. |

### Alchemy controls

Each ingredient shows the effects the player knows. Once the potion holds an ingredient, the list splits into the
ingredients in the potion, those that share an effect with it, and the others. The best apparatus the player carries is
used, as in the engine's window.

| Button | What it does |
| --- | --- |
| D-pad, left stick | Move the selection. |
| A | Put the highlighted ingredient in the potion, or take it out. |
| LB, RB | Brew fewer or more potions, up to what the ingredients allow. |
| X | Brew. |
| Left stick press | Name the potion with the on-screen keyboard. Until then, and after a change of ingredients, it has the name of its first effect. |

### Enchanting and spellmaking controls

The right side lists the item, the soul gem, the cast type and the effects, with a line to add an effect, and the
numbers at the bottom. The numbers include the chance that the player's own enchanting works when the game's "show
enchant chance" setting is on. Spellmaking has only the effects. Until the player names it, a new spell is named after
its first effect, and an enchanted item keeps the item's name.

| Button | What it does |
| --- | --- |
| D-pad up and down, left stick | Move the selection. |
| D-pad left and right | Move between the list on the left and the lines on the right. |
| A | On a line, fill the list with what can go there, step to the next cast type, or edit the effect. In the list, choose the highlighted entry. An effect on a skill or an attribute asks which one first. |
| X | Enchant the item, or buy the spell. |
| Left stick press | Name the enchanted item or the spell with the on-screen keyboard. |
| B | Go back to the lines, or close. |

While an effect is edited, the left side shows its range, magnitude, duration and area.

| Button | What it does |
| --- | --- |
| D-pad up and down | Move between the settings. |
| D-pad left and right | Change the setting. Holding changes it faster. |
| LB, RB | Change the setting by 10. |
| A | Keep the effect. |
| X | Delete an effect that was added already. |
| B | Cancel the change. |

## Rest, level-up and jail

### Rest controls

The rest panel shows whether the player sleeps or only waits there, the hours, when the rest ends, and how long resting
until healed takes. While the hours pass, a progress panel shows. After a sleep, the level-up opens when one is ready.

| Button | What it does |
| --- | --- |
| D-pad left and right | Fewer or more hours. |
| LB, RB | One hour, or a whole day. |
| A | Rest or wait. |
| X | Rest until healed, when the player can sleep and is hurt. |
| B | Close. |

### Level-up controls

The class picture and the level's text are on the left, and the attributes as tiles on the right, with what each
gains. The highlighted attribute's description and the skills that raise it are below the tiles.

| Button | What it does |
| --- | --- |
| D-pad, left stick | Move between the attributes. |
| A | Choose the attribute, or take it back. With enough chosen already, the last one gives way. |
| X | Finish, once enough attributes are chosen. |

The jail shows a progress panel while the player serves their time. It takes no input.

## How the scripts are laid out

Everything is in `scripts/modernmorrow/`:

- `player.lua` and `global.lua` are the mod's main scripts. The player script sets up the layers,
  registers the screens and passes input on. The global script does what player scripts may not, such as dropping an
  item into the world, making a trade, taking items with the crime rules, disposing of a corpse, buying services,
  crafting, resting, levelling up and naming the cells that journal entries name. The player script asks for these with
  events, and the global script answers with events when the player script needs the outcome.
- `ui/` holds what every screen shares:
  - `style.lua`: colours, text sizes and spacing.
  - `layer.lua`: the layers, each 720 units tall, for the HUD, for screens, for dialogs over them and for the short
    messages.
  - `draw.lua`: a canvas that collects rectangles, text, wrapped paragraphs, bars, item icons and button
    icons at absolute positions, and turns them into one layout.
  - `text.lua`: measuring, wrapping and shortening text with `ui.measureText`.
  - `list.lua`: a list with group headers that scrolls to keep the selection in view, and that Y can jump
    through group by group.
  - `hints.lua`: button hints. B gets no hint when it only goes back.
  - `tabs.lua`: a screen's own row of tabs with a status line.
  - `countpicker.lua`: the dialog that asks how many of a stack to use.
  - `confirm.lua`: the dialog that asks a yes or no question, such as whether to delete a spell.
  - `scroll.lua`: scrolling text with a stick, and the scroll bar beside it.
  - `panel.lua`: the panel on the right side of the screen that the barter, loot, companion and service screens
    share, with its title, tabs, list of fat entries and details column.
  - `question.lua`: the dialog that asks a question with a list of answers, for the message boxes and persuasion.
  - `screen.lua`: the full screens of crafting and the level-up, with the title and status line at the top and boxes
    below, their lists of fat entries, and the progress panel that resting and the jail show.
  - `keyboard.lua`: the on-screen keyboard, for the name in character creation and the names in crafting.
  - `choice.lua`: the full screen choice from a list, with a picture and details, for the classes and birth signs.
- `input/router.lua` sends controller input to the screen or dialog on top. It repeats a held
  d-pad direction, and turns the triggers into button presses.
- `screens/hub.lua` holds the menus that the inventory button opens. Each screen replaces one
  engine window. The hub shows one screen at a time, hides the engine's map, draws the bar of screen
  names at the top, and switches screens when the triggers are pulled. A screen can replace the only window of
  another mode, as the journal does, and the hub changes the mode to switch to it.
- `screens/inventory.lua`, `screens/magic.lua`, `screens/stats.lua` and `screens/journal.lua` are the four screens.
- `screens/reading.lua` replaces the engine's Book and Scroll windows. It isn't one of the hub's screens.
- `screens/barter.lua`, `screens/loot.lua`, `screens/companion.lua` and `screens/services.lua` replace the Trade,
  Container, Companion, Training, Travel, SpellBuying and MerchantRepair windows. They aren't the hub's screens either.
- `screens/tools.lua` replaces the Repair and Recharge windows, `screens/alchemy.lua` the Alchemy window, and
  `screens/effectcraft.lua` the EnchantingDialog and SpellCreationDialog windows. The last one holds the effect editor
  that enchanting and spellmaking share, with the engine's effect rules: the ranges an effect allows, the limits of each
  setting, and constant effects working on self only.
- `screens/rest.lua`, `screens/levelup.lua` and `screens/jail.lua` replace the WaitDialog, WaitDialogProgressBar,
  LevelUpDialog and JailScreen windows. The rest and the jail fill their progress panels from the engine's
  RestProgress and JailProgress events.
- `screens/dialogue.lua` replaces the Dialogue window. `screens/messages.lua` shows the message boxes that ask a
  question, through `I.UI.registerInteractiveMessageHandler`, and the short messages, through
  `I.UI.registerMessageHandler`.
- `chargen/` replaces the character creation windows. `flow.lua` ends a step with `types.Player.finishCharGenStep`,
  which goes on as the engine's window would, and takes a step that the mod's review sent the player to back to the
  review. `name.lua`,
  `race.lua`, `classes.lua` (how to find the class, the class questions and their result, and the class list),
  `createclass.lua`, `birth.lua` and `review.lua` are the steps. They give the player their choices with the
  `types.Player.setCharGen` functions.
- `camera/framing.lua` turns the camera to the NPC's face during a conversation, with the camera's extra pitch and yaw.
- `hud/hud.lua` draws the HUD and hides the engine's elements that it replaces. It asks the engine whose health the
  enemy bar shows, with `I.UI.getHudEnemy`, and whether the sneak indicator would show, with `I.UI.isHudElementWanted`.
  It learns who fights the player from the event that the built-in music script sends from each actor.
- `items/` holds what the screens know about items: `info.lua` (kinds, slots, stats, effects and the groups that the
  panels sort items into),
  `details.lua` (the details panel), `recent.lua` (the log of new items, kept in the save) and
  `books.lua` (a book's markup, read the way the engine's book formatter reads it, and laid out as one column).
- `magic/` holds what the magic screen knows: `castables.lua` (the spells, powers, magic items and
  scrolls the player can ready, and the effects on the player) and `recent.lua` (the log of recently
  readied spells, kept in the save).

- `journal/quests.lua` collects the quests from the journal, and works out who gave each one, where and for which
  faction.

Strings are in `l10n/ModernMorrow/en.yaml`.

## Known problems

- Training shows no progress bar while the two hours pass. The engine's training window shows its bar for a tenth of a
  second, while the screen fades out.
- Reading a book from the inventory hides the inventory behind the book. When a mode opens on top of another, the
  `I.UI` interface hides every window that Lua replaced, including those of the mode below.
- The race screen has no head. The fork draws the head in 3D and turns it with the right stick, and Lua can't draw a
  3D preview in its UI.
- The breath meter stays the engine's. Lua can tell when the engine shows it, but not how much breath is left.
- The HUD doesn't show which quick key was used, as the fork does. Lua can't see the quick keys.
- When the review sends the player back to a step, the engine's review window tells character creation to come back to
  it. The mod's review can't, so the mod remembers that itself. Otherwise `types.Player.finishCharGenStep` would go on
  through the steps after it.
- The name step starts from the player's name when it differs from the name the game started with. The engine's window
  starts from it once the name step was done, which Lua can't ask.
- A player script can only read the name of the cell that the player is in, so the global script names the other cells
  that journal entries name.
