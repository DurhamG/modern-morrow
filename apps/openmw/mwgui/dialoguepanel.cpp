#include "dialoguepanel.hpp"

#include <algorithm>
#include <cmath>
#include <array>

#include <MyGUI_Gui.h>
#include <MyGUI_ILayer.h>
#include <MyGUI_LayerManager.h>

#include <components/esm3/loadgmst.hpp>
#include <components/misc/strings/lower.hpp>
#include <components/translation/translation.hpp>
#include <components/widgets/box.hpp>

#include "../mwbase/dialoguemanager.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"

#include "dialogue.hpp"

namespace MWGui
{
    namespace
    {
        using namespace ControllerUi;

        // The panel's place on the canvas, which is 720 units tall, like the other controller screens.
        constexpr int sPanelTop = 404;
        constexpr int sPanelBottom = 696;
        constexpr int sPanelMargin = 48;
        constexpr int sPanelPadding = 20;
        constexpr int sTextSize = 20;
        constexpr int sRowHeight = 28;
        constexpr int sServiceRowHeight = 52;
        constexpr int sHistoryTop = sPanelTop + 56;
        constexpr int sHintsHeight = 44;

        // How far the conversation scrolls per second with the right stick pushed all the way, in canvas units.
        constexpr float sHistoryScrollSpeed = 600.f;

        const MyGUI::Colour sPanelBackColour = MyGUI::Colour::parse("#0d0b08");
        const MyGUI::Colour sDispositionColour = MyGUI::Colour::parse("#3a5aa8");

        const std::string sKeywordColourTag = "#7fb3ff";
        const std::string sTextColourTag = "#caa560";

        struct Columns
        {
            int mTextX;
            int mTextW;
            int mOptionsX;
            int mOptionsW;
        };

        Columns columns(int width)
        {
            const int split = static_cast<int>(width * 0.62f);
            const int textX = sPanelMargin + sPanelPadding;
            return { textX, split - 24 - textX, split, width - sPanelMargin - sPanelPadding - split };
        }

        const std::string& gameSetting(std::string_view id)
        {
            return MWBase::Environment::get().getESMStore()->get<ESM::GameSetting>().find(id)->mValue.getString();
        }

        // A short line about what each service does, by the service's name.
        std::string serviceDetail(const std::string& service)
        {
            static const std::array<std::pair<std::string_view, std::string_view>, 9> details = { {
                { "sPersuasion", "Persuasion" },
                { "sBarter", "Barter" },
                { "sSpells", "Spells" },
                { "sTravel", "Travel" },
                { "sSpellmakingMenuTitle", "Spellmaking" },
                { "sEnchanting", "Enchanting" },
                { "sServiceTrainingTitle", "Training" },
                { "sRepair", "Repair" },
                { "sCompanionShare", "CompanionShare" },
            } };
            for (const auto& [setting, key] : details)
                if (gameSetting(setting) == service)
                    return "#{Interface:DialogueService" + std::string(key) + "}";
            return {};
        }

        // A text box that wraps its text to its width, and grows to fit it.
        Gui::EditBox* addWrappedText(MyGUI::Widget* parent, int x, int y, int w, const std::string& caption,
            int fontHeight, const MyGUI::Colour& colour)
        {
            auto* box = parent->createWidget<Gui::EditBox>(
                "NormalText", MyGUI::IntCoord(x, y, w, fontHeight), MyGUI::Align::Default);
            box->setEditStatic(true);
            box->setEditMultiLine(true);
            box->setEditWordWrap(true);
            box->setNeedMouseFocus(false);
            box->setNeedKeyFocus(false);
            box->setMaxTextLength(caption.size() + 1);
            box->setFontHeight(fontHeight);
            box->setTextColour(colour);
            box->setTextShadow(true);
            box->setTextShadowColour(MyGUI::Colour::Black);
            box->setCaption(MyGUI::UString(caption));
            box->setSize(w, box->getTextSize().height);
            return box;
        }

        void clearChildren(MyGUI::Widget* widget)
        {
            while (widget->getChildCount() > 0)
                MyGUI::Gui::getInstance().destroyWidget(widget->getChildAt(0));
        }
    }

