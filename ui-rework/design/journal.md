# Journal

This is the design for the reworked journal, agreed with the user. In controller menus mode, it replaces the journal book with a tab of the menus. It reuses the other screens' look, so it was built without mock-ups.

## The problems with the journal book

- It's a book of dated entries, oldest first, with each quest's entries spread across many pages. Finding out what to do next means paging back to a quest's last entry.
- The quest list is a separate page behind an Options button, and it only lists quest names.
- Topic links in the text and the topic index need a cursor to use.

## Decisions

- **A Journal tab** beside the map, inventory, magic and stats. The journal key opens the menus on it.
- **A list of quests on the left, details on the right,** as on the other screens. Pinned quests come first, under their own heading, then the quests in progress, then the finished ones under their own heading.
- **Three ways to sort the list,** as tabs switched with L1 and R1:
  - **Recent:** by the last update, newest first. The screen opens on this tab.
  - **Places:** grouped by the town the quest started in, such as Balmora. Quests without a known place come last.
  - **Factions:** grouped by the faction the quest is for. Quests for no faction come last.
- **Each entry** shows the quest's name, the date of its last update, and its place and faction in two columns. Guild and House quests leave the faction out of their name, such as "Exterminator" for "Fighters Guild: Exterminator", because the column shows it.
- **The details panel** shows the full name, whether the quest is in progress or finished, who gave it, where, and for which faction. Below that are all its entries, newest first, with their dates. The right stick scrolls them when they don't fit.
- **X pins the highlighted quest,** or unpins it. Pinned quests show in a Pinned group at the top of every tab, so the quests the player is working on stay in reach. The save keeps the pin on each of the game's quest IDs that share the quest's name, from save format version 43. Older saves load with nothing pinned.
- **Y jumps to the next group,** as on the other screens.
- **No topic links, no topic index, and no book of dated entries.** A topic index could come back later as a part of the Journal tab, if players miss it.

## Where the quest giver and place come from

Morrowind's quests have only a name, numbered stages with their text, and a finished flag. Nothing says who gives a quest or where.

- **From now on, the journal records it.** When a quest entry is added, the save keeps the actor the player was talking to, and the cell the player was in. Scripts that aren't run by an actor record only the cell. These two fields share save format version 42 with the recent items and spells.
- **For older entries,** the game looks through the conversations for the line that sets the quest's earliest stage. Its speaker is the quest giver, and the cell the speaker is placed in is the quest's place. Some conversations only name a faction or a cell instead of a speaker.
- **The place's town** is the start of an interior's name, before a comma or colon, such as "Balmora" for "Balmora, Guild of Fighters". An exterior cell without a name is in its region.
- **The faction** is the one in a quest's name, then the quest giver's faction, then the faction the conversation names.

Counted over Morrowind, Tribunal and Bloodmoon, about 22% of quests have no faction and about 15% have no town, from the conversations alone. Quests that scripts advance, rather than conversations, have neither until the journal records them.

## Controls

| Button | What it does |
| --- | --- |
| L1, R1 | Switch between Recent, Places and Factions. |
| D-pad up and down, left stick | Move the selection. Holding repeats. |
| X | Pin the highlighted quest, or unpin it. |
| Y | Jump to the next group. |
| Right stick | Scroll the quest's entries. |
| B | Close the menu. |
| L2, R2 | Switch between the map, inventory, magic, stats and journal. |

## The first version

`apps/openmw/mwgui/journalscreen.cpp` draws the screen. `apps/openmw/mwgui/questlog.cpp` collects the quests from the journal and works out their givers, places and factions. The `controller-journal` scenario adds quests, opens the journal with the journal key, and looks at each tab.

### Not done yet

- Marking quests that changed since the player last looked.
