# Plan

## Goal

Rework OpenMW's in-game UI so that it:

- looks modern,
- can be used entirely with a gamepad, with no mouse cursor,
- is readable on a TV from across the room and on a 1280x800 handheld screen such as the Steam Deck.

Getting something that looks and feels good comes first. Making the changes clean enough to send upstream is a possible later step, not a requirement. Once we have a UI we like, we will work out how OpenMW could support it cleanly.

## Ways of working

- Work happens on the `ui-rework` branch.
- Every commit must build. The unit tests don't need to run on every commit. Run them when the work reaches a good stopping point.
- Fast iteration matters more than thorough validation. Every so often, review how long one iteration takes, and look for ways to speed it up. Examples include building fewer targets or configurations, and running fewer checks on the hot path. Validation tends to grow over time, so this review needs to happen more than once.
- The project alternates between working sessions together and stretches where the agent works alone. The user says when the agent should work alone.
- The agent may use subagents and git worktrees to work on independent pieces in parallel.
- No software is downloaded or installed from the internet without the user's go-ahead. Web research is allowed. The user has approved the Android tools and the reference copies of the community ports. See [android.md](android.md).

## Phases

### Phase 1: automation harness (done)

Build the tools an agent needs to iterate on the UI without help:

- launch the game in a known state, at a chosen resolution, without touching the user's own settings or saves,
- send keyboard, mouse and gamepad input,
- read the state of the UI as text,
- take screenshots straight from the game's renderer,
- read the game's log output.

See [harness.md](harness.md) for the design and the order of the work.

Phase 1 is done when the agent can run a script that opens each major UI screen at each target resolution, and get back a screenshot and a text dump of each one.

### Phase 2: rework the UI one piece at a time (current)

Go through the pieces listed in [screens.md](screens.md), discussing each one with the user and iterating on it with the harness. The games in [research/gamepad-ui-references.md](research/gamepad-ui-references.md) are a reference for each piece.

Done so far:

- **HUD.** The minimap and weapon name text are gone. The rest is gathered in the bottom left corner, sized relative to the screen height, and fades out when it has nothing to show. Effects that last for good, such as from race and birthsign, leave no icon.
- **Inventory.** A new screen with tabs for recent items, equipment by body area, consumables, notes, tools and misc. Every list shows names and stats, and a details panel compares items with the equipped ones. See [design/inventory.md](design/inventory.md).
- **Map.** The bottom button bar is gone from all menus. The map has Local and World tabs like the other screens, fills the screen below them, shows its own button hints, pans with the right stick, and shows 7x7 cells when distant terrain is on.
- **Loot.** An opaque panel on the right side of the screen lists a container's items, grouped by kind, with upgrades marked and a details panel that compares with what's worn. Stealing is spelled out. See [design/loot.md](design/loot.md).
- **Barter.** The loot panel's look, with Buy and Sell tabs, prices on every entry, a deal box, and haggling on the right stick. See [design/barter.md](design/barter.md).
- **Magic.** A new screen with tabs for recent spells, an Active tab with the effects on the character and the spell ready to cast, and tabs for spells, powers, magic items and scrolls. Every entry writes out its effects. See [design/magic.md](design/magic.md).
- **Dialogue.** An opaque panel along the bottom of the screen, with the NPC in view above it. New topics come first, services have their own tab, and replies to choices replace the topics. See [design/dialogue.md](design/dialogue.md).
- **Pause menu.** The start button's menu during a game lists Save, Load, Options and Exit in a panel over a darkened view of the game. B returns to the game.
- **Stats.** A new screen with Character, Skills and Factions tabs. The Character tab shows the character as tiles, with each attribute's bonus at the next level up. Everything can be selected for its details. See [design/stats.md](design/stats.md).
- **Journal.** A fifth tab of the menus lists quests, the ones in progress first, sorted by the last update, by town or by faction. The details panel shows who gave a quest, where, and its entries, newest first. See [design/journal.md](design/journal.md).
- **Quick keys.** The d-pad uses them during play: a press of a direction for slots 1 to 4, and a hold for slots 5 to 8. Clicking the left stick on the inventory or magic screen assigns the highlighted item or spell, and a marker shows what is on each. See [design/quick-keys.md](design/quick-keys.md).
- **Alchemy.** A full screen with the ingredients and their known effects on the left, grouped by whether they share an effect with the potion, and the potion on the right. See [design/crafting.md](design/crafting.md).
- **Enchanting.** A full screen with the item, soul gem, cast type, effects and costs on the right, and a list on the left to pick each from. Effects are edited on the left while the costs follow along. See [design/crafting.md](design/crafting.md).
- **Spellmaking.** The enchanting screen without the item, soul gem and cast type. A new spell is named after its first effect. See [design/crafting.md](design/crafting.md).
- **Repairing and recharging.** One full screen with the items that need it and the tool in use. Y switches tools, and the next tool takes over when one runs out. See [design/crafting.md](design/crafting.md).
- **Everything else.** The merchant services, questions and confirmations, rest and level up, books, saving and loading, the count dialog, companion sharing, loading and jail, character creation and the options all have controller panels in the same style. See [design/other-screens.md](design/other-screens.md). Not reviewed yet.

### Phase 3: make the changes clean enough to upstream

Once the UI looks and feels right, work out how OpenMW could support it cleanly. Not a priority yet.

## Target screens and resolutions

These are the screen sizes the harness should cover:

| Name | Resolution | Represents |
| --- | --- | --- |
| `deck` | 1280x800 | Steam Deck and similar handhelds |
| `tv1080` | 1920x1080 | 1080p TV |
| `tv4k` | 3840x2160 | 4K TV |
| `fold` | 2448x1848 | Galaxy Z Fold 8 main screen, 7.6" at about 403 ppi, 4:3 |
| `fold-cover` | 1972x1248 | Galaxy Z Fold 8 cover screen, 5.5" at about 428 ppi, 16:10 |

Published guidance gives some numbers to check designs against:

- **Steam Deck:** Valve requires the smallest text to be at least 9 px tall at 1280x800, and recommends 12 px.
- **Phones:** the Deck numbers, kept at the same physical size, come to about 17 px (22 px recommended) on the Fold's main screen and 18 px (24 px recommended) on its cover screen. These screens have nearly twice the Deck's pixel density, so the UI needs a scaling factor near 2 there.
- **Xbox:** Microsoft's guidance for console text is at least 26 px for body text at 1080p, or 52 px at 4K.
- **TV:** keep important content inside the "TV-safe" area, which is inset 48 px from the left and right and 27 px from the top and bottom at 1080p.
- **Focus:** the widget with controller focus must be clearly visible from across the room.
