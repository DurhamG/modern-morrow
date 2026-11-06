# Travel window demo

This small Lua mod replaces the engine's travel window with a Lua window. It shows what a mod can build with the
travel API from the `travel` branch. It is not part of the merge request.

The window lists the destinations of the guide or caravaner you are talking to. Each row shows the destination's
name, the hours the trip takes, and its price. Destinations you can't afford are greyed out. Clicking a destination
makes the trip, and Escape closes the window.

## What it uses

- `I.UI.registerWindow('Travel', show, hide)` hides the engine's window. The show function gets the merchant.
- `types.NPC.record(merchant).travelDestinations` lists the destinations. The new `cellName` field names them.
- `types.Actor.getTravelPrice` and `types.Actor.getTravelHours` give the price and the time of each trip.
- `types.Player.buyTravel` makes the trip. Only global scripts can call it, so the player script sends an event to
  the global script in `scripts/traveldemo/global.lua`.

The window uses the built-in `I.MWUI` templates and the mouse. It is about 150 lines of Lua.

## Trying it

Build OpenMW from the `travel` branch. Then add these lines to your `openmw.cfg`, with the path to this folder:

```
data="path/to/travel-demo"
content=travel-demo.omwscripts
```

Talk to a guide or a caravaner who offers travel, such as the Silt Strider caravaner in Seyda Neen, and pick Travel.
