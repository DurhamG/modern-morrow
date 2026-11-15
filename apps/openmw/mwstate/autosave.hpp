#ifndef GAME_STATE_AUTOSAVE_H
#define GAME_STATE_AUTOSAVE_H

namespace MWState
{
    /// Whether this frame's time counts towards the timed autosave. Only play counts: not menus, not while no game is
    /// running, and not when the "autosave interval" setting turns the autosave off.
    inline bool autosaveClockRuns(int intervalMinutes, bool running, bool inMenu)
    {
        return running && intervalMinutes > 0 && !inMenu;
    }

    /// Whether enough play has passed since the last save for the timed autosave.
    inline bool autosaveDue(double playedSinceSave, int intervalMinutes)
    {
        return playedSinceSave >= intervalMinutes * 60.0;
    }

    /// Whether the timed autosave may save now. These are the same checks as quicksave, without its message when
    /// saving isn't allowed, and with none during combat.
    inline bool canAutosave(bool charGenDone, bool savingAllowed, bool playerDead, bool inCombat)
    {
        return charGenDone && savingAllowed && !playerDead && !inCombat;
    }
}

#endif
