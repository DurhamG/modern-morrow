#include "cloudtab.hpp"

#include <format>

#include <components/misc/timeconvert.hpp>
#include <components/settings/values.hpp>
#include <components/version/version.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/statemanager.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwstate/character.hpp"
#include "../mwstate/cloudrequests.hpp"

#include "controllerscreen.hpp"
#include "textinput.hpp"

namespace MWGui
{
    namespace
    {
        std::string userAgent()
        {
            const std::string_view commit = Version::getCommitHash();
            return "ModernMorrow/" + std::string(commit.empty() ? std::string_view("dev") : commit.substr(0, 10));
        }

        const std::string& phrase()
        {
            return Settings::saves().mCloudPhrase.get();
        }

        std::string uploadedText(const MWState::CloudSaveDetails& details)
        {
            const std::optional<std::time_t> time = MWState::parseCloudTime(details.mUploaded);
            return time ? Misc::timeTToString(*time, "%Y.%m.%d %H:%M") : std::string();
        }
    }

    CloudTab::CloudTab() = default;

    CloudTab::~CloudTab()
    {
        if (mPhraseDialog != nullptr)
            MWBase::Environment::get().getWindowManager()->removeDialog(std::move(mPhraseDialog));
    }

    MWState::CloudRequests& CloudTab::requests()
    {
        if (mRequests == nullptr)
            mRequests = std::make_unique<MWState::CloudRequests>(std::make_unique<MWState::CloudSaves>(
                MWState::makeCloudTransport(userAgent()), Settings::saves().mCloudAddress.get()));
        return *mRequests;
    }

    void CloudTab::open(bool saving, const std::string& newSaveName)
    {
        mSaving = saving;
        mNewSaveName = newSaveName;
        mSaved = false;
        if (mStatus == Status::Idle)
            checkDetails();
    }

    bool CloudTab::busy() const
    {
        return mStatus != Status::Idle;
    }

    void CloudTab::checkDetails()
    {
        mFailure = MWState::CloudFailure::None;
        mDetails.reset();
        if (phrase().empty() || !requests().details(phrase()))
            return;
        mStatus = Status::Checking;
    }

    bool CloudTab::update()
    {
        if (mRequests == nullptr)
            return false;
        // An upload's progress changes while it runs, so the tab draws again each frame meanwhile.
        std::optional<MWState::CloudRequests::Finished> finished = mRequests->takeResult();
        if (!finished)
            return mStatus == Status::Uploading;

        mStatus = Status::Idle;
        mFailure = finished->mResult.mFailure;
        if (mFailure == MWState::CloudFailure::None)
        {
            mDetails = finished->mResult.mDetails;
            mSaved = finished->mKind == MWState::CloudRequests::Kind::Upload;
        }
        else if (mFailure == MWState::CloudFailure::NoSave)
            mDetails.reset();
        return true;
    }

    std::size_t CloudTab::entryCount() const
    {
        return mSaving ? 1 : 0;
    }

    std::pair<std::string, std::string> CloudTab::entry(std::size_t /*index*/) const
    {
        return { "#{Interface:CloudSaveEntry}", ControllerUi::escaped(mNewSaveName) };
    }

    std::string CloudTab::emptyMessage() const
    {
        return "#{Interface:SavesNone}";
    }

