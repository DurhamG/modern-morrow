# Quest marker corrections: plan

This plan follows [quest-markers.md](quest-markers.md). That design works out every quest's next steps from the
content files, and marks the targets on the maps and the compass. Its audits found the markers right at about 88% of
stages. That counts a stage as right when any of its markers is right, so it hides a second problem: at roughly 500
stages, a right marker comes with a misleading one. Examples are a spoiler for a later step, a way to fail the quest,
or a side step such as Elone in Seyda Neen, who only gives directions to Caius Cosades.

The game data can't tell these apart. Elone and Sharn gra-Muzgob look the same to the analysis, but one is optional
and the other isn't. So this plan adds human judgement, in two parts:

- **Corrections.** A small data file says, at the stages that need it, which of the analysis's next steps to mark,
  which are optional, which to hide, and any targets the content never names.
- **A review of every stage** of Morrowind, Tribunal and Bloodmoon, about 2,000 stages in about 500 quests. Each stage
  is judged by what a player reading the journal would expect. The review's result is kept as a record of the
  expected markers, and a harness check compares the game against it, so later changes can't make a reviewed stage
  worse without someone noticing.

The analysis stays in charge of where targets are now, the routes through doors, and the checks made while playing,
such as dead actors and global variables. Quests from mods keep the analysis alone.

## Decisions this plan makes

- **Our own data, not Comprehensive Objective Mod's.** That mod's data has no licence, was written from UESP's
  walkthroughs, which are CC BY-SA, is English only, and was wrong at about 10% of the stages we judged. We keep
  using it only to measure, as before, and never ship it or copy from it.
- **No text in the data.** Entries hold quest IDs, journal indices and object IDs. Facts come from the game's own
  files: journal entries, dialogue lines with their speakers, conditions and results, scripts, and placements. Nothing
  is written from UESP.
- **The data is optional to the engine.** An entry whose quest, index or object doesn't exist in the loaded content is
  dropped with a warning, and that stage falls back to the analysis. A mod that renumbers a quest gets today's
  markers, not wrong ones.
- **Separate from core gameplay.** The new code lives in new files and is called from one place, where the markers
  choose which next steps to mark. With Quest Markers off, the corrections are never read and none of this runs.
- **On the quest-markers branch.** Nothing reaches master until the whole feature is done and you've played it.

## The data file

`files/data/questcorrections.yaml`, read through the virtual file system, so a mod could replace it. The format
follows the feasibility sample from the evaluation:

```yaml
version: 1
quests:
  A1_1_FindSpymaster:
    - stages: 1-2        # the journal indices this entry covers: a number, a range or a list
      next: [14]         # mark only the ways that lead to these indices
      optional: [5]      # show these ways as optional leads
  A1_V_VivecInformants:
    - stages: 1
      targets:           # objectives no line or script in the content names
        - { task: talk, object: addhiranirr, done: { quest: A1_6_AddhiranirrInformant, finished: true } }
        - { task: talk, object: huleeya, done: { quest: A1_7HuleeyaInformant, atLeast: 50 } }
        - { task: talk, object: mehra milo, done: { quest: A1_10_MehraMilo, atLeast: 50 } }
      follow: [A1_6_AddhiranirrInformant, A1_7HuleeyaInformant, A1_10_MehraMilo]   # helper quests to mark too
```

These two entries come from the evaluation's sample, checked against the three games' files.

- `hide` lists indices never to mark. The default rule then picks among the rest.
- If no `next` way survives the checks made while playing, such as a dead speaker, the default rule applies.
- Comments in the file name where each fact came from, so a reviewer can check them.

## The review record

`ui-rework/quests/reviewed/<quest>.yaml`, one file per quest, holding no game text. For each stage, it records the
object IDs or cells that should be marked, which of them are optional, and whether the stage was judged right
without a correction. Two harness commands use it:

- `quest-review` writes, for each quest, a review sheet. The sheet has the journal entries, each way forward with its
  speaker, conditions and result, and what the markers show now. The sheets hold Morrowind's text, so they go in the
  git-ignored `.automation/` folder.
- `quest-review-check` compares what the markers would show at every reviewed stage with the record. It reports
  changed stages, corrections entries that were dropped, and entries that no longer change anything. It runs in
  `prerelease.py`, so a release can't ship a regression in a reviewed quest.

This check works on what's in the content files, without a running game's state. Checks that need the running game,
such as which faction member is nearest, are covered by scenarios.

## How it's tested without playing

- **Google Tests** for every rule: parsing the file, dropping bad entries, filtering ways, optional steps, explicit
  targets with their `done` condition, helper quests, and the new topic rule. Each is a plain function tested on
  small made-up quests.
- **A harness command that lists the markers shown now**, with their quest, object, cell and whether they're
  optional, and an `expect-marker` scenario check that uses it. Scenarios set a quest's stage with the console and
  check what's marked, such as "at A1_1 stage 1, Caius is marked and Elone isn't".
