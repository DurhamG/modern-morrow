# Quest markers (experiment)

GitHub issue 2 asks for map markers for every kind of quest. This is an experiment on the `quest-markers` branch, to find out whether markers can be right often enough to be worth having. It starts with the markers players know from Skyrim. A more Morrowind-like version can come later, once there's something to play with.

This page is the plan. The numbers in it come from a throwaway analysis of Morrowind.esm, Tribunal.esm and Bloodmoon.esm.

## 1. Where a quest goes next

Morrowind's journal keeps one number for each quest, its stage, such as 10 for "Eydis Fire-Eye told me to talk to Drarayne Thelas". Nothing in a save says where the player should go next. That has to be worked out from the game's content, which is possible because something in the content always moves a quest from one stage to the next.

### What moves a quest on

- **A line of dialogue.** Its result script runs `Journal FG_RatHunt 50`. The line has a speaker, and conditions that say when it can be said, such as the quest's stage, a dead actor, or an item the player carries.
- **An object's script.** A script on an object runs `Journal ...` when something happens to the object, such as the actor dying, the player activating it or picking it up, or the player coming near it.
- **A global script.** It runs `Journal ...` from no object in particular, usually after checking named objects, such as whether an actor is dead.

### How often this finds a target

The analysis looked at every quest stage that isn't finished and isn't the quest's last: 2,208 stages in 755 quests. For each one it took the ways forward that the stage allows, and kept the ones that lead to the next-lowest stage. Then it asked what kind of target they give:

| Target | Stages | Share |
|---|---|---|
| Talk to a person placed in the world | 1,548 | 70.1% |
| An object whose script moves the quest on when it dies | 198 | 9.0% |
| Kill a placed actor (a dialogue line needs them dead) | 123 | 5.6% |
| Activate, approach or pick up a placed object | 116 | 5.3% |
| Fetch an item placed in three places or fewer | 40 | 1.8% |
| A global script that names placed objects | 29 | 1.3% |
| Only an area: a faction, a class, or a cell | 78 | 3.5% |
| A global script with no object, or an unplaced object | 35 | 1.6% |
| Nothing found | 37 | 1.7% |
| Fetch a common item | 3 | 0.1% |

So about 93% of stages give a concrete object to mark, and about 3.5% give an area. Most of the stages with nothing are notes, test entries, or quests that end on an unmarked stage.

### Choosing among the ways forward

A stage usually has several ways forward, and some are failures, such as "the quest giver died". The first rule tried:

1. Keep the dialogue lines whose conditions on this quest allow the current stage, and every script that sets a higher stage.
2. Keep the ones that lead to the lowest higher stage.
3. Turn each into what the player has to do:
   - **Kill.** The line needs an actor dead, and that actor doesn't speak in this quest. An actor who does speak in it, such as the quest giver, is a failure branch.
   - **Fetch.** The line needs an item, the item is placed three times or fewer, and the player doesn't have it.
   - **Talk.** Otherwise, the line's speaker.
   - **Go to an object.** For a script, the object it's on.

Checked by hand, this gets these quests right at every stage:
- "Fighters Guild: Exterminator": talk to Drarayne Thelas, kill the rats in her house, then report back.
- "Antabolis Informant": talk to Hasphat Antabolis, take the puzzle box from Arkngthand, then return.
- A Morag Tong writ: the target in Shara.

It isn't always what Skyrim would show. In "Report to Caius Cosades", the lowest stage is a hint: Elone in Seyda Neen gives directions, then a patron of the South Wall cornerclub, then Bacola Closcius, then Caius himself. Skyrim would point straight at Caius. Candidate rules:

- **A.** The lowest next stage only. This follows the journal's hints, one person at a time.
- **B.** Every way forward. Elone, Bacola and Caius would be marked together.
- **C.** The way forward that reaches furthest, with the lowest as a smaller "lead" marker.

The first step of the work is to label about 60 stages by hand with what a player would expect, from main quest, guild, Great House, side and expansion quests, then measure each rule against them.

### Checks while playing

The content says what could move a quest on. The game state then rules some of it out:

- A target actor who is already dead is no longer a target, unless the task is to kill them.
- A fetch is done once the player carries the item, and the marker moves on to the person to give it to.
- A script on a dead actor or a removed object can't run any more, so it isn't a way forward.
- A faction or class speaker becomes the quest giver when they match, and otherwise the nearest placed member.
- A cell becomes an area on the map.

### Where things are

