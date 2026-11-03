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
- No software is downloaded or installed from the internet. Web research is allowed.

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

- **HUD.** The minimap and weapon name text are gone. The rest is gathered in the bottom left corner, sized relative to the screen height, and fades out when it has nothing to show.
- **Map.** The bottom button bar is gone from all menus. The map fills the screen height, shows its own button hints, pans with the right stick, and shows 7x7 cells when distant terrain is on.

### Phase 3: make the changes clean enough to upstream

Once the UI looks and feels right, work out how OpenMW could support it cleanly. Not a priority yet.

## Target screens and resolutions

These are the screen sizes the harness should cover:

| Name | Resolution | Represents |
| --- | --- | --- |
| `deck` | 1280x800 | Steam Deck and similar handhelds |
| `tv1080` | 1920x1080 | 1080p TV |
| `tv4k` | 3840x2160 | 4K TV |

Published guidance gives some numbers to check designs against:

- **Steam Deck:** Valve requires the smallest text to be at least 9 px tall at 1280x800, and recommends 12 px.
- **Xbox:** Microsoft's guidance for console text is at least 26 px for body text at 1080p, or 52 px at 4K.
- **TV:** keep important content inside the "TV-safe" area, which is inset 48 px from the left and right and 27 px from the top and bottom at 1080p.
- **Focus:** the widget with controller focus must be clearly visible from across the room.
