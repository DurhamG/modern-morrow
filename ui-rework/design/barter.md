# Barter window

This is the design for the reworked barter window, in controller menus mode. The user asked for it in the style of the other screens, to review when they return. It hasn't been reviewed yet.

## The problems with the current window

- The player's inventory and the merchant's goods are two grids of icons side by side, with a character portrait, and every item needs a tooltip.
- The deal is a number between the grids, and haggling means typing a number or holding small arrow buttons.
- Nothing compares a merchant's item with what the player wears.

## Decisions

- **The same panel as looting.** Barter is a panel on the right side of the screen, over the world, opaque, with the HUD hidden. In first person, the camera keeps framing the merchant, moved into the space left of the panel.
- **Buy and Sell tabs,** switched with L1 and R1. Buy lists the merchant's goods. Sell lists the player's items that this merchant buys, so the player never offers something the merchant refuses.
- **Fat entries,** grouped by kind like the loot panel, most valuable first. Each entry shows the name, what the item is with its main stat, and the price of one. Y jumps to the next group.
- **Markers.** Items in the deal are tinted and marked Buying or Selling, with how many when it's part of a stack. Goods that beat what the player wears are marked Better, and the player's worn items are marked Equipped.
- **The deal.** A puts the whole stack into the deal, or takes it all back out. Left and right on the d-pad put one more in or take one out, for stacks such as arrows. The deal box shows how many items are bought and sold, what the player pays or gets, and the merchant's own price.
- **Haggling.** The right stick changes the offer while it's held: right asks for more gold, or offers to pay less. The offer shows in blue once it differs from the merchant's price. Pressing the right stick goes back to the merchant's price. The game's own rules decide whether the merchant accepts, as before.
- **The details panel** shows the highlighted item with its price, beside the equipped items it would replace, as on the loot panel.
- **Gold.** The header shows both sides' gold, and the deal box warns when one side can't pay.
- X makes the offer. On success, the window closes and the conversation goes on. B cancels the deal and goes back to the conversation.

## Controls

| Button | What it does |
| --- | --- |
| L1, R1 | Switch between Buy and Sell. |
| D-pad up and down, left stick | Move the selection. Holding repeats. |
| A | Put the item into the deal, or take it back out. |
| D-pad left and right | Put one more of a stack in, or take one out. |
| Y | Jump to the next group. |
| Right stick left and right | Haggle. |
| Right stick press | Go back to the merchant's price. |
| X | Make the offer. |
| B | Cancel and go back to the conversation. |

## The first version

`apps/openmw/mwgui/barterpanel.cpp` draws the panel. The trade window still keeps the deal, works out the prices and makes the trade, and hides itself while the panel reads from it. `files/data/scripts/omw/camera/dialogue_framing.lua` moves the merchant's face left during barter. The `controller-barter` scenario buys and sells with Arrille, haggles too far, then makes a deal at his price.

### Not done yet

- The user hasn't reviewed this design yet.
