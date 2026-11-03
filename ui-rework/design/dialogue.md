# Dialogue window

This is the design for the reworked dialogue window, being worked out with the user. The mock-ups in [../mockups/](../mockups/) show it. They are built by `ui-rework/mockups/dialogue.py`, from real lines by Arrille and Fargoth in Seyda Neen.

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

- **Exhausted topics.** The dialogue manager tracks, for each topic and NPC, whether the player has heard every response the NPC has. OpenMW has a hidden setting that colours topics by this, which is off by default.
- **Specific topics.** It also tracks whether the NPC has a response of their own on a topic, rather than one many NPCs share, such as "latest rumors".
- **Keywords.** The blue words in an answer are topics, so the screen can collect them from each answer.
- **Choices.** When an answer offers choices, the game blocks Goodbye until the player picks one.
- **Forced goodbyes.** Some answers end the conversation from the NPC's side. Then the only option left is to leave.

## Decisions

- **Not full screen.** Dialogue is immersive, so the conversation is a panel along the bottom of the screen, with the NPC in view above it.
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

## Open questions

- **Short or close NPCs.** The panel covers the lower part of the screen. With a short NPC standing close, such as Fargoth, the panel hides their face. The camera could frame the NPC's face when a conversation starts, or the panel could be shorter.
- **Persuasion** opens its own small window. It will get its own pass later.
