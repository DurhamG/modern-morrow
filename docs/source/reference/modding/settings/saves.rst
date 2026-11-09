Saves Settings
##############

.. omw-setting::
   :title: character
   :type: string
   :default: ""

   The default character shown in the Load Game menu.
   Automatically updated when a different character is selected.

.. omw-setting::
   :title: autosave
   :type: boolean
   :range: true, false
   :default: true
   :location: :bdg-info:`In Game > Options > Prefs`

   Determines whether the game auto-saves when the character rests.

.. omw-setting::
   :title: max quicksaves
   :type: int
   :range: >0
   :default: 1
   :location: :bdg-success:`Launcher > Settings > Miscellaneous`

   Number of quicksave and autosave slots available.
   If greater than 1, quicksaves are created sequentially.
   When the max is reached, the oldest quicksave is overwritten on the next quicksave.
   Unused while :ref:`quicksave history` is on.

.. omw-setting::
   :title: quicksave history
   :type: boolean
   :range: true, false
   :default: true

   Whether each quicksave and autosave makes a new save, keeping a history of saves to go back to.
   Older quicksaves thin out with the time played since them, so recent play keeps many saves and older play few.
   The newest 10 quicksaves are always kept.
   Older ones keep one save every 5 minutes of play for the last hour, every 30 minutes for the last 6 hours,
   every 2 hours for the last day, and every 4 hours before that.
   Quicksaves and autosaves thin out separately, and saves made from the save menu are never deleted.

.. omw-setting::
   :title: autosave interval
   :type: int
   :range: >=0
   :default: 5

   Autosaves after this many minutes of play without a save.
   The autosave waits for a moment when no menu is open and the player isn't in combat.
   Time spent in menus doesn't count, and any save or load starts the count again.
   0 turns it off.
