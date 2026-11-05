#ifndef MWGUI_DIALOGE_H
#define MWGUI_DIALOGE_H

#include <list>
#include <memory>

#include "referenceinterface.hpp"
#include "windowbase.hpp"

#include "bookpage.hpp"

#include "../mwbase/dialoguemanager.hpp"
#include "../mwdialogue/keywordsearch.hpp"

#include <MyGUI_Delegate.h>

namespace Gui
{
    class AutoSizedTextBox;
    class MWList;
}

namespace MWGui
{
    class DialogueWindow;

    class PersuasionDialog : public WindowModal
    {
    public:
        PersuasionDialog(DialogueWindow& dialogueWindow);

        void onOpen() override;

        MyGUI::Widget* getDefaultKeyFocus() override;

    protected:
        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg) override;

    private:
        DialogueWindow& mDialogueWindow;

        int mInitialGoldLabelWidth;
        int mInitialMainWidgetWidth;

        MyGUI::Button* mCancelButton;
        MyGUI::Button* mAdmireButton;
        MyGUI::Button* mIntimidateButton;
        MyGUI::Button* mTauntButton;
        MyGUI::Button* mBribe10Button;
        MyGUI::Button* mBribe100Button;
        MyGUI::Button* mBribe1000Button;
        MyGUI::Widget* mActionsBox;
        Gui::AutoSizedTextBox* mGoldLabel;

        std::vector<MyGUI::Button*> mButtons;
        size_t mControllerFocus = 0;

        void adjustAction(MyGUI::Widget* action, int& totalHeight);

        void onCancel(MyGUI::Widget* sender);
        void onPersuade(MyGUI::Widget* sender);
    };

    struct Link
    {
        virtual ~Link() = default;
        virtual void activated() = 0;
    };

    struct Topic : Link
    {
        MyGUI::delegates::MultiDelegate<const std::string&> eventTopicActivated;
        Topic(const std::string& id)
            : mTopicId(id)
        {
        }
        std::string mTopicId;
        void activated() override;
    };

    struct Choice : Link
    {
        MyGUI::delegates::MultiDelegate<int> eventChoiceActivated;
        Choice(int id)
            : mChoiceId(id)
        {
        }
        int mChoiceId;
        void activated() override;
    };

    struct Goodbye : Link
    {
        MyGUI::delegates::MultiDelegate<> eventActivated;
        void activated() override;
    };

    class DialogueWindow : public WindowBase, public ReferenceInterface
    {
    public:
        DialogueWindow();

        void onTradeComplete();

        bool exit() override;

        void notifyLinkClicked(TypesetBook::InteractiveId link);

        void setPtr(const MWWorld::Ptr& actor) override;

        /// @return true if stale keywords were updated successfully
        bool setKeywords(const std::list<std::string>& keyWord);

        void onFrame(float dt) override;
        void clear() override { resetReference(); }

        void updateTopics();

        void setVisible(bool visible) override;

        std::string_view getWindowIdForLua() const override { return "Dialogue"; }

    protected:
        void updateTopicsPane();
        bool isCompanion(const MWWorld::Ptr& actor);
        bool isCompanion();

        void onSelectListItem(const std::string& topic, int id);
        void onByeClicked(MyGUI::Widget* sender);
        void onMouseWheel(MyGUI::Widget* sender, int rel);
        void onWindowResize(MyGUI::Window* sender);
        void onTopicActivated(const std::string& topicId);
        void onChoiceActivated(int id);
        void onGoodbyeActivated();

        void onScrollbarMoved(MyGUI::ScrollBar* sender, size_t pos);

        void updateHistory(bool scrollbar = false);

        void onReferenceUnavailable() override;

        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg) override;

    private:
        void updateDisposition();
        void restock();
        void deleteLater();
        void redrawTopicsList();

        bool mIsCompanion;
        std::list<std::string> mKeywords;

        size_t mTranscriptSize = 0;
        std::vector<std::pair<std::string, int>> mChoices;
        std::vector<BookTypesetter::Style*> mChoiceStyles;
        bool mGoodbye;

        std::vector<std::unique_ptr<Link>> mLinks;
        std::unordered_map<std::string, std::unique_ptr<Link>> mTopicLinks;

        std::vector<std::unique_ptr<Link>> mDeleteLater;

        MWDialogue::KeywordSearch mKeywordSearch;

        BookPage* mHistory;
        Gui::MWList* mTopicsList;
        MyGUI::ScrollBar* mScrollBar;
        MyGUI::ProgressBar* mDispositionBar;
        MyGUI::TextBox* mDispositionText;
        MyGUI::Button* mGoodbyeButton;

        PersuasionDialog mPersuasionDialog;

        MyGUI::IntSize mCurrentWindowSize;

        void setControllerFocus(size_t index, bool focused);
        size_t mControllerFocus = 0;
        int mControllerChoice = -1;

        void updateTopicFormat();
    };
}
#endif
