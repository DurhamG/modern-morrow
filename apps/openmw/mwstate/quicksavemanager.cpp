#include "quicksavemanager.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>

double MWState::getQuickSaveSpacing(double age)
{
    constexpr double minute = 60;
    constexpr double hour = 60 * minute;
    if (age < hour)
        return 5 * minute;
    if (age < 6 * hour)
        return 30 * minute;
    if (age < 24 * hour)
        return 2 * hour;
    return 4 * hour;
}

std::vector<const MWState::Slot*> MWState::findQuickSavesToDelete(std::vector<const Slot*> saves)
{
    std::vector<const Slot*> toDelete;
    if (saves.size() <= sQuickSavesAlwaysKept)
        return toDelete;

    // Newest first, by when each save was made. After loading an older save, the newest save can have less time
    // played than some before it.
    std::stable_sort(
        saves.begin(), saves.end(), [](const Slot* a, const Slot* b) { return a->mTimeStamp > b->mTimeStamp; });
    const double newest = saves.front()->mProfile.mTimePlayed;

    // Each stretch of play time is named by its length and by how many such lengths of play came before it.
    std::map<std::pair<double, double>, const Slot*> kept;
    for (std::size_t i = sQuickSavesAlwaysKept; i < saves.size(); ++i)
    {
        const Slot* save = saves[i];
        const double played = save->mProfile.mTimePlayed;
        const double spacing = getQuickSaveSpacing(std::max(0.0, newest - played));
        auto [it, inserted] = kept.try_emplace({ spacing, std::floor(played / spacing) }, save);
        if (inserted)
            continue;
        if (played < it->second->mProfile.mTimePlayed)
            std::swap(it->second, save);
        toDelete.push_back(save);
    }
    return toDelete;
}

MWState::QuickSaveManager::QuickSaveManager(std::string& saveName, unsigned int maxSaves)
    : mSaveName(saveName)
    , mMaxSaves(maxSaves)
    , mSlotsVisited(0)
    , mOldestSlotVisited(nullptr)
{
}

void MWState::QuickSaveManager::visitSave(const Slot* saveSlot)
{
    if (mSaveName == saveSlot->mProfile.mDescription)
    {
        ++mSlotsVisited;
        if (isOldestSave(saveSlot))
            mOldestSlotVisited = saveSlot;
    }
}

bool MWState::QuickSaveManager::isOldestSave(const Slot* compare) const
{
    if (mOldestSlotVisited == nullptr)
        return true;
    return (compare->mTimeStamp <= mOldestSlotVisited->mTimeStamp);
}

bool MWState::QuickSaveManager::shouldCreateNewSlot() const
{
    return (mSlotsVisited < mMaxSaves);
}

const MWState::Slot* MWState::QuickSaveManager::getNextQuickSaveSlot()
{
    if (shouldCreateNewSlot())
        return nullptr;
    return mOldestSlotVisited;
}