    DialoguePanel::DialoguePanel(DialogueWindow& window)
        : mWindow(window)
    {
    }

    DialoguePanel::~DialoguePanel()
    {
        if (mRoot != nullptr)
            MyGUI::Gui::getInstance().destroyWidget(mRoot);
    }

    void DialoguePanel::show()
    {
        if (mRoot == nullptr)
        {
            MyGUI::ILayer* layer = MyGUI::LayerManager::getInstance().getByName("ControllerMenus", false);
            if (layer == nullptr)
                return;
            mRoot = MyGUI::Gui::getInstance().createWidget<MyGUI::Widget>(
                "", MyGUI::IntCoord(MyGUI::IntPoint(), layer->getSize()), MyGUI::Align::Default, "ControllerMenus");
            mRoot->setNeedMouseFocus(false);
        }
        mRoot->setSize(mRoot->getLayer()->getSize());
        mRoot->setVisible(true);
        mHeldDirection.reset();
        update();
    }

    void DialoguePanel::hide(bool conversationEnded)
    {
        if (mRoot != nullptr)
            mRoot->setVisible(false);
        // Opening a service, such as barter, hides the panel until the player comes back to the conversation.
        if (!conversationEnded)
            return;
        mShownEntries = 0;
        mTab = Tab::Topics;
        mFocus = 0;
        mScroll = 0;
        mFocusedLabel.clear();
        mFocusOnTop = true;
    }

    bool DialoguePanel::inChoice() const
    {
        return !mWindow.mChoices.empty();
    }

    void DialoguePanel::update()
    {
        if (mRoot == nullptr || !mRoot->getVisible())
            return;
        clearChildren(mRoot);
        const int width = mRoot->getWidth();
        addRect(mRoot, sPanelMargin, sPanelTop, width - 2 * sPanelMargin, sPanelBottom - sPanelTop, sPanelBackColour);
        addRect(mRoot, sPanelMargin, sPanelTop, width - 2 * sPanelMargin, 1, sAccentColour, 0.6f);
        const Columns cols = columns(width);
        // A faint line between the conversation and the options.
        addRect(mRoot, cols.mOptionsX - 24, sPanelTop + 12, 1, sPanelBottom - sPanelTop - 24, sAccentColour, 0.15f);

        mConversation = mRoot->createWidget<MyGUI::Widget>(
            "", MyGUI::IntCoord(0, 0, width, sPanelBottom), MyGUI::Align::Default);
        mConversation->setNeedMouseFocus(false);
        mOptions = mRoot->createWidget<MyGUI::Widget>(
            "", MyGUI::IntCoord(0, 0, width, sPanelBottom), MyGUI::Align::Default);
        mOptions->setNeedMouseFocus(false);
        mHints = mRoot->createWidget<MyGUI::Widget>(
            "", MyGUI::IntCoord(0, 0, width, sPanelBottom), MyGUI::Align::Default);
        mHints->setNeedMouseFocus(false);

        // A new answer starts the options again from the top, where the topics it mentions are.
        const std::size_t entries = mWindow.mHistoryContents.size();
        if (entries != mShownEntries && mTab == Tab::Topics)
        {
            mFocus = 0;
            mScroll = 0;
            mFocusOnTop = true;
        }
        drawConversation();
        drawOptions();
    }

    // ---------------------------------------------------------------------------------------------------------------
    // The conversation

    std::vector<std::string> DialoguePanel::mentionedTopics() const
    {
        // The topics the answers mention, newest answer first, as lower case topic IDs.
        MWBase::WindowManager& windowManager = *MWBase::Environment::get().getWindowManager();
        const Translation::Storage& translationStorage = windowManager.getTranslationDataStorage();
        std::vector<std::string> mentioned;
        for (auto it = mWindow.mHistoryContents.rbegin(); it != mWindow.mHistoryContents.rend(); ++it)
        {
            if (dynamic_cast<const Response*>(it->get()) == nullptr)
                continue;
            for (const auto& match : mWindow.mKeywordSearch.parseHyperText((*it)->mText, translationStorage))
                if (std::find(mentioned.begin(), mentioned.end(), match.mTopicId) == mentioned.end())
                    mentioned.push_back(match.mTopicId);
        }
        return mentioned;
    }

