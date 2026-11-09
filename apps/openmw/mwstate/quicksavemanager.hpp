#ifndef GAME_STATE_QUICKSAVEMANAGER_H
#define GAME_STATE_QUICKSAVEMANAGER_H

#include <string>
#include <vector>

#include "character.hpp"

namespace MWState
{
    /// The number of quicksaves that are always kept, counting back from the newest.
    constexpr std::size_t sQuickSavesAlwaysKept = 10;

    /// How far apart, in seconds of play, the quicksave history keeps saves that are the given number of seconds of
    /// play older than the newest quicksave. Each spacing divides the next, so a stretch of play that is merged into a
    /// longer one keeps the same oldest save.
    double getQuickSaveSpacing(double age);

    /// Finds the quicksaves to delete, so that many quicksaves thin out into a history. The saves are all of one
    /// kind, such as every "Quicksave" of a character.
    ///
    /// The newest saves, by when they were made, are all kept. Older saves are grouped into stretches of play time, as
    /// long as getQuickSaveSpacing gives for their age, and each stretch keeps its save with the least time played. So
    /// recent play keeps a save every few minutes, and older play one every few hours.
    std::vector<const Slot*> findQuickSavesToDelete(std::vector<const Slot*> saves);

    class QuickSaveManager
    {
        std::string mSaveName;
        unsigned int mMaxSaves;
        unsigned int mSlotsVisited;
        const Slot* mOldestSlotVisited;

    private:
        bool shouldCreateNewSlot() const;
        bool isOldestSave(const Slot* compare) const;

    public:
        QuickSaveManager(std::string& saveName, unsigned int maxSaves);
        ///< A utility class to manage multiple quicksave slots
        ///
        /// \param saveName The name of the save ("QuickSave", "AutoSave", etc)
        /// \param maxSaves The maximum number of save slots to create before recycling old ones

        void visitSave(const Slot* saveSlot);
        ///< Visits the given \a slot \a

        const Slot* getNextQuickSaveSlot();
        ///< Get the slot that the next quicksave should use.
        ///
        ///\return Either the oldest quicksave slot visited, or nullptr if a new slot can be made
    };
}

#endif