    void CloudTab::drawDetails(MyGUI::Widget* panel, int x, int y, int w, int bottom) const
    {
        using namespace ControllerUi;
        const auto paragraph = [&](const std::string& text, int fontSize, const MyGUI::Colour& colour) {
            for (const std::string& line : wrapText(panel, localised(text), w, fontSize, 6))
            {
                if (y + 26 > bottom)
                    return;
                addText(panel, x, y, w, 26, escaped(line), fontSize, colour);
                y += 26;
            }
            y += 8;
        };

        // The phrase, or a request to type one.
        if (phrase().empty())
            paragraph("#{Interface:CloudNoPhraseYet}", sFontNormal, sBrightColour);
        else
        {
            addText(panel, x, y, w, 26, "#{Interface:CloudPhrase}", sFontSmall, sDimColour);
            y += 22;
            addText(panel, x, y, w, 30, escaped(phrase()), sFontLarge, sBrightColour);
            y += 40;
        }

        // What is happening, or what went wrong.
        if (mStatus == Status::Checking)
            paragraph("#{Interface:CloudChecking}", sFontNormal, sTextColour);
        else if (mStatus == Status::Uploading)
        {
            const MWState::CloudRequests::Progress progress = mRequests->progress();
            const int percent = progress.mTotal == 0 ? 0 : static_cast<int>(progress.mDone * 100 / progress.mTotal);
            paragraph(
                localised("#{Interface:CloudUploading}") + std::format(" {}%", percent), sFontNormal, sBrightColour);
        }
        else if (mSaved)
            paragraph("#{Interface:CloudSaved}", sFontNormal, sBrightColour);
        else if (mFailure != MWState::CloudFailure::None && mFailure != MWState::CloudFailure::NoSave)
            paragraph(
                std::format("#{{Interface:{}}}", MWState::cloudFailureMessage(mFailure)), sFontNormal, sBrightColour);

        // The save the phrase holds.
        if (mDetails)
        {
            addText(panel, x, y, w, 26, "#{Interface:CloudHolds}", sFontSmall, sDimColour);
            y += 22;
            addText(panel, x, y, w, 30, escaped(mDetails->mPlayerName), sFontLarge, sBrightColour);
            y += 34;
            std::vector<std::string> lines;
            if (mDetails->mPlayerLevel)
                lines.push_back(std::format("#{{OMWEngine:Level}} {}", *mDetails->mPlayerLevel));
            if (!mDetails->mCell.empty())
                lines.push_back(std::format("#{{sCell={}}}", mDetails->mCell));
            if (!mDetails->mDescription.empty())
                lines.push_back(escaped(mDetails->mDescription));
            if (const std::string uploaded = uploadedText(*mDetails); !uploaded.empty())
                lines.push_back(uploaded);
            for (const std::string& line : lines)
            {
                if (y + 26 > bottom)
                    break;
                addText(panel, x, y, w, 26, line, sFontNormal, sTextColour);
                y += 26;
            }
            y += 8;
        }
        else if (mFailure == MWState::CloudFailure::NoSave)
            paragraph("#{Interface:CloudHoldsNothing}", sFontNormal, sTextColour);

        if (mSaving && !busy())
            paragraph("#{Interface:CloudSaveText}", sFontNormal, sTextColour);
    }

    std::vector<std::pair<SDL_GameControllerButton, std::string>> CloudTab::hints() const
    {
        std::vector<std::pair<SDL_GameControllerButton, std::string>> result;
        if (busy())
            return result;
        if (entryCount() > 0)
            result.emplace_back(SDL_CONTROLLER_BUTTON_A, "#{Interface:SavesSave}");
        result.emplace_back(SDL_CONTROLLER_BUTTON_Y,
            phrase().empty() ? "#{Interface:CloudTypePhrase}" : "#{Interface:CloudChangePhrase}");
        return result;
    }

    void CloudTab::activate()
    {
        if (busy() || !mSaving)
            return;
        if (phrase().empty())
            askPhrase(AfterPhrase::Save);
        else
            saveToCloud();
    }

    void CloudTab::changePhrase()
    {
        if (!busy())
            askPhrase(AfterPhrase::Check);
    }

    void CloudTab::askPhrase(AfterPhrase after)
    {
        mAfterPhrase = after;
        mPhraseDialog = std::make_unique<TextInputDialog>();
        mPhraseDialog->setTextLabel(ControllerUi::localised("#{Interface:CloudPhrase}"));
        mPhraseDialog->setTextInput(phrase());
        mPhraseDialog->setNextButtonShow(false);
        mPhraseDialog->eventDone += MyGUI::newDelegate(this, &CloudTab::onPhraseDone);
        mPhraseDialog->setVisible(true);
    }

    void CloudTab::onPhraseDone(WindowBase* /*sender*/)
    {
        const std::string typed = mPhraseDialog->getTextInput();
        MWBase::Environment::get().getWindowManager()->removeDialog(std::move(mPhraseDialog));
        if (!typed.empty() && typed != phrase())
        {
            Settings::saves().mCloudPhrase.set(typed);
            mSaved = false;
        }
        if (!phrase().empty())
        {
            if (mAfterPhrase == AfterPhrase::Save)
                saveToCloud();
            else
                checkDetails();
        }
        if (mRedraw)
            mRedraw();
    }

    void CloudTab::saveToCloud()
    {
        // A cloud save is also an ordinary save on this device, in this game's own folder. Then that same file is
        // uploaded.
        MWBase::StateManager* states = MWBase::Environment::get().getStateManager();
        states->saveGame(mNewSaveName, nullptr, MWBase::StateManager::SaveFolder::Own);
        const MWState::Character* character = states->getCurrentCharacter();
        mSaved = false;
        mFailure = MWState::CloudFailure::None;
        if (character == nullptr || character->begin() == character->end())
        {
            mFailure = MWState::CloudFailure::ServiceError;
            return;
        }
        // The newest save comes first.
        if (requests().upload(character->begin()->mPath, phrase()))
            mStatus = Status::Uploading;
    }
}