    void DialoguePanel::drawConversation()
    {
        const Columns cols = columns(mRoot->getWidth());
        const MWWorld::Ptr& actor = mWindow.mPtr;

        // The NPC's name and disposition.
        const std::string name = actor.isEmpty() ? std::string() : escaped(actor.getClass().getName(actor));
        MyGUI::TextBox* title
            = addText(mConversation, cols.mTextX, sPanelTop + 10, cols.mTextW, 34, name, sFontTitle, sBrightColour);
        const int nameW = title->getTextSize().width;
        mShownDisposition = -1;
        if (!actor.isEmpty() && actor.getClass().isNpc())
        {
            mShownDisposition = MWBase::Environment::get().getMechanicsManager()->getDerivedDisposition(actor);
            const int dx = cols.mTextX + nameW + 20;
            MyGUI::TextBox* label = addText(mConversation, dx, sPanelTop + 14, 240, 30,
                "#{Interface:DialogueDisposition}  " + std::to_string(mShownDisposition), sFontSmall, sDimColour);
            const int barX = dx + label->getTextSize().width + 12;
            addRect(mConversation, barX, sPanelTop + 26, 80, 6, MyGUI::Colour::Black, 0.8f);
            addRect(mConversation, barX, sPanelTop + 26, std::clamp(mShownDisposition, 0, 100) * 80 / 100, 6,
                sDispositionColour);
        }

        // A hint for the right stick, shown when earlier answers are above.
        mEarlierHint = mConversation->createWidget<MyGUI::Widget>(
            "", MyGUI::IntCoord(cols.mTextX + cols.mTextW - 120, sPanelTop + 12, 120, 30), MyGUI::Align::Default);
        mEarlierHint->setNeedMouseFocus(false);
        addGlyph(mEarlierHint, 0, 4, 22, SDL_CONTROLLER_BUTTON_RIGHTSTICK);
        addText(mEarlierHint, 28, 0, 92, 30, "#{Interface:DialogueEarlier}", sFontSmall, sDimColour);
        addRect(mConversation, cols.mTextX, sHistoryTop - 8, cols.mTextW, 1, sAccentColour, 0.2f);

        // Every answer, one after the other, in a column the right stick scrolls.
        const int viewH = sPanelBottom - sHintsHeight - sHistoryTop;
        MyGUI::Widget* clip = mConversation->createWidget<MyGUI::Widget>(
            "", MyGUI::IntCoord(cols.mTextX, sHistoryTop, cols.mTextW, viewH), MyGUI::Align::Default);
        clip->setNeedMouseFocus(false);
        mHistory = clip->createWidget<MyGUI::Widget>(
            "", MyGUI::IntCoord(0, 0, cols.mTextW, viewH), MyGUI::Align::Default);
        mHistory->setNeedMouseFocus(false);

        MWBase::WindowManager& windowManager = *MWBase::Environment::get().getWindowManager();
        const Translation::Storage& translationStorage = windowManager.getTranslationDataStorage();
        int y = 0;
        mLatestTop = 0;
        for (const auto& entry : mWindow.mHistoryContents)
        {
            const auto* response = dynamic_cast<const Response*>(entry.get());
            if (response == nullptr)
            {
                // Messages, such as a journal update.
                y += addWrappedText(mHistory, 0, y, cols.mTextW, escaped(entry->mText), sFontNormal, sDimColour)
                         ->getHeight()
                    + 10;
                continue;
            }
            mLatestTop = y;
            if (!response->mTitle.empty())
            {
                addText(mHistory, 0, y, cols.mTextW, 28, escaped(response->mTitle), sTextSize, sBrightColour);
                y += 30;
            }
            // The topics an answer mentions are blue, as in the normal dialogue window.
            std::string text;
            const std::string& raw = response->mText;
            auto pos = raw.cbegin();
            for (const auto& match : mWindow.mKeywordSearch.parseHyperText(raw, translationStorage))
            {
                text += escaped(std::string(pos, match.mBeg));
                const std::string shown = escaped(match.getDisplayName());
                if (mWindow.mTopicLinks.contains(match.mTopicId))
                    text += sKeywordColourTag + shown + sTextColourTag;
                else
                    text += shown;
                pos = match.mEnd;
            }
            text += escaped(std::string(pos, raw.cend()));
            y += addWrappedText(mHistory, 0, y, cols.mTextW, text, sTextSize, sTextColour)->getHeight() + 14;
        }
        mHistoryHeight = y;
        mHistory->setSize(cols.mTextW, std::max(y, viewH));

        // A new answer scrolls the conversation so it starts at the top.
        if (mWindow.mHistoryContents.size() != mShownEntries)
        {
            mHistoryScroll = static_cast<float>(mLatestTop);
            mShownEntries = mWindow.mHistoryContents.size();
        }
        placeHistory();
    }

