# UI rework project notes

This folder holds the planning, design and progress documents for reworking OpenMW's user interface. The goal is a modern UI that works well with a controller, on a TV, and on handhelds like the Steam Deck.

These documents are for the people and agents working on the project. They are not part of OpenMW and will be removed before anything is sent upstream.

## Documents

- [plan.md](plan.md) describes the overall goals, the phases of the project, and the current phase.
- [harness.md](harness.md) describes the automation harness. The harness lets an agent launch the game, send it keyboard and gamepad input, set up game states, and take screenshots from inside the game.
- [progress.md](progress.md) is a dated log of what has been done, what was learned, and what is next.
- [research/gamepad-ui-references.md](research/gamepad-ui-references.md) lists games with well-regarded gamepad UIs, and sites for browsing their screens.

## How to keep these up to date

- Update `progress.md` at the end of each working session or after each significant commit.
- When a decision changes the plan, update `plan.md` and note the reason in `progress.md`.
- Keep design documents short. Describe the current design, not the history of how it got there. The history belongs in `progress.md` and in commit messages.