- **`quest-review-check`** over every reviewed stage of the three games, with each review batch.
- **Screenshots** of the maps and the compass at the milestones that change what they draw, sent to you.
- **The comparison with Comprehensive Objective Mod**, rerun after each batch, as an internal measurement only.

You play it once the feature is done.

## Milestones

Each step is a commit, unless it says otherwise. Every commit builds and keeps the tests passing. The estimates are
rough, for AI-assisted work.

### 1. Corrections in the engine, without visible changes (2–3 days)

1. Add the harness command that lists the markers shown now, and the `expect-marker` scenario check, with a scenario
   that checks today's markers at A1_1 stage 1, Elone included. Nothing changes in the game yet.
2. Add the corrections data types and the YAML reader, as plain functions in new files, with Google Tests. Nothing
   reads a real file yet.
3. Check entries against the loaded content, dropping bad ones with a warning, with Google Tests on made-up quests.
4. Add the function that applies an entry's `next` and `hide` to the analysis's ways, falling back to the default
   rule, with Google Tests.
5. Read `questcorrections.yaml` when the markers first build the analysis, and apply it. The first entries are for
   A1_1. The scenario from step 1 now expects Caius without Elone. `quest-targets` shows corrected stages too.

### 2. Optional steps (1–2 days)

1. `optional` in the data and in the filtering function, with Google Tests.
2. A hollow, smaller marker image for optional steps, drawn on the local map, the 3D map and the world map. The
   compass leaves optional steps out. Screenshots of each.
3. The A1_1 entries mark the South Wall Cornerclub and Bacola Closcius as optional. Scenario checks for both kinds.

### 3. Targets the content doesn't name, and helper quests (1–2 days)

1. `targets` with their `done` condition, going through the same placement, item holder and door route code as the
   analysis's targets. Google Tests, then a scenario with the three informants in "Vivec Informants".
2. `follow`, marking a helper quest's targets while it's active. Google Tests and a scenario.

### 4. The topic rule (1 day)

1. In the analysis: a line in a topic that the player can only learn at a later stage of the same quest isn't a next
   step yet. This needs every line that names or adds the topic to need a later stage. Google Tests.
2. A scenario at Sharn gra-Muzgob's stages, where Caius was marked throughout. The comparison numbers before and
   after, in the progress log.

### 5. Review tooling (1–2 days)

1. The review record's format and reader, with Google Tests.
2. `quest-review`, writing the review sheets.
3. `quest-review-check`, and running it from `prerelease.py`.

### 6. The review (6–8 days, in batches)

Each batch reviews every stage of its quests, then commits the corrections entries and the review records for those
quests. A few scenario spot checks are added where the corrections do something new. The progress log gets the
coverage so far and the comparison numbers.

1. The main quest.
2. The Fighters Guild, Mages Guild, Thieves Guild and Morag Tong.
3. The Imperial Cult, Imperial Legion and Tribunal Temple.
4. The three Great Houses, including their strongholds.
5. Vampire clans and side quests in Vvardenfell, split into several commits by region.
6. Tribunal.
7. Bloodmoon.

For each quest, an AI agent drafts the judgement from the review sheet and the game data, and proposes entries. A
second agent checks a sample of stages, and every stage the first one changed. You can read any batch's diff. Batches
1 and 2 cover most of what new players see, so the feature could ship after them with the rest still on the
analysis.

### 7. Mods that change official quests (1 day)

1. A fingerprint of each quest's structure, made from its journal indices, speakers and conditions, without any text,
   stored with the review record. When a loaded mod changes a corrected quest's structure, that quest's entries are
   dropped with a warning and it falls back to the analysis. Google Tests with made-up quests. No real mods can be
   tested here, since nothing may be downloaded.

### Total

About 13–18 days. After milestones 1 to 5 and batches 1 and 2 of the review, the feature would already fix what you
saw at the start of the game.

## Risks and limits

- **The review's quality.** AI drafting with sampled checking will still get some stages wrong. The record and
  `quest-review-check` stop those from getting worse, and later fixes are small diffs.
- **The static check can't see the running game.** Stages that depend on globals, a nearest faction member or the
  player's earlier choices are only checked by the scenarios that cover them.
- **Mods.** Tamriel Rebuilt and other quest mods get the analysis only. A mod that rewrites an official quest but
  keeps its structure passes the fingerprint and could make an entry wrong.
- **The estimates.** The number of stages that need entries is estimated from samples of 25 and 70 stages, so the
  review could take 30% more or less than planned.

## Answers from review of this plan

- Optional steps are hollow, smaller markers, left off the compass.
- No objective text for now. A line such as "Talk to Caius Cosades" could come later, from templates in the
  localisation files.
- The feature can ship after review batches 1 and 2, with the rest of the review following.
