#ifndef MWGUI_JOURNALSCREEN_H
#define MWGUI_JOURNALSCREEN_H

#include <string>
#include <vector>

#include <MyGUI_Types.h>

#include "controllerscreen.hpp"
#include "questlog.hpp"

namespace MWGui
{
    /// The journal in controller menus mode, as a tab beside the map, inventory, magic and stats. It lists the
    /// player's quests, the ones in progress first, and shows the highlighted quest's entries beside the list, newest
    /// first. The Recent, Places and Factions tabs sort the same quests by their last update, by the town they
    /// started in, and by the faction they are for. See ui-rework/design/journal.md.
    class JournalScreen : public ControllerScreen
    {
    public:
        JournalScreen();

        void onOpen() override;
        void onFrame(float duration) override;
        void clear() override;

    protected:
        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg) override;
        bool onControllerThumbstickEvent(const SDL_ControllerAxisEvent& arg) override;

    private:
        enum class Tab
        {
            Recent,
            Places,
            Factions,
        };
        static constexpr int sTabCount = 3;

        // A line in the list: a group header, or an index into the tab's order of quests.
        struct Row
        {
            std::string mHeader;
            int mEntry = -1;
        };

        Tab mTab = Tab::Recent;
        std::vector<QuestLog::Quest> mQuests;
        // The highlighted quest, by name, so it stays highlighted when the tab changes
        std::string mSelected;
        // The list's focus is set from mSelected each time the screen draws.
        ControllerList mList;

        // The highlighted quest's entries, which the right stick scrolls when they don't fit.
        MyGUI::Widget* mEntriesPaper = nullptr;
        MyGUI::ImageBox* mEntriesScrollThumb = nullptr;
        MyGUI::IntCoord mEntriesTrack;
        std::string mEntriesQuest;
        float mEntriesScroll = 0.f;
        int mEntriesScrollMax = 0;

        void redraw() override;

        /// The quests in the order the current tab lists them, as indices into mQuests.
        std::vector<int> order() const;
        std::string groupOf(const QuestLog::Quest& quest) const;
        std::vector<Row> buildRows(const std::vector<int>& order) const;
        static std::vector<int> groupStarts(const std::vector<Row>& rows);
        int focusIndex(const std::vector<int>& order) const;

        void drawList(int x, int y, int w, int h, const std::vector<int>& order, const std::vector<Row>& rows);
        void drawEntry(int x, int y, int w, int h, const QuestLog::Quest& quest, bool focused);
        void drawDetails(int x, int y, int w, int h, const QuestLog::Quest& quest, bool groups);
        void placeEntries();
        void scrollEntries(float stick, float duration);

        // Selects the quest the list focuses, and draws the screen again.
        void selectFocused();
        void changeTab(int delta);
        void nextGroup();
    };
}

#endif
