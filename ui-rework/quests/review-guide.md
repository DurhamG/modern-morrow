# Reviewing quest markers

This guide says how to judge the quest markers at each stage of a quest, and how to write down the result. It goes
with [design/quest-marker-corrections.md](../design/quest-marker-corrections.md), which explains the plan.

## What a review produces

For each quest:

- **Correction entries** in `files/data/questcorrections.yaml`, at the stages where the markers are wrong. The file's
  comments explain each key: `stages`, `next`, `optional`, `hide`, `targets` and `follow`.
- **A review record** in `ui-rework/quests/reviewed/<quest>.yaml`, with every stage that doesn't finish the quest, what
  it should mark, and a verdict. The `quest-review` scenario then fails if a later change makes any reviewed stage mark
  something else.

## The review sheet

Write the sheets with the harness, into the git-ignored `.automation` folder, because they hold the game's text:

```
python ui-rework/tools/omw.py launch
python ui-rework/tools/omw.py quest-review path=.automation/quest-review quests=A1_,A2_
python ui-rework/tools/omw.py quit
```

A sheet has, for each stage, the journal entry, what the markers show now ("Shows now"), the stage's correction if it
has one, and every way forward the analysis found. It also lists the stages that a line or script sets without a
journal entry, because the game sets the stage all the same, and the markers then work from it. Judge those too. A way leads to a higher stage, and lists its objectives: talk to,
kill or fetch an object, talk to anyone in a faction, or visit a cell. Each object's placement follows in brackets. A
way's checks are its line's conditions on global variables, other quests' stages and the speaker's local variables.
Only the running game can test them, so a way whose checks don't hold is left out while playing, and the rule then
chooses among the rest. After the stages come the lines and scripts that move the quest on, with their conditions
and results, and a review record of what the markers show now.

"Shows now" is worked out without the checks. The default rule marks the ways to the next stage above the current one,
and the ways to the furthest stage. Kills, fetches and visits come before the talk they lead to, so while playing the
talk is only marked once they're done.

## How to judge a stage

Imagine a player who has just read the stage's journal entry, and everything before it, and nothing after. The
markers are right when they point where that player expects to go next.

- **The next step the journal asks for is marked.** If the journal says to report to someone, that person is marked.
  If it names a place, the place or the person there is marked.
- **Side steps are optional.** Someone who gives directions, a place to ask around, or a hint the player can skip, is
  marked as optional: a hollow marker on the maps, left off the compass. Use `optional` for these.
- **Nothing is marked that the player can't know about yet.** A later outcome, such as a reward from someone the
  journal hasn't mentioned, is a spoiler. Use `next` to mark only the right ways, or `hide` to leave a way out.
- **Failures aren't marked.** Ways that need a quest giver or a companion dead are usually how the quest fails. The
  analysis sets them aside already, unless they are the only way forward.
- **Real choices are all marked.** When the journal offers alternatives, such as persuading someone or killing them,
  mark the ways for each.
- **Steps the content doesn't name get targets.** When the journal sends the player to people or places that no line
  or script names as a step, add `targets`, each with a `done` condition so the marker goes away once the step is
  done. A target can be `optional: true` for a side step, and can wait for an `after` condition, such as a report
  that only makes sense once someone is dead, or for a list of conditions that must all hold. Conditions can test a
  quest's stage, a death (`dead`), an item the player carries (`carried`), or a global variable (`global`).
- **A kill the journal asks for can be next.** When the way the journal asks for needs someone who talks in the quest
  dead, name its stage in `next`. It is marked when every `next` way is such a kill.
- **Helper quests are followed.** When a quest hands its steps to other quests, such as one per informant, add
  `follow` so their markers show while the main quest is tracked.

### Verdicts

- `right`: the markers were right without a correction.
- `corrected`: a correction entry makes them right.
- `unfixable`: no marker can be right, such as a step that depends on a random roll, or that sends the player to
  search a whole region. Leave such a stage as the analysis has it, or hide what misleads.

A stage that marks nothing can still be `right`, when there is nothing for the player to go to, such as a stage that
waits for time to pass.

## Writing entries

- One entry can cover several stages with a range, such as `stages: 1-10`, when the same choice is right for all.
- An entry names only quest IDs, journal indices, object IDs and cell names. It never holds text for players.
- The comment above each entry says where its facts come from: the journal index, the topic, the speaker, the
  conditions, the placement. Describe them in your own words. Don't copy the game's text.
- Each corrected quest needs a fingerprint under `fingerprints` at the top of the file. It lets the game leave out
  the quest's entries when a mod changes how the quest moves on.
- After editing, rebuild so the data file is copied, then run `python ui-rework/tools/omw.py scenario quest-review`.
  It reports entries that don't match the content, entries that change nothing, and corrected quests without a
  fingerprint, with the value to add.

## Writing the record

Write the sheets again once the quest's corrections are in, and put the verdicts in a text file, one line per stage:
the quest ID, the stage, the verdict, and a short reason in your own words. Then
`python ui-rework/tools/quest_records.py --sheets .automation/quest-review VERDICTS` copies each sheet's record into
`ui-rework/quests/reviewed/<quest>.yaml`, with the verdicts and reasons. It writes nothing if a stage has no verdict.