`ESMStore::getRefCells` already lists the cells that place each object. The marker starts at the object's placed position. 105 of the 613 people that quests send the player to (17%) can be moved by a script or a conversation, such as a follower or someone who travels. So when the game has a live or saved copy of the object, its real position wins.

### Mods

The analysis runs on whatever content files are loaded, when the game starts, so quests from mods work to the extent they're built the same way. Quests written in OpenMW Lua don't use the journal's content this way and aren't covered. The result is kept for the content list, so later starts don't redo it.

## 2. Getting there: doors

486 of the 612 placed quest speakers (79%) are indoors, so pointing at the right door matters more than pointing at the target. The content has 3,619 doors that lead elsewhere, between 2,887 cells. That's small enough to search in well under a millisecond.

- The route goes from the player's cell to the target's cell through the fewest doors. All exterior cells count as one connected place.
- Outdoors, a marker for an indoor target goes on the door the route uses. Indoors, it goes on the door out.
- On the world map, an indoor target shows at the exterior door of its route.
- Some places can't be reached by a door, such as Mournhold, which Asciene Rane in Ebonheart's Grand Council Chambers teleports the player to. A target there gets its map marker but no door. Solstheim is part of the same exterior, so it's reachable, though the route ignores boats. Travel services can come later.

## 3. What the player sees

The first version copies Skyrim, since that's what people asking for markers expect.

- **Tracking.** Each quest can be tracked or not. A quest that starts or moves on is tracked automatically, as in Skyrim, and a setting can turn that off. On the controller journal, a button tracks or stops tracking the highlighted quest, and another shows its target on the map. Tracking is separate from pinning, which only orders the list.
- **Compass.** A strip at the top of the HUD with north, east, south and west, and a marker for each tracked target. The marker points at the target, or at the first door on the route to it. Several targets give several markers.
- **Maps.** A quest marker on the local map, both 2D and 3D, and on the world map. It shows the quest's name and newest entry. An area target is a circle rather than a marker.
- **Settings.** Quest markers on or off, and the compass on or off.

A more Morrowind-like version could come later, once this one has been played. For example, it could mark only places the journal's text names, give rough areas instead of exact points, or drop the compass.

## 4. Saves

The tracked quests are saved in Modern Morrow's `UIST` record. That needs a new save format version and a round-trip test, as `CLAUDE.md` says.

## 5. Order of work

1. The analysis in C++, as plain functions over the content's records, with Google Tests on small made-up quests. A harness command lists every stage's targets so they can be reviewed in bulk. Then the hand-labelled stages, and a choice of rule.
2. Finding where targets are while playing, and the door routes, with tests.
3. Tracking: the journal buttons, the automatic tracking, and saving.
4. Map markers.
5. The compass.
6. A scenario that follows a quest through several stages and checks the markers.

## 6. Risks

- **Wrong markers are worse than none.** A marker on the wrong person sends the player across the island. Targets the rules aren't sure of could show as areas, or not at all.
- **Spoilers.** A marker can give away where something is before the journal says so, such as a hidden kill target. Skyrim does the same, but a Morrowind-like version may want to wait until the journal names the place.
- **Time.** The Python analysis takes about a second for the three official files, and the C++ version should be much faster. It can run on a background thread at load.

## 7. Comparison with existing mods

Several mods already mark quests. They were researched separately, and their reports were only read after the design above was written, so that it wasn't shaped by them. No code was read closely or copied. The research read mod pages and individual source files on GitHub, and downloaded nothing.

### The mods

- **Quest Guider**, by Diject, for MWSE, and **Quest Guider Lite**, its OpenMW version. This is the main one, at 210 endorsements for the OpenMW version. A separate .NET program, Quest Data Builder, reads the player's content files and writes the quest data. The mod then shows markers and has its own journal window. The MWSE version runs the program from the game, which freezes it for 15 to 45 seconds. On OpenMW, the player runs it by hand.
- **Quest Markers Plus.** A fork of an older Quest Guider Lite that adds a Skyrim-style compass. Players report markers that need a reload to appear, and finished quests that stay tracked.
- **Comprehensive Objective Mod.** It has a hand-written objective for each stage of 3,161 quests, written from UESP's walkthroughs and apparently mostly by an AI agent. It checks that data against what the content files say moves each quest on. It's a week old.
- **OpenMW Quest Markers**, by mildbyte. Hand-written objectives for part of the main quest only, with routing that includes travel services.
- Journal mods that name people and places without marking them, such as Questman, Quest Log Menu and Smart Journal.

### Where they agree with this design

