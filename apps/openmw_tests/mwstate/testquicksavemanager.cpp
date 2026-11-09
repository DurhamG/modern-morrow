#include "apps/openmw/mwstate/quicksavemanager.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <list>
#include <string>

namespace MWState
{
    namespace
    {
        constexpr double sMinute = 60;
        constexpr double sHour = 60 * sMinute;

        // Quicksaves made one after another, each with when it was made and how long the game had been played.
        class QuickSaveHistory
        {
        public:
            const Slot& save(double timePlayed)
            {
                Slot& slot = mSlots.emplace_back();
                slot.mPath = std::to_string(mSlots.size());
                slot.mProfile.mTimePlayed = timePlayed;
                slot.mTimeStamp = std::filesystem::file_time_type{} + std::chrono::seconds(++mMade);
                return slot;
            }

            std::vector<const Slot*> all() const
            {
                std::vector<const Slot*> saves;
                for (const Slot& slot : mSlots)
                    saves.push_back(&slot);
                return saves;
            }

            /// Deletes what findQuickSavesToDelete says to, and returns how many it deleted.
            std::size_t thin()
            {
                const std::vector<const Slot*> toDelete = findQuickSavesToDelete(all());
                for (const Slot* slot : toDelete)
                    mSlots.remove_if([&](const Slot& s) { return &s == slot; });
                return toDelete.size();
            }

            /// The times played of the saves, newest first.
            std::vector<double> timesPlayed() const
            {
                std::vector<double> times;
                for (const Slot& slot : mSlots)
                    times.push_back(slot.mProfile.mTimePlayed);
                std::sort(times.rbegin(), times.rend());
                return times;
            }

            bool has(const Slot* slot) const
            {
                return std::any_of(mSlots.begin(), mSlots.end(), [&](const Slot& s) { return &s == slot; });
            }

        private:
            std::list<Slot> mSlots;
            int mMade = 0;
        };

        TEST(MWStateQuickSaveHistoryTest, keepsTheNewestSaves)
        {
            QuickSaveHistory history;
            for (std::size_t i = 0; i < sQuickSavesAlwaysKept; ++i)
                history.save(i * 10.0);
            EXPECT_EQ(history.thin(), 0);
            EXPECT_EQ(history.timesPlayed().size(), sQuickSavesAlwaysKept);
        }

        TEST(MWStateQuickSaveHistoryTest, keepsTheOldestSaveOfEachStretch)
        {
            QuickSaveHistory history;
            // A save every minute for 30 minutes of play.
            for (int i = 0; i <= 30; ++i)
                history.save(i * sMinute);
            history.thin();
            // The 10 newest saves are from minutes 21 to 30. The older ones keep one every 5 minutes.
            const std::vector<double> expected = { 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 15, 10, 5, 0 };
            std::vector<double> minutes;
            for (double time : history.timesPlayed())
                minutes.push_back(time / sMinute);
            EXPECT_EQ(minutes, expected);
        }

        TEST(MWStateQuickSaveHistoryTest, keepsTheNewestSavesAfterLoadingAnOlderOne)
        {
            QuickSaveHistory history;
            for (int i = 0; i <= 60; ++i)
                history.save(i * sMinute);
            history.thin();
            // The player loads the save from 40 minutes of play and plays on from there.
            std::vector<const Slot*> resumed;
            for (int i = 1; i <= 12; ++i)
                resumed.push_back(&history.save(40 * sMinute + i * 10));
            history.thin();
            for (std::size_t i = resumed.size() - sQuickSavesAlwaysKept; i < resumed.size(); ++i)
                EXPECT_TRUE(history.has(resumed[i])) << i;
        }

        // Saving every 30 seconds for 60 hours of play, and thinning after each save as the game does, leaves a
        // short history with no long gaps.
        TEST(MWStateQuickSaveHistoryTest, staysShortWithoutGapsOverALongGame)
        {
            QuickSaveHistory history;
            const double end = 60 * sHour;
            for (double played = 0; played <= end; played += 30)
            {
                history.save(played);
                history.thin();
            }

            const std::vector<double> times = history.timesPlayed();
            // At most 10 newest, 12 from the last hour, 10 from the next 5 hours, 9 from the rest of the day, and 9
            // from the 36 hours before that, plus one at each change of spacing.
            EXPECT_LE(times.size(), 10 + 12 + 10 + 9 + 9 + 4);
            EXPECT_EQ(times.back(), 0) << "the first save is kept";
            for (std::size_t i = sQuickSavesAlwaysKept; i < times.size(); ++i)
            {
                const double gap = times[i - 1] - times[i];
                EXPECT_LE(gap, 2 * getQuickSaveSpacing(end - times[i]))
                    << "between " << times[i] / sHour << " and " << times[i - 1] / sHour << " hours";
            }
        }
    }
}