    void DialoguePanel::placeHistory()
    {
        if (mHistory == nullptr)
            return;
        const int viewH = mHistory->getParent()->getHeight();
        const float maxScroll = static_cast<float>(std::max(mLatestTop, mHistoryHeight - viewH));
        mHistoryScroll = std::clamp(mHistoryScroll, 0.f, std::max(0.f, maxScroll));
        mHistory->setPosition(0, -static_cast<int>(mHistoryScroll));
        if (mEarlierHint != nullptr)
            mEarlierHint->setVisible(mHistoryScroll > 1.f);
    }

    // ---------------------------------------------------------------------------------------------------------------
    // The options

    std::vector<DialoguePanel::Row> DialoguePanel::buildRows() const
    {
        std::vector<Row> rows;
        if (inChoice())
        {
            for (const auto& [text, id] : mWindow.mChoices)
            {
                Row row;
                row.mLabel = text;
                row.mChoice = id;
                rows.push_back(std::move(row));
            }
            return rows;
        }
        if (mWindow.mGoodbye)
        {
            Row row;
            row.mLabel = gameSetting("sGoodbye");
            rows.push_back(std::move(row));
            return rows;
        }
        if (mTab == Tab::Services)
        {
            for (const std::string& service : mWindow.getServiceNames())
            {
                Row row;
                row.mLabel = service;
                row.mDetail = serviceDetail(service);
                rows.push_back(std::move(row));
            }
            return rows;
        }

        // New topics come first: the ones just mentioned, then the ones this NPC has their own answer to, then the
        // rest. Topics with nothing new to hear come last, as Asked.
        MWBase::DialogueManager* dialogueManager = MWBase::Environment::get().getDialogueManager();
        const std::vector<std::string> mentioned = mentionedTopics();
        std::vector<Row> fresh;
        std::vector<Row> asked;
        std::vector<std::pair<int, Row>> ranked;
        for (const std::string& keyword : mWindow.mKeywords)
        {
            const int flags = dialogueManager->getTopicFlag(ESM::RefId::stringRefId(keyword));
            Row row;
            row.mLabel = keyword;
            if (flags & MWBase::DialogueManager::TopicType::Exhausted)
            {
                row.mAsked = true;
                asked.push_back(std::move(row));
                continue;
            }
            const auto found = std::find(mentioned.begin(), mentioned.end(), Misc::StringUtils::lowerCase(keyword));
            int rank = static_cast<int>(mentioned.size()) + 2;
            if (found != mentioned.end())
            {
                row.mMentioned = true;
                rank = static_cast<int>(found - mentioned.begin());
            }
            else if (flags & MWBase::DialogueManager::TopicType::Specific)
                rank = static_cast<int>(mentioned.size()) + 1;
            ranked.emplace_back(rank, std::move(row));
        }
        std::stable_sort(
            ranked.begin(), ranked.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        if (!ranked.empty())
        {
            rows.push_back(Row{ "#{Interface:DialogueNewTopics}" });
            for (auto& [rank, row] : ranked)
                rows.push_back(std::move(row));
        }
        if (!asked.empty())
        {
            rows.push_back(Row{ "#{Interface:DialogueAskedTopics}" });
            for (Row& row : asked)
                rows.push_back(std::move(row));
        }
        return rows;
    }

    void DialoguePanel::drawOptions()
    {
        clearChildren(mOptions);
        clearChildren(mHints);
        const Columns cols = columns(mRoot->getWidth());
        const int x = cols.mOptionsX;
        const int w = cols.mOptionsW;
        int y = sPanelTop + 12;

        const std::vector<Row> rows = buildRows();
        int optionCount = 0;
        for (const Row& row : rows)
            optionCount += row.mHeader.empty() ? 1 : 0;

        // Keep the focus on the same option when the list changes around it.
        if (mFocusOnTop)
        {
            mFocus = 0;
            mFocusedLabel.clear();
        }
        else if (!mFocusedLabel.empty())
        {
            for (int i = 0, index = 0; i < static_cast<int>(rows.size()); ++i)
            {
                if (!rows[i].mHeader.empty())
                    continue;
                if (rows[i].mLabel == mFocusedLabel)
                    mFocus = index;
                ++index;
            }
        }
        mFocus = std::clamp(mFocus, 0, std::max(0, optionCount - 1));

        if (inChoice())
        {
            addText(mOptions, x, y, w, 28, "#{Interface:DialogueChooseReply}", sFontNormal, sBrightColour);
            addRect(mOptions, x, y + 32, w, 1, sAccentColour, 0.2f);
            y += 44;
        }
        else if (!mWindow.mGoodbye)
        {
            // The Topics and Services tabs, switched with the shoulder buttons.
            addGlyph(mOptions, x, y + 5, 22, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
            int tx = x + 32;
            for (Tab tab : { Tab::Topics, Tab::Services })
            {
                const bool selected = tab == mTab;
                MyGUI::TextBox* label = addText(mOptions, tx, y, 200, 32,
                    tab == Tab::Topics ? "#{Interface:DialogueTabTopics}" : "#{Interface:DialogueTabServices}",
                    sFontNormal, selected ? sBrightColour : sDimColour);
                const int tabW = label->getTextSize().width + 24;
                label->setCoord(tx, y, tabW, 32);
                label->setTextAlign(MyGUI::Align::Center);
                if (selected)
                    addRect(mOptions, tx + 8, y + 30, tabW - 16, 2, sAccentColour);
                tx += tabW + 4;
            }
            addGlyph(mOptions, tx + 2, y + 5, 22, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
            y += 44;
        }

        // The rows, scrolled so the focused one is in view.
        const int listBottom = sPanelBottom - 8;
        const auto rowHeight = [&](const Row& row) {
            if (!row.mHeader.empty())
                return sRowHeight;
            if (row.mChoice >= 0)
                return static_cast<int>(wrapText(mOptions, row.mLabel, w - 16, sFontNormal, 4).size()) * 24 + 10;
            return row.mDetail.empty() ? sRowHeight : sServiceRowHeight;
        };
        int focusRow = 0;
        for (int i = 0, index = 0; i < static_cast<int>(rows.size()); ++i)
        {
            if (!rows[i].mHeader.empty())
                continue;
            if (index++ == mFocus)
                focusRow = i;
        }
        if (focusRow < mScroll)
            mScroll = focusRow > 0 && !rows[focusRow - 1].mHeader.empty() ? focusRow - 1 : focusRow;
        while (mScroll < focusRow)
        {
            int bottom = y;
            for (int i = mScroll; i <= focusRow; ++i)
                bottom += rowHeight(rows[i]);
            if (bottom <= listBottom)
                break;
            ++mScroll;
        }
        // Don't scroll past the end of the list, which would leave empty space below it.
        int lastScroll = static_cast<int>(rows.size());
        for (int i = static_cast<int>(rows.size()) - 1, used = 0; i >= 0; --i)
        {
            used += rowHeight(rows[i]);
            if (y + used > listBottom)
                break;
            lastScroll = i;
        }
        mScroll = std::clamp(mScroll, 0, std::max(0, lastScroll));

        int index = 0;
        for (int i = 0; i < mScroll; ++i)
            index += rows[i].mHeader.empty() ? 1 : 0;
        for (int i = mScroll; i < static_cast<int>(rows.size()); ++i)
        {
            const Row& row = rows[i];
            const int h = rowHeight(row);
            if (y + h > listBottom)
                break;
            if (!row.mHeader.empty())
            {
                addText(mOptions, x, y + 2, w, 22, row.mHeader, sFontSmall, sDimColour);
                addRect(mOptions, x, y + 23, w, 1, sAccentColour, 0.2f);
                y += h;
                continue;
            }
            const bool focused = index == mFocus;
            if (focused)
            {
                if (!mFocusOnTop)
                    mFocusedLabel = row.mLabel;
                addRect(mOptions, x - 8, y, w + 8, h - 2, sAccentColour, 0.16f);
                addRect(mOptions, x - 8, y, 3, h - 2, sAccentColour);
            }
            MyGUI::Colour colour = row.mAsked ? sDimColour : (row.mMentioned ? sNewColour : sBrightColour);
            if (focused)
                colour = sActiveColour;
            if (row.mChoice >= 0)
            {
                const std::vector<std::string> lines = wrapText(mOptions, row.mLabel, w - 16, sFontNormal, 4);
                for (std::size_t line = 0; line < lines.size(); ++line)
                    addText(mOptions, x + 4, y + 5 + static_cast<int>(line) * 24, w - 8, 24, escaped(lines[line]),
                        sFontNormal, colour);
            }
            else if (!row.mDetail.empty())
            {
                addText(mOptions, x + 4, y + 2, w - 8, 26, escaped(row.mLabel), sFontNormal, colour);
                addText(mOptions, x + 4, y + 24, w - 8, 22, row.mDetail, sFontSmall, sDimColour);
            }
            else
                addText(mOptions, x + 4, y, w - 8, h - 2, escaped(row.mLabel), sFontNormal, colour);
            y += h;
            ++index;
        }

        // What the buttons do now.
        std::vector<std::pair<SDL_GameControllerButton, std::string>> hints;
        if (inChoice())
            hints.emplace_back(SDL_CONTROLLER_BUTTON_A, "#{Interface:DialogueReply}");
        else if (mWindow.mGoodbye)
            hints.emplace_back(SDL_CONTROLLER_BUTTON_B, "#{Interface:DialogueLeave}");
        else
        {
            if (optionCount > 0)
                hints.emplace_back(SDL_CONTROLLER_BUTTON_A,
                    mTab == Tab::Topics ? "#{Interface:DialogueAsk}" : "#{Interface:DialogueOpen}");
            int groups = 0;
            for (const Row& row : rows)
                groups += row.mHeader.empty() ? 0 : 1;
            if (groups > 1)
                hints.emplace_back(SDL_CONTROLLER_BUTTON_Y, "#{Interface:MagicNextGroup}");
            hints.emplace_back(SDL_CONTROLLER_BUTTON_B, "#{Interface:DialogueLeave}");
        }
        drawHints(hints);
    }

    void DialoguePanel::drawHints(const std::vector<std::pair<SDL_GameControllerButton, std::string>>& hints)
    {
        int x = columns(mRoot->getWidth()).mTextX;
        const int y = sPanelBottom - 36;
        for (const auto& [button, label] : hints)
        {
            addGlyph(mHints, x, y + 1, 24, button);
            MyGUI::TextBox* text = addText(mHints, x + 32, y - 3, 220, 30, label, sFontNormal, sBrightColour);
            x += 32 + text->getTextSize().width + 26;
        }
    }

    // ---------------------------------------------------------------------------------------------------------------
    // Input

    void DialoguePanel::onFrame(float duration)
    {
        if (mRoot == nullptr || !mRoot->getVisible())
            return;

        // The disposition changes after persuasion, and the dialogue window doesn't redraw for it.
        const MWWorld::Ptr& actor = mWindow.mPtr;
        if (!actor.isEmpty() && actor.getClass().isNpc()
            && MWBase::Environment::get().getMechanicsManager()->getDerivedDisposition(actor) != mShownDisposition)
            update();

        MWBase::WindowManager* winMgr = MWBase::Environment::get().getWindowManager();
        if (winMgr->getActiveControllerWindow() != &mWindow)
        {
            mHeldDirection.reset();
            return;
        }
        int dx = 0;
        int dy = 0;
        if (mHeldDirection.update(duration, dx, dy) && dy != 0)
            move(dy);

        // The right stick scrolls the conversation. Squaring the deflection gives finer control near the centre.
        const float stick = MWBase::Environment::get().getInputManager()->getControllerAxisValue(
            SDL_CONTROLLER_AXIS_RIGHTY);
        const float magnitude = std::abs(stick);
        if (magnitude >= sStickDeadzone)
        {
            const float scaled = (magnitude - sStickDeadzone) / (1.f - sStickDeadzone);
            mHistoryScroll += std::copysign(scaled * scaled, stick) * sHistoryScrollSpeed * duration;
            placeHistory();
        }
    }

    bool DialoguePanel::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
    {
        MWBase::WindowManager* winMgr = MWBase::Environment::get().getWindowManager();
        switch (arg.button)
        {
            case SDL_CONTROLLER_BUTTON_A:
                activate();
                break;
            case SDL_CONTROLLER_BUTTON_B:
                // A choice has to be answered before the conversation can end.
                if (!inChoice())
                    mWindow.onGoodbyeActivated();
                break;
            case SDL_CONTROLLER_BUTTON_Y:
                nextGroup();
                break;
            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                // There are only two tabs, so either button switches to the other.
                if (!inChoice() && !mWindow.mGoodbye)
                {
                    mTab = mTab == Tab::Topics ? Tab::Services : Tab::Topics;
                    mFocus = 0;
                    mScroll = 0;
                    mFocusedLabel.clear();
                    winMgr->playSound(ESM::RefId::stringRefId("Menu Click"));
                    drawOptions();
                }
                break;
            default:
                // The d-pad is read every frame in onFrame, so it can repeat while held.
                break;
        }
        return true;
    }

    void DialoguePanel::move(int delta)
    {
        int optionCount = 0;
        for (const Row& row : buildRows())
            optionCount += row.mHeader.empty() ? 1 : 0;
        if (optionCount == 0)
            return;
        mFocus = std::clamp(mFocus + delta, 0, optionCount - 1);
        mFocusedLabel.clear();
        mFocusOnTop = false;
        drawOptions();
    }

    void DialoguePanel::nextGroup()
    {
        // Moves the selection to the first option of the next group, with its header at the top of the list.
        const std::vector<Row> rows = buildRows();
        std::vector<std::pair<int, int>> groupStarts;
        for (int i = 0, index = 0; i < static_cast<int>(rows.size()); ++i)
        {
            if (!rows[i].mHeader.empty())
            {
                groupStarts.emplace_back(index, i);
                continue;
            }
            ++index;
        }
        if (groupStarts.size() < 2)
            return;
        auto next = std::find_if(
            groupStarts.begin(), groupStarts.end(), [&](const auto& start) { return start.first > mFocus; });
        if (next == groupStarts.end())
            next = groupStarts.begin();
        mFocus = next->first;
        mScroll = next->second;
        mFocusedLabel.clear();
        mFocusOnTop = false;
        MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
        drawOptions();
    }

    void DialoguePanel::activate()
    {
        const std::vector<Row> rows = buildRows();
        int index = 0;
        for (const Row& row : rows)
        {
            if (!row.mHeader.empty())
                continue;
            if (index++ != mFocus)
                continue;
            MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
            if (row.mChoice >= 0)
                mWindow.onChoiceActivated(row.mChoice);
            else if (mWindow.mGoodbye)
                mWindow.onGoodbyeActivated();
            else
                mWindow.onSelectListItem(row.mLabel, 0);
            return;
        }
    }
}