Quest Guider works out objectives the same way this design does, which suggests the approach is the natural one:
- It finds every `Journal` command in dialogue result scripts and in scripts.
- It turns the conditions around each command into requirements, such as talk to someone, have an item, or kill someone.
- It marks the objects those requirements name, at their placed positions, or at their live positions when they're loaded.
- For an indoor target, it searches through doors to the exterior entrance.
- It hides markers for items the player already has and actors already killed.

### Where they go further

These are worth adopting, in rough order of value:

1. **Conditions inside scripts.** Quest Guider parses the `if` blocks around a `Journal` command, so a script's `Journal` gives "kill this" or "have this item" rather than only "go to the object the script is on". It also follows a variable back to where it's set, so a condition on a flag becomes whatever sets the flag. Modern Morrow has the engine's own mwscript compiler, which can do this more reliably than their hand-written parser.
2. **Conditions implied by dialogue order.** The game says the first line in a topic whose conditions all pass. So if a higher line is the same but also needs an item, this line only says when the player doesn't have it. Quest Guider adds that reversed condition. It also follows dialogue choices back to the line that offered them.
3. **Items carried, not only placed.** An item in a container or an actor's inventory, including through leveled lists with their chances, is a target too. The analysis above only counted items lying in the world.
4. **`PositionCell` in scripts** as extra places where a moved actor may be. This is cheap, and it helps the 17% of quest speakers that can move.
5. **Disabled objects.** An actor or door that's placed but disabled until the quest enables it is no target yet. Comprehensive Objective Mod also drops route doors that aren't open yet, and says so.
6. **Several journal IDs as one quest.** The main quest is split into many IDs, such as `A1_1_FindSpymaster` and `A1_2_AntabolisInformant`. The controller journal already groups quests by name, so tracking can follow that.

### Where this design is ahead

- **No separate program.** Every mod that finds objectives does the work outside the game, because Lua can't read every cell quickly, and reading cells bloated saves before OpenMW 0.52. In the engine, the analysis can run at load in C++ against the records already in memory. It would cover every content file without the player doing anything, and could rerun whenever the content changes.
- **The real maps.** OpenMW gives Lua no way to add markers to its own maps, so Quest Guider Lite needs a separate rewritten map, Advanced World Map. Modern Morrow can draw on its own local, 3D and world maps.
- **Live positions** from the engine for any object, instead of attaching a script to each tracked object as it loads.
- **Measuring the rule.** Quest Guider's author says it marks what *may* be needed and doesn't know the order, so "it's not like the markers in Oblivion or Skyrim". Players see several candidates at once. Comprehensive Objective Mod gave up on working it out and wrote every stage by hand. Labelling stages and measuring the rules, as planned in section 1, is the step neither took. It decides whether Skyrim-style single markers are possible, or whether the honest answer is several markers.

### What the player sees in them

Ideas to take into the Skyrim version:

- **Above and below.** An arrow on the marker when the target is above or below the player, as Quest Guider and Comprehensive Objective Mod do.
- **Pinned to the edge.** A marker off the edge of the compass or map stays at the edge, pointing the way.
- **A line of objective text.** Quest Guider writes each requirement as a sentence, such as "Talk to Caius Cosades" or "Kill Tirer Belvayn". Skyrim shows objective text too. The compass or journal could show the tracked quest's objective as one line.
- **Doors that say what's behind them.** A door's tooltip names the quest targets behind it.
- **A colour for each quest** when several are tracked.
- **Rough areas.** Quest Guider's optional mode draws a translucent circle offset at random instead of an exact pin. That fits the area targets here, and the more Morrowind-like version later.
- **Spoilers.** mildbyte's mod deliberately doesn't point at a place until the player has solved the riddle that names it, as in "Path of the Incarnate". A Morrowind-like version could hold back markers in the same way.

Their known failures are also worth guarding against:
- Finished quests that stay tracked.
- Markers that only appear after a reload.
- Markers on items that were already picked up.
- Markers on common items, which every mod caps at 6 to 25 placements.

### Changes to the plan

- Step 1 of the work adds conditions inside scripts, conditions implied by dialogue order, dialogue choices, and items carried by actors and containers. It does this using the engine's own script compiler.
- Step 2 adds `PositionCell` destinations, disabled objects and doors, and a cap on how many places one target can mark.
- Step 4 adds the edge pinning, the above and below arrows, and a colour for each quest. Step 5 adds the objective text line.
- The hand-labelled stages stay the deciding test. If no rule reaches Skyrim-like accuracy, the fallback is Quest Guider's honest approach: several markers, labelled as possible next steps.
