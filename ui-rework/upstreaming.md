# Turning Modern Morrow into a mod

This note asks what upstream OpenMW would need to change so that Modern Morrow could ship as a Lua mod for stock OpenMW instead of as a fork. It compares what the fork's code depends on with what upstream's Lua API offers, as of upstream commit 16b14001cf (1 November 2026).

## The short answer

Modern Morrow can't be a mod today, and it couldn't become one by moving its C++ into a plugin. OpenMW has no native plugin interface. The only extension points are Lua scripts and data-file overrides. So "Modern Morrow as a mod" means rewriting every screen in Lua, against an engine that gives Lua enough to work with.

Upstream already points that way. `I.UI.registerWindow` was added so that Lua can replace built-in windows, as the first step of moving the UI out of C++ ([!3236](https://gitlab.com/OpenMW/openmw/-/merge_requests/3236)). A draft port of the Persuasion dialog to Lua ([!4954](https://gitlab.com/OpenMW/openmw/-/merge_requests/4954)) hit the same input and focus gaps that this note lists. Its author argues that Lua should call the existing C++ game rules rather than copy them. Engine changes that serve that plan are generic, and any mod could use them, so they stand a fair chance upstream. A full alternative UI in C++ probably doesn't.

The engine changes fall into five groups:

1. **Input.** Let a Lua screen own the gamepad while it is open. This is the most important change, and nothing else works well without it.
2. **Window coverage.** Give Lua a way to replace the screens it can't reach today: character creation, the count and yes/no dialogs, message boxes with buttons, the main menu and the HUD.
3. **Game rules.** Expose the C++ game rules that the old windows run. These cover dialogue, prices, crafting, resting, levelling up and character creation.
4. **Drawing.** Add text measurement, scrolling, fixed-height scaling and map textures to Lua UI.
5. **Small engine features.** A handful of changes the fork made that aren't UI at all. They can go upstream on their own merits, or be dropped.

The rest of this note goes through each group. It ends with what a mod could already do today, and an order to do the work in.

## What building it showed (November 2026)

In November 2026 we tried this plan for real. The result is a local branch, not pushed anywhere: `full` in the worktree `D:\Code\openmw-upstreaming`. It has 177 engine commits on top of upstream, grouped into 60 small planned merge requests in 7 milestones. On top of those, the whole Modern Morrow UI is rebuilt as a Lua mod in `modern-morrow/`. Every engine commit builds on its own, and upstream's tests pass, with 54 new integration tests. The mod's screens were checked in the game with a gamepad. The full write-up is `upstreaming-notes/summary.md` in that worktree. It covers the merge requests, the bug fixes found on the way, and the open design questions.

The plan held up. These are the places where it was wrong or incomplete:

- **Phase 1 wasn't all simple getters.** The item value after condition, and the barter price with its 75% cap, lived in private helpers inside the trade window. They needed a refactor first, like the phase 2 work. Two items weren't needed at all: the time left on a power (`canUsePower` already exists) and the UI scale factor (layer sizes give it).
- **Upstream removed `mwmechanics/trading.cpp` in 2024** and moved haggling into the trade window. Moving it back out for Lua goes against that cleanup, so the merge request has to explain why.
- **Phase 5 is much smaller than planned.** The count and yes/no dialogs don't need a way for Lua to answer C++. Every caller sits in a window that Lua replaces whole, such as the inventory, magic or the main menu. Only script message boxes with buttons need one.
- **A Lua main menu belongs in a menu script.** Player scripts get no input while the main menu is open. Menu scripts still lack several things, such as disabled windows that survive loading a game.
- **Dialogue and character creation needed refactors first.** The conversation's history, its rules and the topic links lived in the dialogue window. The character creation windows were rebuilt from scratch every time the UI refreshed, so a Lua mod couldn't replace them.
- **Porting the screens found about 30 more small gaps.** Examples:
  - an item's equipment slots and sounds;
  - dropping or picking up an item by the engine's rules;
  - a merchant's goods in the containers they own;
  - who gave a journal entry and where, which needs save format 42;
  - plain messages that Lua can draw;
  - rest and jail progress.

  These became milestone 7, with 77 commits. They are small, but they are real work. A full UI needs them.
- **The work was bigger in commits than estimated, but not in lines.** It came to about 10,400 lines added and 1,950 removed, including docs and tests. That sits inside the estimate below. But it is 60 merge requests rather than 20 to 30, because each one was kept to a single purpose.
- **Some things still have no Lua path**, and the mod leaves them to the engine or out entirely: the map and its fog of war, the race preview's 3D head, the settings window, tooltips and keyboard focus.

## How the fork works today, and why a mod can't copy it

Each Modern Morrow screen is a C++ "panel" class. The old MyGUI window stays alive as the model, invisible, and the panel draws over it on its own layer. The panel performs actions by calling the old window's private handlers, such as `TradeWindow::onOfferButtonClicked` or `AlchemyWindow::onCreateButtonClicked`. Many panels are declared friends of their window for exactly this reason. A barter panel, for example, writes the window's private haggle balance directly. The fork adds no Lua bindings at all.

None of that is reachable from Lua. A Lua mod gets two things instead. It can read records and live game state, which is broad and good. It can also perform a short list of actions through the engine's normal rules: using and equipping items, picking up world items with crime handling, committing crimes, training a skill, and saving or loading. Everything that involves a price, a chance or a formula would have to be rewritten in Lua from the game settings (GMSTs). That loses parity with the engine and is a lot of work.

One detail helps. When Lua replaces a window, the engine still creates the old window and still calls its `setPtr` when the mode opens. So `DialogueWindow::setPtr` still starts the conversation and restocks the merchant, and `TradeWindow::setPtr` still builds the item lists. Only the window's visibility and `onOpen` are skipped.

## 1. Input: let a Lua screen own the gamepad

**What happens upstream now.** `ControllerManager::buttonPressed` forwards each gamepad press to Lua as a queued event, then handles it itself. Lua runs later, so it can't stop the engine's handling. In a menu, `gamepadToGuiControl` does the following:

- It first offers the button to the active window's `onControllerButtonEvent`. That window is the hidden C++ one, not the Lua screen.
- Otherwise B closes the current modal or the whole menu mode.
- The d-pad becomes MyGUI arrow keys, A becomes Space, and X and Y become `;` and `'`.
- L2 and R2 always switch between the mode's windows (`cycleActiveControllerWindow`).

A Lua inventory therefore can't use B to back out of a sub-menu, because B closes the whole inventory. Its d-pad presses also move the focus in MyGUI widgets underneath it.

**What upstream would need.**

- **A way for Lua to claim controller input while its screen is open.** Lua runs on a worker thread, so a synchronous "did you handle this?" callback is awkward. A declared flag fits better. For example, `I.UI.registerWindow` could take an option meaning "this window owns controller input". While that window is active, the engine would skip `gamepadToGuiControl` and the trigger cycling, and pass buttons and axes only to Lua. The same idea applies to keyboard input.
- **Axis events, or confirmation that polling is the intended way.** Lua can already poll `input.getAxisValue` from `onFrame`, which runs while paused. That's what the fork's `HeldDirection` does in C++ anyway, so this is a nice-to-have.
- **Keyboard focus control in Lua UI.** There's no way to set focus from Lua, and mouse and keyboard focus are merged. !4954 ran into the same thing. A gamepad-only mod can do its own focus tracking, but keyboard users of the same screens need this.
- **Disabling engine actions.** Quick keys on the d-pad during play need the engine to leave the d-pad alone. Lua can only override movement, combat and three UI actions: Inventory, Journal and QuickKeysMenu. The fork's `isControllerButtonBound` check, which skips directions the player has bound, would become a Lua query.

## 2. Window coverage

`registerWindow` accepts 24 windows today: Inventory, Magic, Map, Stats, Journal, Trade, Dialogue, Container, Companion, Alchemy, EnchantingDialog, SpellCreationDialog, Repair, Recharge, MerchantRepair, SpellBuying, Training, Travel, WaitDialog, LevelUpDialog, JailScreen, Book, Scroll and QuickKeys.

Modern Morrow also redraws these, which Lua can't replace:

| Screen | What's missing upstream |
|---|---|
| Character creation: name, race, class pick, class questions, custom class, birthsign, review | No window IDs, and no Lua API to set name, race or class, or to finish character creation (see section 3). |
| Count dialog, yes/no confirmation | No window IDs. The magic screen uses the confirmation dialog to delete spells, and dropping items uses the count dialog. |
| Message boxes with buttons, from game scripts | No window ID. Lua's `I.UI.showInteractiveMessage` only shows a single OK button and returns no answer. |
| Plain messages | Lua can show messages but can't restyle the engine's own. The fork moves them to a larger, centred style. |
| Main menu and pause menu | Not replaceable. Menu scripts can list, save, load and delete saves, but only draw over the C++ menu. |
| Settings window | Lua can add pages inside it but can't replace it. The fork drives the old widgets by firing their events, which is the least portable thing it does. |
| HUD | Only `I.UI.setHudVisibility`, which hides the whole HUD, including messages and tooltips. `registerHudElement` exists only as a TODO comment in `ui.lua`. |
| Tooltips, loading screen | Not replaceable. |

The useful upstream changes are window IDs for the dialogs and character-creation screens, and per-element HUD control. The HUD would need either per-element visibility or the planned `registerHudElement`, so that a mod can draw its own bars and effect icons while the engine keeps showing messages.

## 3. Game rules Lua can't call

This is the largest group. For each area, the second column is what a Lua screen needs to call. Each binding should wrap the existing C++ rather than copy its formulas into Lua.

| Area | Bindings needed | C++ that already does it |
|---|---|---|
| **Dialogue** | List the topics this NPC offers. Ask a topic and get the response text with `%PCName` and similar substituted. Get the current choices and pick one. Say goodbye. Persuade with admire, intimidate, taunt or bribe, and get the result. Get the services list, after the service refusal check. | `DialogueManager::keywordSelected`, `questionAnswered`, `persuade`, `goodbyeSelected`, and the topic list in `DialogueWindow` |
| **Barter** | Get the price of an offer, with Mercantile, disposition and haggling. Accept or refuse an offer with the engine's rules, including the haggle roll, the disposition change and skill gain. Check whether an item is stolen, and from whom. | `MechanicsManager::getBarterOffer`, `TradeWindow::onOfferButtonClicked`, `getStolenItemOwners` |
| **Services** | Prices and actions for training, travel, spell buying and merchant repair. Training would check the skill caps, and travel would take followers along. | The same `getBarterOffer`, plus each service window's handler |
| **Containers** | Take an item from a container with the engine's ownership and crime rules. Get the pickpocket chance. | `MechanicsManager::itemTaken`, `isAllowedToUse`, `PickpocketItemModel` |
| **Magic** | Effective spell cost, success chance, enchantment cast cost, and the time left on a power. | `calcSpellCost`, `getSpellSuccessChance`, `getEffectiveEnchantmentCastCost`, `Spells::canUsePower` |
| **Alchemy** | Which ingredient effects the player can see, a preview of the potion, and brewing. | the `MWMechanics::Alchemy` class |
| **Enchanting** | Enchant points, cost, chance and price, and creating the item. | the `MWMechanics::Enchanting` class |
| **Spellmaking** | Cost and price of a custom spell, and creating it. | the logic in `SpellCreationDialog` |
| **Repair, recharge** | Repair amount and chance, recharge amount, and using the tool. | the `MWMechanics::Repair` and `Recharge` classes |
| **Rest and wait** | Whether the player can rest here. Rest or wait with the engine's healing, interruptions and level-up prompt. | `World::canRest`, `Actors::rest`, `WaitDialog` |
| **Level-up** | Apply a level-up with three chosen attributes. | `NpcStats::levelUp` and `LevelupDialog` |
| **Character creation** | Set the name, race and class, including a custom class. Run the class questions. Finish character creation. | `MechanicsManager::setPlayerName`, `setPlayerRace`, `setPlayerClass`, and `GenerateClassResultDialog` |
| **Effect text** | The engine's description of a spell effect as a string, such as "Fortify Strength 10 pts for 30 secs on Self". | `describeSpellEffect()`, which the fork split out of `MWSpellEffect` in `widgets.cpp` |
| **Saves** | Each save's screenshot, cell name and in-game date. Whether saving is allowed right now. | `ESM::SavedGame`, `StateManager` |
| **Messages** | A message box with buttons that returns which button was pressed. | `MessageBoxManager::createInteractiveMessageBox` |

Dialogue and character creation are the hard blockers. Lua can't drive them at all, and their rules are too tangled to copy. Barter, crafting and resting could in theory be copied from the GMSTs, but the result would drift from the engine.

## 4. Drawing

Lua UI covers most of what the panels draw. It has text, rectangles, images from any data-file path (including item and spell icons), flex layout and new layers placed anywhere among the built-in ones. The gaps are:

- **Text measurement.** The panels wrap and lay out text using font metrics, for example in `ControllerUi::wrapText`. Lua can't measure text or read back an element's size after layout. *Built in the attempt:* `ui.measureText` lays the text out with a hidden widget that uses the Text widget's skin, so the size matches what the widget draws. It only works on the main thread.
- **Scrolling.** There's no scroll view. A mod can offset children inside a fixed parent if MyGUI clips them, but nobody has tested that. *Tested in the attempt:* MyGUI does clip them, so no new API is needed.
- **A fixed-height layer.** The fork adds a `Height` property to `ScalingLayer`. It gives a canvas that is always 720 units tall and as wide as the screen's aspect ratio. This makes TV and handheld scaling come out right. It would be a small generic change: an option on `ui.layers.insertAfter` and a `ui` call to read the scale.
- **Map textures and fog of war.** The local and world maps are render targets, not files, so Lua can't show them. The fork keeps the built-in map window and only adds stick panning, tab switching and full-screen fitting. A mod could do the same if the controller changes went upstream into the built-in map window. Otherwise Lua would need the map textures, the explored cells and the markers.
- **The race preview.** Character creation shows a rendered 3D head. Lua would need a way to show that render target.
- **Book text.** The reading panel lays out book markup with `BookFormatter`. A mod could parse the book's HTML-like text itself, but an engine call that turns book text into plain paragraphs would avoid a second, slightly different parser.

## 5. Engine changes that aren't UI APIs

The fork made a few changes outside the screens. Most wouldn't be needed as a mod, or could go upstream on their own merits:

| Fork change | As a mod |
|---|---|
| Saves record recent items and recent spells for the Recent tabs (save format 42). | Not needed. Lua scripts can store their own data in saves through `onSave` and `onLoad`, and can watch inventory and spell changes themselves. |
| Quest entries record who gave them and where (save format 42). | Mostly not needed. A Lua script can note the dialogue partner and cell when a journal entry appears. An engine field would be more accurate for entries added by game scripts. |
| Dialogue remembers answers heard this session. | Becomes part of the dialogue bindings. |
| A, B and Start skip videos. | A small generic fix, which can go upstream as is. |
| The `local map cell distance` setting, terrain loading out to the map corner, and the sky blending clip change. | General engine features, independent of the UI. Each can go upstream on its own or stay as a fork patch. |
| `controller menus = true` by default. | Not needed if Lua screens own the input. A mod can't change engine defaults anyway. |
| The HUD is hidden in menus and dialogue. | Lua can already do this with `setHudVisibility`. |
| The camera turns to the NPC's face in dialogue. | Already possible in Lua today. `dialogue_framing.lua` only uses upstream camera calls. As a mod it would run from its own `onFrame` instead of hooking `camera.lua`. |
| Separate settings and save folders, and the package names. | Not needed. |
| The automation harness (`mwautomation/`). | A development tool, not part of the mod. It could stay a private patch for testing, or be offered upstream separately as a test harness. |

## What a mod could already do today

Against stock upstream, a Lua mod could already ship:

- The **inventory, magic, stats and journal screens**, for display. Equipping, using items and choosing spells go through the engine's normal path. Deleting spells and dropping items would need its own count and yes/no dialogs.
- **Quick keys on the d-pad**, with the mod's own slot storage, as long as the d-pad isn't bound to anything else.
- **Book and scroll reading**, with the mod's own markup parser.
- **The dialogue camera.**
- **A save and load list**, from a menu script, drawn over the main menu.

Without the input change in section 1, though, each of these screens has to put up with B closing the whole menu and the d-pad moving focus underneath it. In practice the input change has to land before any screen can move into the mod.

## How big the upstream work would be

These figures count only the engine changes upstream would review. They leave out the mod's own Lua screens. They are estimates, measured against past upstream merges of the same kind:

| Past upstream change | Size |
|---|---|
| `I.UI` and `registerWindow`, "Control GUI modes from Lua" (2023) | 490 lines over 46 files, mostly a one-line window ID in each window class |
| The player journal API (2025) | 360 lines, about half of it API docs |
| Barter gold bindings (2025) | 29 lines |
| The `DialogueResponse` event (2026) | 58 lines |
| The use-item handlers (2023) | 210 lines |
| The C++ controller menus, [!4710](https://gitlab.com/OpenMW/openmw/-/merge_requests/4710) (2025) | 4,500 lines over 116 files, in one merge request |
| All of `apps/openmw/mwlua` today | 20,000 lines |

| Piece | Rough size | Kind of work |
|---|---|---|
| Input ownership and focus control | 400–700 | New design. It has to fit the threaded Lua model and both keyboard and gamepad. |
| Window IDs for more windows | 150 | Mechanical, copying the existing one-liners. |
| Lua-owned modal dialogs that return an answer to C++ (count, yes/no, message boxes with buttons) | 300–500 | New design. The C++ code that opens these dialogs waits for an answer through callbacks. |
| Per-element HUD control, a replaceable main menu and loading screen, Lua access to engine settings | 600–1,100 | Mostly mechanical, with some design for engine settings. |
| Magic numbers, effect text, save details, message answers | 400–700 | Mechanical: wrapping functions that already exist. |
| Crafting: alchemy, enchanting, repair, recharge | 500–900 | Mostly mechanical. These classes already live in `mwmechanics`, separate from the GUI. |
| Barter, haggling and the four services | 800–1,200 | Refactoring. The offer, haggle and service logic sits inside the GUI windows and has to move into the game rules first. |
| Containers, rest and wait, level-up, spellmaking | 800–1,300 | Refactoring, for the same reason. Rest is the trickiest, because waiting runs over several frames. |
| Character creation | 400–700 | Refactoring plus design. It is a step-by-step flow with back and next. |
| Dialogue | 800–1,500 | The hardest piece. The dialogue manager already reports to an interface (`ResponseCallback`), which helps. But topic links, choices, goodbye and persuasion all have to work as Lua events. |
| Text measurement, scroll views, fixed-height layer | 500–800 | Some design. Measuring text from the Lua thread is awkward. |
| Map textures, fog of war, markers, the race preview | 700–1,400 | New design around render targets. This can be skipped by keeping the built-in map. |
| API docs in `files/lua_api` and integration tests | about a third on top | Mechanical, but required. |

The total is about 8,000–12,000 lines of C++ and Lua bindings, plus 3,000–4,000 of docs and tests. That is two to three times the C++ controller menus merge, or about half the size of all of today's Lua bindings. Spread over 20 to 30 merge requests, the typical one would be 200–600 lines.

About half the lines are mechanical: window IDs, wrappers around existing functions, and docs. They're quick to review. About a third is moving game logic out of GUI windows. The lines are simple, but reviewers have to check that the original game's behaviour is unchanged. The remaining sixth is new design: input ownership, modal answers, dialogue, text measurement and render targets. That sixth is where most of the review time would go. Lua API additions are also a lasting promise to mod authors, so maintainers have good reason to be careful with them.

## Splitting it into small merge requests

Upstream merged 141 merge requests that touched the Lua API between November 2025 and October 2026, about 12 a month. The median one changed 81 lines, and three quarters changed fewer than 265. Two regular contributors wrote most of them. A small, single-purpose Lua change is therefore the normal unit of work upstream.

Most of this project splits into pieces that size, and each piece is useful to other mods without Modern Morrow. That makes about 50 merge requests in all. Roughly 35 are ordinary small changes. About 15 need maintainers to agree on an approach first, and those agreements come down to five conversations: input, dialogs, dialogue, character creation and Lua UI widgets.

Fifty merge requests is about four months of upstream's entire current Lua merge rate. The limit is the maintainers' time, not the size of each change. Even small Lua API changes get careful review of names, of which kind of script may call them, and of the rule that actions are queued for the main thread. Every merged API is also something upstream must keep working for mod authors. At a pace that doesn't crowd out everyone else, perhaps two to four merges a month, this would take one to two years.

## The order to do it in

The order follows two rules. First, a piece comes after everything it depends on. Second, among pieces that are ready, the least debatable goes first. The early phases are small, read-only and obviously useful to any mod. They teach us upstream's review habits and build trust before we ask for anything that needs a design decision.

The two rules pull against each other in one place. Input ownership is what every Modern Morrow screen waits on, but it is also a real design decision. So we open the conversation about it early, as a GitLab issue during phase 2, while the easy code keeps flowing. The code comes in phase 3, once there is agreement.

Logic that lives inside a GUI window always goes in two merge requests. The first moves the logic into the game rules with no change in behaviour. The second adds the Lua binding. Each is easy to review on its own.

Sizes include docs and tests. Each Lua API change also bumps `OPENMW_LUA_API_REVISION`, which upstream does in its own commit.

### Phase 1: small fixes and read-only getters

No design questions, nothing changes game behaviour, and each is 30–150 lines. In order:

1. **Controller buttons skip videos.** A plain fix with no API, and a gentle first contact with the review process.
2. **Spell cost and success chance.** These wrap `calcSpellCost` and `getSpellSuccessChance`, which the engine already computes for the HUD.
3. **Enchantment cast cost** and **time left on a power.**
4. **An item's value after condition, charge and soul.**
5. **The barter price of an offer.** This wraps the existing `getBarterOffer`. It is read-only, and it is the first piece of the barter screen.
6. **Whether an item is stolen, and from whom.**
7. **Whether the player can rest here.** This wraps `World::canRest`.
8. **The cell name and in-game date in the save list.** These are already in each save's header.
9. **The UI scale factor**, and **whether a controller button is bound to an action.**

### Phase 2: logic moved out of GUI windows, and its bindings

Each item is a refactor with no behaviour change, followed by a binding, at 50–300 lines per merge request. These touch the original game's rules, so they come once reviewers know us. They run simplest first:

1. **Training price and caps**, **spell buying price** and **merchant repair price.** All three are short formulas around `getBarterOffer`.
2. **Travel price and the trip itself**, including followers.
3. **The engine's spell effect description as a string.** This splits `describeSpellEffect()` out of `MWSpellEffect` in `widgets.cpp`, as the fork already does.
4. **Taking an item from a container with the engine's crime rules**, and **the pickpocket chance.**
5. **Repair and recharge.** They wrap classes that already sit apart from the GUI.
6. **Enchanting**, then **alchemy.** These wrap the larger `Enchanting` and `Alchemy` classes. They need an agreed shape for an ongoing crafting session, which repair and recharge will have set a pattern for.
7. **Applying a level-up.**
8. **Accepting a barter offer with the haggle roll.** This moves the offer logic out of `TradeWindow::onOfferButtonClicked`.
9. **Spellmaking cost, price and creation.**
10. **Resting and waiting.** This goes last because waiting runs over several frames, with healing and interruptions along the way.

**During this phase, open the design issue for input ownership.** It can cite the Persuasion draft (!4954), which hit the same problem, and show the fork's screens as a working example.

### Phase 3: input ownership

This is the first design-led phase, and it unlocks every Modern Morrow screen.

1. **A flag that lets a Lua window take controller input.** While that window is active, the engine skips its own menu handling of buttons, axes and triggers. This is perhaps 150–250 lines once the design is agreed.
2. **The same for the keyboard.**
3. **Focus control in Lua UI**, with keyboard focus kept separate from mouse focus.

### Phase 4: Lua UI widget gaps

These are design-led too, but they are widget work that also helps upstream's own Lua settings pages.

1. **A fixed-height layer option for `ui.layers`.** This is the fork's 720-unit canvas, and it is small.
2. **Text measurement.**
3. **A scroll view.**

**Milestone: the first screens move from the fork into the mod.** Inventory, magic, stats, journal, reading and quick keys need only phases 1 to 4. The inventory can draw its own count picker for dropping items, because the mod opens that picker itself. Barter, training, travel, spell buying, repair, recharge, enchanting, alchemy, level-up and waiting can follow, since phase 2 gave them their game rules.

### Phase 5: dialogs that return an answer, and the HUD

1. **The mechanism for a Lua dialog that answers C++ code.** This is the design piece.
2. **The count dialog**, **the yes/no dialog** and **script message boxes with buttons**, one merge request each, at 50–100 lines.

   *The attempt showed that only script message boxes need this.* The count and yes/no dialogs are only opened by windows that a mod replaces whole, so the mod draws its own.
3. **Per-element HUD control.** This is either upstream's planned `registerHudElement` or per-element visibility.
4. **Window IDs for the main menu and the loading screen.**

**Milestone: the HUD, the modal dialogs and the pause and save screens move to the mod.**

### Phase 6: dialogue

This is the largest design conversation, so it comes when trust is highest. It splits along the conversation:

1. List the topics this NPC offers, read-only.
2. Ask a topic and get the response as an event.
3. Choices and goodbye.
4. Persuasion, building on !4954.
5. Services, with the refusal check.
6. A helper that finds topic links in response text.

**Milestone: dialogue and persuasion move to the mod.**

### Phase 7: character creation

1. **Setters for the player's name, race and class.** The C++ setters already exist.
2. **The custom class record.**
3. **The class questions.**
4. **Window IDs for each step, with back and next.**

**Milestone: character creation moves to the mod.**

### Phase 8, optional: map and preview textures

Local and world map textures, fog of war and markers, plus the race preview. This can be skipped. A cheaper route is to send the fork's controller changes to the built-in map window, such as stick panning and tab switching, as ordinary C++ improvements to upstream's controller menus.

**Milestone: the fork has no screens left.** What remains is the packaging, the save folders and the automation harness, none of which a mod needs.

Until each milestone, the fork carries the same patches that are under review upstream, and it gets smaller every time one merges.

Two costs are worth stating plainly. First, rewriting the screens in Lua is close to rewriting the project's UI. The panels are about 15,000 lines of C++. The layout and design decisions carry over, but the code doesn't. Second, upstream already ships an opt-in C++ controller mode (`controller menus`, from [!4710](https://gitlab.com/OpenMW/openmw/-/merge_requests/4710) and [!4741](https://gitlab.com/OpenMW/openmw/-/merge_requests/4741)). Some of the fork's less opinionated pieces could improve that mode directly, instead of waiting for the Lua path. Examples are the map stick panning, video skipping and the larger messages. It's worth asking upstream which way they'd prefer before starting either.
