# Dialogue window

This is the design for the reworked dialogue window, agreed with the user. The mock-ups in [../mockups/](../mockups/) show it. They are built by `ui-rework/mockups/dialogue.py`, from real lines by Arrille and Fargoth in Seyda Neen.

## The problems with the current window

- Everything is too small on a TV or a handheld. The window follows the GUI scaling factor, and most of it is empty space.
- The answer text runs the full width of a wide window, so each line is very long.
- The window is partly see-through, which makes the text harder to read.
- The stone border and title bar look dated.
- Services, such as Persuasion, Barter and Spells, share one narrow list with the topics, separated only by a thin line.
- The topics are in name order, so nothing points the player at what's worth asking.
- The blue keywords in answers are links, but a controller has no clear way to reach them.
- There are no button hints, and the disposition bar has no label.

## What the game knows about topics

- **Exhausted topics.** The dialogue manager tracks, for each topic and NPC, whether the player has heard the answer the NPC would give now. OpenMW has a hidden setting that colours topics by this, which is off by default. It used to check only the journal, but some answers remove themselves from the journal, such as most answers to "Background". It now also remembers the answers heard since the game was loaded.
- **Specific topics.** It also tracks whether the NPC has a response of their own on a topic, rather than one many NPCs share, such as "latest rumors".
- **Keywords.** The blue words in an answer are topics, so the screen can collect them from each answer.
- **Choices.** When an answer offers choices, the game blocks Goodbye until the player picks one.
- **Forced goodbyes.** Some answers end the conversation from the NPC's side. Then the only option left is to leave.

## Decisions

- **Not full screen.** Dialogue is immersive, so the conversation is a panel along the bottom of the screen, with the NPC in view above it. The panel stops short of the screen's edges, so the world shows around it, and the HUD hides during the conversation.
- **Sized relative to the screen height,** like the HUD and the other controller screens.
- **Opaque,** with a plain dark background and a thin gold line along the top, instead of the stone border.
- **The left side holds the conversation.** It shows the NPC's name and disposition, labelled, then the latest answer under its topic, in a larger font with shorter lines.
- **Earlier answers are reached with the right stick,** which scrolls back through the conversation. Only the latest answer shows at first.
- **The right side holds the options,** with Topics and Services tabs, switched with L1 and R1 like the other screens.
- **Topics are grouped.**
  - New topics come first. Topics just mentioned in an answer lead them, in blue, then the ones specific to this NPC, then the rest.
  - Asked topics, which have nothing new, follow, dimmed. They stay in the list, since players re-ask topics to read them again.
  - Y jumps between the groups.
- **Choices replace the options.** When an answer offers choices, only the choices show, and B does nothing until the player picks one.
- **Services have their own tab:** Persuasion, Barter, Spells, Travel, Training, Repair, Enchanting and Spellmaking, whichever the NPC offers. Each has a short line about what it does.
- **No Goodbye button.** B leaves the conversation.
- **Button hints** run along the bottom of the conversation.
- **The camera frames the NPC's face, in first person.** When a conversation starts, the camera turns to the NPC so their face sits in the middle of the space above the panel. It eases back to where the player was looking when the conversation ends. Services opened from the conversation, such as barter, keep the NPC framed. The turn is an extra pitch and yaw on top of the player's view, so the player's own view direction never changes. The third person camera keeps still, because looking down at the NPC from behind the player felt wrong.

## The first version

`apps/openmw/mwgui/dialoguepanel.cpp` draws the panel in controller menus mode, and `files/data/scripts/omw/camera/dialogue_framing.lua` turns the camera. The dialogue window still runs the conversation, and hides itself while the panel reads from it. Opening a service, such as barter, hides the panel until the player returns to the conversation. The `controller-dialogue` scenario talks to Arrille, and the `controller-dialogue-choice` scenario answers Fargoth's choice.

### Controls

| Button | What it does |
| --- | --- |
| D-pad, left stick | Move the selection. Holding repeats. |
| A | Ask about the topic, open the service, or give the reply. |
| B | Leave the conversation. It does nothing while a reply is needed. |
| Y | Jump to the next group of topics. |
| L1, R1 | Switch between the Topics and Services tabs. |
| Right stick | Scroll back through earlier answers. |

## Open questions

- **Persuasion** opens its own small window. It will get its own pass later.
