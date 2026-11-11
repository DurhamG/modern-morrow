#ifndef MWGUI_JOURNALSCREEN_H
#define MWGUI_JOURNALSCREEN_H

#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include <MyGUI_Types.h>

#include "../mwdialogue/keywordsearch.hpp"

#include "controllerscreen.hpp"
#include "questlog.hpp"

namespace MWDialogue
{
    class Topic;
}

namespace MWGui
{
    /// The journal in controller menus mode, as a tab beside the map, inventory, magic and stats. It lists the
    /// player's quests, the ones in progress first, and shows the highlighted quest's entries beside the list, newest
    /// first. The Recent, Places and Factions tabs sort the same quests by their last update, by the town they
    /// started in, and by the faction they are for. The Notes tab lists the journal's notes, such as a faction's rules,
    /// which aren't quests. X pins the highlighted quest or note, which puts it in a Pinned group at the top of every
    /// tab. The topics the player has asked about are highlighted in the entries. A moves the focus
    /// into the entries, where the d-pad steps from topic to topic, and A opens a topic's page: what people told the
    /// player about it. B steps back. See ui-rework/design/journal.md.
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
            Notes,
        };
        static constexpr int sTabCount = 4;

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

        // The highlighted quest's entries, or a topic's page, which the right stick scrolls when they don't fit.
        MyGUI::Widget* mEntriesPaper = nullptr;
        MyGUI::ImageBox* mEntriesScrollThumb = nullptr;
        MyGUI::IntCoord mEntriesTrack;
        std::string mEntriesPage;
        float mEntriesScroll = 0.f;
        int mEntriesScrollMax = 0;
        int mEntriesViewHeight = 0;

        // The topics the player has asked about, to find them in the text of quests and topics.
        MWDialogue::KeywordSearch mKeywordSearch;
        std::unordered_map<std::string, const MWDialogue::Topic*> mTopics;

        // Whether the focus is in the details panel, rather than in the list.
        bool mInDetails = false;
        // The topic pages opened from the quest, the last of them showing, each with the link that was highlighted on
        // the page before it. The quest's entries show while there are none.
        struct TopicPage
        {
            const MWDialogue::Topic* mTopic;
            int mLinkBefore;
        };
        std::vector<TopicPage> mTopicPages;
        // The topics highlighted on the page that shows, in reading order, with where each one is on the paper. A
        // topic that wraps onto a second line has a part on each.
        struct Link
        {
            const MWDialogue::Topic* mTopic;
            std::vector<MyGUI::IntCoord> mParts;
        };
        std::vector<Link> mLinks;
        int mLinkFocus = 0;
        HeldDirection mLinkHeld;

        // Text with the topics in it marked, as start and end offsets in the text.
        struct LinkedText
        {
            std::string mText;
            std::vector<std::tuple<std::size_t, std::size_t, const MWDialogue::Topic*>> mSpans;
        };

        void redraw() override;

        /// The quests or notes the current tab lists, in its order, as indices into mQuests.
        std::vector<int> order() const;
        std::string groupOf(const QuestLog::Quest& quest) const;
        std::vector<Row> buildRows(const std::vector<int>& order) const;
        static std::vector<int> groupStarts(const std::vector<Row>& rows);
        int focusIndex(const std::vector<int>& order) const;

        void drawList(int x, int y, int w, int h, const std::vector<int>& order, const std::vector<Row>& rows);
        void drawEntry(int x, int y, int w, int h, const QuestLog::Quest& quest, bool focused);
        void drawDetails(int x, int y, int w, int h, const QuestLog::Quest& quest, bool groups);
        /// Finds the topics the player has asked about in text, leaving out the topic whose page the text is on.
        LinkedText linkTopics(const std::string& text, const MWDialogue::Topic* page) const;
        /// Draws text wrapped to the width on the paper from y, with its topics highlighted, and adds them to mLinks.
        /// Returns the height it takes.
        int drawLinkedText(int y, int width, const LinkedText& text, const MyGUI::Colour& colour);
        void placeEntries();
        void scrollEntries(float stick, float duration);

        // Selects the quest the list focuses, and draws the screen again.
        void selectFocused();
        // Pins the highlighted quest, or unpins it.
        void togglePinned();
        // Moves the focus into the details, onto the first topic, or opens the highlighted topic.
        void activateDetails();
        // Steps back from a topic's page, or moves the focus back to the list. Returns false when the list has it.
        bool stepBack();
        void moveLink(int delta);
        // Opens a topic's page from a link on the page that shows.
        void openLink(int link);
        // Moves the focus back to the list, and shows the quest's entries.
        void leaveDetails();
        void changeTab(int delta);
        void nextGroup();
    };
}

#endif
