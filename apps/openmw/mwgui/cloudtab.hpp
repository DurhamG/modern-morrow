#ifndef OPENMW_MWGUI_CLOUDTAB_H
#define OPENMW_MWGUI_CLOUDTAB_H

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <SDL_gamecontroller.h>

#include "../mwstate/cloudsaves.hpp"

namespace MyGUI
{
    class Widget;
}

namespace MWState
{
    class CloudRequests;
}

namespace MWGui
{
    class TextInputDialog;
    class WindowBase;

    /// The Cloud tab of the save and load screens. Saving makes a new local save, then uploads it as the phrase's
    /// cloud save. Loading shows the save the phrase holds, and downloads and loads it. See ui-rework/cloud-saves.md.
    class CloudTab
    {
    public:
        CloudTab();
        ~CloudTab();

        /// Called when the tab is shown. newSaveName names the local save that saving to the cloud makes.
        void open(bool saving, const std::string& newSaveName);

        /// Whether a request is running. Meanwhile the screen can't be left, and its tabs can't be switched.
        bool busy() const;

        /// Looks for a finished request. Returns true when the tab must be drawn again.
        bool update();

        /// How many entries the tab lists: saving's one entry, or the save the phrase holds.
        std::size_t entryCount() const;

        /// The title and subtitle of an entry.
        std::pair<std::string, std::string> entry(std::size_t index) const;

        /// What the list shows when it has no entries.
        std::string emptyMessage() const;

        /// Draws the details on the right, from top to bottom.
        void drawDetails(MyGUI::Widget* panel, int x, int y, int w, int bottom) const;

        /// The button hints the tab shows.
        std::vector<std::pair<SDL_GameControllerButton, std::string>> hints() const;

        /// A saves to the cloud. Y changes the phrase.
        void activate();
        void changePhrase();

        /// Called when the tab must be drawn again outside update, such as after the phrase dialog.
        std::function<void()> mRedraw;

    private:
        enum class Status
        {
            Idle,
            Checking,
            Uploading,
        };

        enum class AfterPhrase
        {
            Check,
            Save,
        };

        MWState::CloudRequests& requests();
        void askPhrase(AfterPhrase after);
        void onPhraseDone(WindowBase* sender);
        void checkDetails();
        void saveToCloud();

        bool mSaving = false;
        std::string mNewSaveName;
        Status mStatus = Status::Idle;
        MWState::CloudFailure mFailure = MWState::CloudFailure::None;
        bool mSaved = false;
        std::optional<MWState::CloudSaveDetails> mDetails;
        AfterPhrase mAfterPhrase = AfterPhrase::Check;
        std::unique_ptr<TextInputDialog> mPhraseDialog;
        std::unique_ptr<MWState::CloudRequests> mRequests;
    };
}

#endif
