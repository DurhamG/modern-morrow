#ifndef MWGUI_DIALOGUEPANEL_H
#define MWGUI_DIALOGUEPANEL_H

#include <string>
#include <vector>

#include <SDL_events.h>

#include "controllerscreen.hpp"

namespace MyGUI
{
    class Widget;
}

namespace MWGui
{
    class DialogueWindow;

    /// The dialogue window's look in controller menus mode: an opaque panel along the bottom of the screen, with the
    /// NPC in view above it. The latest answer is on the left, and earlier ones are reached with the right stick.
    /// The options are on the right, in Topics and Services tabs. When an answer offers choices, the choices replace
    /// the options. The dialogue window still runs the conversation, and the panel reads it from there. See
    /// ui-rework/design/dialogue.md.
    class DialoguePanel
    {
    public:
        explicit DialoguePanel(DialogueWindow& window);
        ~DialoguePanel();

        void show();
        /// Hides the panel. When the conversation ended, it also forgets the tab and selection.
        void hide(bool conversationEnded);

        /// Draws the panel again, after the conversation changed. A new answer scrolls the conversation to it.
        void update();

        void onFrame(float duration);
        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg);

    private:
        enum class Tab
        {
            Topics,
            Services,
        };

        // A line in the options list: a group header, or an option.
        struct Row
        {
            std::string mHeader;
            std::string mLabel;
            std::string mDetail;
            bool mMentioned = false;
            bool mAsked = false;
            // The choice's ID, for choices
            int mChoice = -1;
        };

        DialogueWindow& mWindow;
        MyGUI::Widget* mRoot = nullptr;
        MyGUI::Widget* mConversation = nullptr;
        MyGUI::Widget* mOptions = nullptr;
        MyGUI::Widget* mHistory = nullptr;
        MyGUI::Widget* mHints = nullptr;
        MyGUI::Widget* mEarlierHint = nullptr;

        Tab mTab = Tab::Topics;
        ControllerList mList;
        // How many options the list had when it was last drawn, for moving through them with the d-pad.
        int mOptionCount = 0;
        std::string mFocusedLabel;
        // After a new answer, the focus stays on the top option, even as the topics update after it.
        bool mFocusOnTop = true;

        // How far the conversation is scrolled up from its start, and where the latest answer begins.
        float mHistoryScroll = 0.f;
        int mHistoryHeight = 0;
        int mLatestTop = 0;
        std::size_t mShownEntries = 0;
        int mShownDisposition = -1;

        std::vector<Row> buildRows() const;
        std::vector<std::string> mentionedTopics() const;
        bool inChoice() const;

        void drawConversation();
        void drawOptions();
        void drawHints(const std::vector<std::pair<SDL_GameControllerButton, std::string>>& hints);
        void placeHistory();

        // Draws the options again after the d-pad or the mouse moved the focus.
        void focusMoved();
        void nextGroup();
        void activate();
    };
}

#endif
