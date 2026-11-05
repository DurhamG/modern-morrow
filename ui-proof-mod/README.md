# UI proof-of-concept mod

This mod is not meant for upstream. It shows that the engine's Lua API is enough for a mod to replace
the game's menus, one plain screen per area, and that the screens work with a controller:

- **Magic**: replaces the Magic window. It lists the player's powers, spells and magic items with their
  cost and the chance to cast them. The d-pad moves the selection, A selects the spell or item, and X
  shows the effects. B leaves the detail view, and B on the list closes the menu.
- **Message boxes**: draws the messages with buttons, such as a game script's `MessageBox`. The d-pad
  picks a button and A answers.
- **HUD**: hides the engine's health, magicka and fatigue bars and draws three bars of its own.
- **Dialogue**: replaces the Dialogue window. It shows the conversation with the links to topics, the
  services, the topics, and the answers when the NPC asks a question. A asks a topic, answers a
  question or asks for a service. Persuasion opens a list of its own. B says goodbye.

The arrow keys, Enter, Escape and X do the same as the d-pad, A, B and X.

## Enabling it

Add these lines to `openmw.cfg`, after the lines for the Morrowind content files:

```
data="D:/Code/openmw-upstreaming/ui-proof-mod"
content=uiproof.omwscripts
```

## Opening the Magic menu

The Magic window belongs to the Interface mode, with the inventory, the map and the stats. To open it
alone, press K during the game, or run this in the console's Lua mode (`luap`):

```lua
I.UI.setMode('Interface', { windows = { 'Magic' } })
```
