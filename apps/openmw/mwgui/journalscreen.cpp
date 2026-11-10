#include "journalscreen.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

#include <cstdio>

#include <MyGUI_Gui.h>
#include <MyGUI_ImageBox.h>
#include <MyGUI_TextBox.h>

#include <components/misc/strings/algorithm.hpp>
#include <components/misc/strings/lower.hpp>
#include <components/translation/translation.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/journal.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwdialogue/journalentry.hpp"
#include "../mwdialogue/topic.hpp"

namespace MWGui
{
    namespace
    {
        using namespace ControllerUi;

        constexpr int sEntryHeight = 56;
        constexpr int sHeaderRowHeight = 28;
        constexpr int sColumnsHeight = 30;
        constexpr int sPlaceColumnWidth = 170;
        constexpr int sFactionColumnWidth = 180;
        constexpr float sScrollSpeed = 700.f;
        constexpr int sLineHeight = sFontNormal + 6;

        // A colour as a tag in a text box's caption, such as "#7FB3FF", which colours the text after it.
        std::string colourTag(const MyGUI::Colour& colour)
        {
            const auto channel
                = [](float value) { return static_cast<int>(std::lround(std::clamp(value, 0.f, 1.f) * 255)); };
            char tag[8];
            std::snprintf(
                tag, sizeof(tag), "#%02X%02X%02X", channel(colour.red), channel(colour.green), channel(colour.blue));
            return tag;
        }

        std::string tabKey(int tab)
        {
            static const std::array<std::string_view, 3> names = { "Recent", "Places", "Factions" };
            return "#{Interface:JournalTab" + std::string(names[tab]) + "}";
        }

        // Guild and House quests have the faction in their name, such as "Fighters Guild: Exterminator". The list
        // shows the faction in its own column, so the name leaves it out.
        std::string shortName(const QuestLog::Quest& quest)
        {
            const std::size_t colon = quest.mName.find(':');
            if (colon == std::string::npos || quest.mFaction.empty())
                return quest.mName;
            if (!Misc::StringUtils::ciEndsWith(QuestLog::factionName(quest), quest.mName.substr(0, colon)))
                return quest.mName;
            const std::size_t start = quest.mName.find_first_not_of(' ', colon + 1);
            return start != std::string::npos ? quest.mName.substr(start) : quest.mName;
        }
    }

    JournalScreen::JournalScreen() = default;

    void JournalScreen::clear()
    {
        mTab = Tab::Recent;
        mQuests.clear();
        mSelected.clear();
        mList.reset();
        mEntriesPage.clear();
        mInDetails = false;
        mTopicPages.clear();
        mKeywordSearch.clear();
        mTopics.clear();
    }

    void JournalScreen::onOpen()
    {
        mList.resetHeld();
        mLinkHeld.reset();
        mInDetails = false;
        mTopicPages.clear();
        // The journal can't change while the menus are open, so the quests and topics are read once.
        mQuests = QuestLog::collect();
        mKeywordSearch.clear();
        mTopics.clear();
        const Translation::Storage& translations
            = MWBase::Environment::get().getWindowManager()->getTranslationDataStorage();
        for (const auto& [id, topic] : MWBase::Environment::get().getJournal()->getTopics())
        {
            const std::string key = Misc::StringUtils::lowerCase(topic.getName());
            mTopics[key] = &topic;
            mKeywordSearch.seed(translations.topicKeyword(topic.getName()), key);
        }
        redraw();
    }

    void JournalScreen::onFrame(float duration)
    {
        if (!isVisible() || MWBase::Environment::get().getWindowManager()->getActiveControllerWindow() != this)
        {
            mList.resetHeld();
            return;
        }
        // In the details, the d-pad steps from topic to topic. In the list, every tab lists all the quests, in its own
        // order.
        if (mInDetails)
        {
            int dx = 0;
            int dy = 0;
            if (mLinkHeld.update(duration, dx, dy))
                moveLink(dy != 0 ? dy : dx);
        }
        else if (mList.updateHeld(duration, static_cast<int>(mQuests.size())))
            selectFocused();
        scrollEntries(
            MWBase::Environment::get().getInputManager()->getControllerAxisValue(SDL_CONTROLLER_AXIS_RIGHTY), duration);
    }

    bool JournalScreen::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
    {
        MWBase::WindowManager* winMgr = MWBase::Environment::get().getWindowManager();
        switch (arg.button)
        {
            case SDL_CONTROLLER_BUTTON_A:
                activateDetails();
                break;
            case SDL_CONTROLLER_BUTTON_B:
                if (!stepBack())
                    winMgr->exitCurrentGuiMode();
                break;
            case SDL_CONTROLLER_BUTTON_X:
                if (!mInDetails)
                    togglePinned();
                break;
            case SDL_CONTROLLER_BUTTON_Y:
                if (!mInDetails)
                    nextGroup();
                break;
            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                changeTab(-1);
                winMgr->playSound(ESM::RefId::stringRefId("Menu Click"));
                break;
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                changeTab(1);
                winMgr->playSound(ESM::RefId::stringRefId("Menu Click"));
                break;
            default:
                // The d-pad is read every frame in onFrame, so it can repeat while held.
                break;
        }
        return true;
    }

    bool JournalScreen::onControllerThumbstickEvent(const SDL_ControllerAxisEvent& arg)
    {
        // The left stick moves the selection and the right stick scrolls the entries. Both are read every frame in
        // onFrame.
        return arg.axis == SDL_CONTROLLER_AXIS_LEFTX || arg.axis == SDL_CONTROLLER_AXIS_LEFTY
            || arg.axis == SDL_CONTROLLER_AXIS_RIGHTX || arg.axis == SDL_CONTROLLER_AXIS_RIGHTY;
    }

    // ---------------------------------------------------------------------------------------------------------------
    // The list

    std::string JournalScreen::groupOf(const QuestLog::Quest& quest) const
    {
        if (quest.mPinned)
            return "#{Interface:JournalPinned}";
        if (quest.mFinished)
            return "#{Interface:JournalFinished}";
        switch (mTab)
        {
            case Tab::Places:
                return quest.mTown.empty() ? "#{Interface:JournalUnknownPlace}" : escaped(quest.mTown);
            case Tab::Factions:
            {
                const std::string faction = QuestLog::factionName(quest);
                return faction.empty() ? "#{Interface:JournalNoFaction}" : escaped(faction);
            }
            default:
                return "#{Interface:JournalInProgress}";
        }
    }

    std::vector<int> JournalScreen::order() const
    {
        // mQuests is already in order of the most recent activity. Pinned quests come first, then quests in
        // progress. On the Places and Factions tabs, the quests in progress are grouped by name, with quests that have
        // no place or faction after the rest.
        std::vector<int> result(mQuests.size());
        for (std::size_t i = 0; i < result.size(); ++i)
            result[i] = static_cast<int>(i);
        const auto groupKey = [&](const QuestLog::Quest& quest) -> std::pair<int, std::string> {
            if (quest.mPinned)
                return { -1, {} };
            if (quest.mFinished)
                return { 2, {} };
            if (mTab == Tab::Places)
                return { quest.mTown.empty() ? 1 : 0, Misc::StringUtils::lowerCase(quest.mTown) };
            if (mTab == Tab::Factions)
            {
                const std::string faction = QuestLog::factionName(quest);
                return { faction.empty() ? 1 : 0, Misc::StringUtils::lowerCase(faction) };
            }
            return { 0, {} };
        };
        std::stable_sort(
            result.begin(), result.end(), [&](int a, int b) { return groupKey(mQuests[a]) < groupKey(mQuests[b]); });
        return result;
    }

    std::vector<JournalScreen::Row> JournalScreen::buildRows(const std::vector<int>& order) const
    {
        std::vector<Row> rows;
        std::string group;
        for (std::size_t i = 0; i < order.size(); ++i)
        {
            std::string header = groupOf(mQuests[order[i]]);
            if (rows.empty() || header != group)
            {
                group = header;
                rows.push_back(Row{ std::move(header), -1 });
            }
            rows.push_back(Row{ {}, static_cast<int>(i) });
        }
        return rows;
    }

    std::vector<int> JournalScreen::groupStarts(const std::vector<Row>& rows)
    {
        // The first entry under each group header.
        std::vector<int> starts;
        for (std::size_t i = 0; i + 1 < rows.size(); ++i)
            if (rows[i].mEntry == -1 && rows[i + 1].mEntry >= 0)
                starts.push_back(rows[i + 1].mEntry);
        return starts;
    }

    int JournalScreen::focusIndex(const std::vector<int>& order) const
    {
        for (std::size_t i = 0; i < order.size(); ++i)
            if (mQuests[order[i]].mName == mSelected)
                return static_cast<int>(i);
        return 0;
    }

    // ---------------------------------------------------------------------------------------------------------------
    // Drawing

    void JournalScreen::redraw()
    {
        clearContent();
        mEntriesPaper = nullptr;
        mEntriesScrollThumb = nullptr;
        const MyGUI::IntSize size = mMainWidget->getSize();

        int finished = 0;
        for (const QuestLog::Quest& quest : mQuests)
            finished += quest.mFinished ? 1 : 0;
        std::vector<std::string> captions;
        for (int tab = 0; tab < sTabCount; ++tab)
            captions.push_back(tabKey(tab));
        drawTabRow(mContent, size.width, captions, static_cast<int>(mTab),
            std::to_string(mQuests.size() - finished) + " " + localised("#{Interface:JournalCountInProgress}") + "    "
                + std::to_string(finished) + " " + localised("#{Interface:JournalCountFinished}"));

        const int top = sContentTop;
        const int bottom = size.height - sContentBottomMargin;
        const int panelW = std::max(320, static_cast<int>(size.width * 0.36f));
        const int listW = size.width - 2 * sMargin - panelW - 16;
        const int px = size.width - sMargin - panelW;

        const std::vector<int> quests = order();
        const std::vector<Row> rows = buildRows(quests);
        if (quests.empty())
        {
            addRect(mContent, sMargin, top, listW, bottom - top, sPanelColour, 0.9f);
            addText(mContent, sMargin + 16, top + 16, listW - 32, 30, "#{Interface:JournalNoQuests}", sFontNormal,
                sDimColour);
            addRect(mContent, px, top, panelW, bottom - top, sPanelColour, 0.9f);
            return;
        }
        const int focus = focusIndex(quests);
        mSelected = mQuests[quests[focus]].mName;
        mList.setFocus(focus);
        drawList(sMargin, top, listW, bottom - top, quests, rows);
        drawDetails(px, top, panelW, bottom - top, mQuests[quests[focus]], groupStarts(rows).size() > 1);
    }

    void JournalScreen::drawList(
        int x, int y, int w, int h, const std::vector<int>& order, const std::vector<Row>& rows)
    {
        addRect(mContent, x, y, w, h, sPanelColour, 0.9f);
        int cx = x + w - 12 - sPlaceColumnWidth - sFactionColumnWidth;
        addText(mContent, cx, y + 6, sPlaceColumnWidth, 22, "#{Interface:JournalPlace}", sFontSmall, sDimColour);
        addText(mContent, cx + sPlaceColumnWidth, y + 6, sFactionColumnWidth, 22, "#{Interface:JournalFaction}",
            sFontSmall, sDimColour);

        std::vector<ControllerList::Row> listRows;
        for (const Row& row : rows)
            listRows.push_back({ row.mEntry < 0 ? sHeaderRowHeight : sEntryHeight, row.mEntry >= 0 });
        const auto drawRow = [&](std::size_t i, int rowY, bool focused) {
            const Row& row = rows[i];
            if (row.mEntry == -1)
            {
                addText(mContent, x + 12, rowY, w - 24, 24, row.mHeader, sFontSmall, sBrightColour);
                addRect(mContent, x + 12, rowY + 23, w - 24, 1, sAccentColour, 0.25f);
                return;
            }
            drawEntry(x + 4, rowY, w - 8, sEntryHeight - 2, mQuests[order[row.mEntry]], focused);
        };
        const int top = y + sColumnsHeight;
        const int listBottom = y + h - 4;
        // Pointing at a quest or clicking it selects it, to show its entries. A does nothing more here.
        mList.draw(mContent, x, top, w, listBottom - top, listRows, drawRow, [this] { selectFocused(); }, nullptr);
    }

    void JournalScreen::drawEntry(int x, int y, int w, int h, const QuestLog::Quest& quest, bool focused)
    {
        if (focused)
        {
            addRect(mContent, x, y, w, h, sAccentColour, 0.16f);
            addRect(mContent, x, y, 3, h, sAccentColour);
        }
        const MyGUI::Colour& nameColour = focused ? sActiveColour : quest.mFinished ? sTextColour : sBrightColour;
        const int columnsX = x + w - 8 - sPlaceColumnWidth - sFactionColumnWidth;
        const int nameW = columnsX - x - 16 - 12;
        addText(mContent, x + 12, y + 4, nameW, 26, escaped(shortName(quest)), sFontNormal, nameColour);
        addText(mContent, x + 12, y + 29, nameW, 20, escaped(QuestLog::timestamp(*quest.mEntries.back())), sFontSmall,
            sDimColour);
        addText(mContent, columnsX, y, sPlaceColumnWidth - 8, h, escaped(quest.mTown), sFontSmall, sTextColour);
        addText(mContent, columnsX + sPlaceColumnWidth, y, sFactionColumnWidth - 4, h,
            escaped(QuestLog::factionName(quest)), sFontSmall, sTextColour);
    }

    void JournalScreen::drawDetails(int x, int y, int w, int h, const QuestLog::Quest& quest, bool groups)
    {
        addRect(mContent, x, y, w, h, sPanelColour, 0.9f);
        addRect(mContent, x, y, w, 1, sAccentColour, 0.3f);
        const int innerX = x + 16;
        const int innerW = w - 32;
        int ty = y + 12;
        const MWDialogue::Topic* topic = mTopicPages.empty() ? nullptr : mTopicPages.back().mTopic;

        if (topic == nullptr)
        {
            for (const std::string& line : wrapText(mContent, quest.mName, innerW, sFontLarge, 2))
            {
                addText(mContent, innerX, ty, innerW, 32, escaped(line), sFontLarge, sBrightColour);
                ty += 30;
            }
            addText(mContent, innerX, ty, innerW, 22,
                quest.mFinished ? "#{Interface:JournalFinished}" : "#{Interface:JournalInProgress}", sFontSmall,
                quest.mFinished ? sBetterColour : sNewColour);
            ty += 30;

            // Who gave the quest, where, and for whom, when the game knows.
            const auto fact = [&](const std::string& label, const std::string& value) {
                if (value.empty())
                    return;
                addText(mContent, innerX, ty, 100, 26, label, sFontSmall, sDimColour);
                addText(mContent, innerX + 100, ty, innerW - 100, 26, escaped(value), sFontNormal, sTextColour);
                ty += 26;
            };
            fact("#{Interface:JournalGivenBy}", QuestLog::giverName(quest));
            fact("#{Interface:JournalPlace}", quest.mPlace);
            fact("#{Interface:JournalFaction}", QuestLog::factionName(quest));
        }
        else
        {
            // The way here from the quest, then the topic's name.
            std::string path = quest.mName;
            for (std::size_t i = 0; i + 1 < mTopicPages.size(); ++i)
                path += " > " + std::string(mTopicPages[i].mTopic->getName());
            addText(mContent, innerX, ty, innerW, 22, escaped(wrapText(mContent, path, innerW, sFontSmall, 1).front()),
                sFontSmall, sDimColour);
            ty += 26;
            for (const std::string& line : wrapText(mContent, std::string(topic->getName()), innerW, sFontLarge, 2))
            {
                addText(mContent, innerX, ty, innerW, 32, escaped(line), sFontLarge, sBrightColour);
                ty += 30;
            }
            addText(mContent, innerX, ty, innerW, 22, "#{Interface:JournalWhatYouHeard}", sFontSmall, sNewColour);
            ty += 30;
        }
        ty += 8;
        addRect(mContent, innerX, ty, innerW, 1, sAccentColour, 0.25f);
        ty += 10;

        // The entries, newest first, on a page the right stick scrolls when they don't fit. They are drawn first, to
        // know which topics they highlight and how tall they are. The area is fitted above the button hints after.
        constexpr int barWidth = 4;
        MyGUI::Widget* area = mContent->createWidget<MyGUI::Widget>(
            "", MyGUI::IntCoord(innerX, ty, innerW, std::max(0, y + h - ty)), MyGUI::Align::Default);
        area->setNeedMouseFocus(false);
        constexpr int unlimitedHeight = 1 << 20;
        const int paperW = innerW - 12;
        mEntriesPaper = area->createWidget<MyGUI::Widget>(
            "", MyGUI::IntCoord(0, 0, paperW, unlimitedHeight), MyGUI::Align::Default);
        mEntriesPaper->setNeedMouseFocus(false);
        mLinks.clear();
        int py = 0;
        if (topic == nullptr)
        {
            for (auto it = quest.mEntries.rbegin(); it != quest.mEntries.rend(); ++it)
            {
                const bool latest = it == quest.mEntries.rbegin();
                addText(mEntriesPaper, 0, py, paperW, 22, escaped(QuestLog::timestamp(**it)), sFontSmall,
                    latest ? sNewColour : sDimColour);
                py += 24;
                py += drawLinkedText(py, paperW, linkTopics((*it)->getText(), nullptr),
                    latest ? sBrightColour : sTextColour);
                py += 14;
            }
        }
        else
        {
            // What each person said about the topic, newest first.
            for (std::size_t i = topic->size(); i-- > 0;)
            {
                const MWDialogue::Entry& entry = (*topic)[i];
                const bool latest = i + 1 == topic->size();
                if (!entry.mActorName.empty())
                {
                    addText(mEntriesPaper, 0, py, paperW, 22, escaped(entry.mActorName), sFontSmall,
                        latest ? sNewColour : sDimColour);
                    py += 24;
                }
                py += drawLinkedText(
                    py, paperW, linkTopics(entry.getText(), topic), latest ? sBrightColour : sTextColour);
                py += 14;
            }
        }
        mEntriesPaper->setSize(paperW, py);
        mLinkFocus = mLinks.empty() ? 0 : std::clamp(mLinkFocus, 0, static_cast<int>(mLinks.size()) - 1);

        // The button hints along the bottom of the panel, on a second row when they don't fit on one. The right
        // stick's hint shows when the entries are taller than the room left above the hints.
        std::vector<Hint> hints;
        if (!mLinks.empty())
            hints.emplace_back(
                SDL_CONTROLLER_BUTTON_A, mInDetails ? "#{Interface:JournalOpenTopic}" : "#{Interface:JournalTopics}");
        if (!mInDetails)
        {
            hints.emplace_back(
                SDL_CONTROLLER_BUTTON_X, quest.mPinned ? "#{Interface:JournalUnpin}" : "#{Interface:JournalPin}");
            if (groups)
                hints.emplace_back(SDL_CONTROLLER_BUTTON_Y, "#{Interface:MagicNextGroup}");
        }
        constexpr int hintsGap = 16;
        std::vector<Hint> scrolling = hints;
        scrolling.emplace_back(SDL_CONTROLLER_BUTTON_RIGHTSTICK, "#{Interface:InventoryScroll}");
        if (py > y + h - hintsHeight(mContent, innerW, scrolling) - hintsGap - ty)
            hints = std::move(scrolling);
        const int areaBottom = drawHints(mContent, innerX, innerW, y + h, hints) - hintsGap;
        area->setSize(innerW, std::max(0, areaBottom - ty));
        // The wheel scrolls the entries too, rather than moving through the quests. Its area goes behind the page, so
        // the topics on the page can be clicked.
        addWheelTarget(mContent, innerX, ty, innerW, std::max(0, areaBottom - ty), [this](int steps) {
            mEntriesScroll = std::clamp(
                mEntriesScroll - steps * sWheelScrollStep, 0.f, static_cast<float>(std::max(0, mEntriesScrollMax)));
            placeEntries();
        })->setDepth(1);

        // Each page starts at its top. In the details, the page scrolls to keep the highlighted topic in view.
        std::string page = quest.mName;
        for (const TopicPage& opened : mTopicPages)
            page += "\n" + std::string(opened.mTopic->getName());
        if (page != mEntriesPage)
        {
            mEntriesPage = page;
            mEntriesScroll = 0.f;
        }
        mEntriesViewHeight = area->getHeight();
        mEntriesScrollMax = std::max(0, py - mEntriesViewHeight);
        if (mInDetails && !mLinks.empty())
        {
            const std::vector<MyGUI::IntCoord>& parts = mLinks[mLinkFocus].mParts;
            const int top = parts.front().top - 8;
            const int bottom = parts.back().bottom() + 8;
            if (top < mEntriesScroll)
                mEntriesScroll = static_cast<float>(top);
            else if (bottom > mEntriesScroll + mEntriesViewHeight)
                mEntriesScroll = static_cast<float>(bottom - mEntriesViewHeight);
        }
        mEntriesScroll = std::clamp(mEntriesScroll, 0.f, static_cast<float>(mEntriesScrollMax));

        if (mEntriesScrollMax > 0)
        {
            mEntriesTrack = MyGUI::IntCoord(innerX + innerW - barWidth, ty, barWidth, areaBottom - ty);
            addRect(mContent, mEntriesTrack.left, mEntriesTrack.top, mEntriesTrack.width, mEntriesTrack.height,
                sAccentColour, 0.12f);
            const float visible = static_cast<float>(mEntriesViewHeight) / py;
            mEntriesScrollThumb = addRect(mContent, mEntriesTrack.left, mEntriesTrack.top, mEntriesTrack.width,
                std::max(16, static_cast<int>(mEntriesTrack.height * visible)), sAccentColour, 0.6f);
        }
        placeEntries();
    }

    JournalScreen::LinkedText JournalScreen::linkTopics(const std::string& text, const MWDialogue::Topic* page) const
    {
        // As in the journal book, the text shows each topic by its display name.
        const Translation::Storage& translations
            = MWBase::Environment::get().getWindowManager()->getTranslationDataStorage();
        LinkedText result;
        MWDialogue::KeywordSearch::Point position = text.begin();
        for (const MWDialogue::KeywordSearch::Match& match : mKeywordSearch.parseHyperText(text, translations))
        {
            const std::string_view name = match.getDisplayName();
            result.mText.append(position, match.mBeg);
            const std::size_t start = result.mText.size();
            result.mText.append(name);
            position = match.mEnd;
            const auto found = mTopics.find(match.mTopicId);
            if (found != mTopics.end() && found->second != page)
                result.mSpans.emplace_back(start, result.mText.size(), found->second);
        }
        result.mText.append(position, text.end());
        return result;
    }

    int JournalScreen::drawLinkedText(int y, int width, const LinkedText& text, const MyGUI::Colour& colour)
    {
        // The text is measured in a hidden text box, a word at a time, to break it into lines that fit.
        MyGUI::TextBox* probe = addText(mEntriesPaper, 0, 0, width, sLineHeight, {}, sFontNormal, colour);
        probe->setVisible(false);
        const std::string& body = text.mText;
        const auto measure = [&](std::size_t from, std::size_t to) {
            probe->setCaption(MyGUI::UString(escaped(std::string_view(body).substr(from, to - from))));
            return probe->getTextSize().width;
        };

        // Each line, as its start and end in the text. A line break in the text starts a new line.
        std::vector<std::pair<std::size_t, std::size_t>> lines;
        std::size_t lineStart = std::string::npos;
        std::size_t lineEnd = 0;
        std::size_t i = 0;
        while (i < body.size())
        {
            if (body[i] == '\n')
            {
                if (lineStart != std::string::npos)
                    lines.emplace_back(lineStart, lineEnd);
                lineStart = std::string::npos;
                ++i;
                continue;
            }
            if (body[i] == ' ' || body[i] == '\t' || body[i] == '\r')
            {
                ++i;
                continue;
            }
            std::size_t wordEnd = i;
            while (wordEnd < body.size() && body[wordEnd] != ' ' && body[wordEnd] != '\t' && body[wordEnd] != '\r'
                && body[wordEnd] != '\n')
                ++wordEnd;
            if (lineStart == std::string::npos)
                lineStart = i;
            else if (measure(lineStart, wordEnd) > width)
            {
                lines.emplace_back(lineStart, lineEnd);
                lineStart = i;
            }
            lineEnd = wordEnd;
            i = wordEnd;
        }
        if (lineStart != std::string::npos)
            lines.emplace_back(lineStart, lineEnd);

        const std::string textTag = colourTag(colour);
        int ly = y;
        std::size_t span = 0;
        for (const auto& [from, to] : lines)
        {
            // The line's text, with each topic in it coloured, and the highlighted topic on a box of its own.
            std::string caption;
            std::size_t at = from;
            for (std::size_t s = span; s < text.mSpans.size(); ++s)
            {
                const auto& [spanStart, spanEnd, spanTopic] = text.mSpans[s];
                if (spanStart >= to)
                    break;
                if (spanEnd <= from)
                    continue;
                const std::size_t partStart = std::max(spanStart, from);
                const std::size_t partEnd = std::min(spanEnd, to);
                // A topic that started on the line before is the same link.
                const bool continued = spanStart < from;
                if (!continued)
                    mLinks.push_back(Link{ spanTopic, {} });
                const int link = static_cast<int>(mLinks.size()) - 1;
                const MyGUI::IntCoord part(measure(from, partStart), ly, measure(partStart, partEnd), sLineHeight);
                mLinks.back().mParts.push_back(part);
                const bool focused = mInDetails && link == mLinkFocus;
                if (focused)
                    addRect(mEntriesPaper, part.left - 3, part.top, part.width + 6, part.height, sAccentColour, 0.3f);
                addPointerTarget(
                    mEntriesPaper, part.left, part.top, part.width, part.height,
                    [this, link] {
                        if (!mInDetails || mLinkFocus == link)
                            return;
                        mLinkFocus = link;
                        redraw();
                    },
                    [this, link] { openLink(link); });
                caption += escaped(std::string_view(body).substr(at, partStart - at));
                caption += colourTag(focused ? sActiveColour : sNewColour);
                caption += escaped(std::string_view(body).substr(partStart, partEnd - partStart));
                caption += textTag;
                at = partEnd;
                if (spanEnd <= to)
                    span = s + 1;
            }
            caption += escaped(std::string_view(body).substr(at, to - at));
            addText(mEntriesPaper, 0, ly, width, sLineHeight, caption, sFontNormal, colour);
            ly += sLineHeight;
        }
        MyGUI::Gui::getInstance().destroyWidget(probe);
        return ly - y;
    }

    void JournalScreen::placeEntries()
    {
        if (mEntriesPaper == nullptr)
            return;
        const int offset = static_cast<int>(mEntriesScroll);
        mEntriesPaper->setPosition(0, -offset);
        if (mEntriesScrollThumb != nullptr && mEntriesScrollMax > 0)
        {
            const int travel = mEntriesTrack.height - mEntriesScrollThumb->getHeight();
            mEntriesScrollThumb->setPosition(
                mEntriesTrack.left, mEntriesTrack.top + travel * offset / mEntriesScrollMax);
        }
    }

    void JournalScreen::scrollEntries(float stick, float duration)
    {
        // Squaring the deflection gives finer control near the centre of the stick.
        const float magnitude = std::abs(stick);
        if (magnitude < sStickDeadzone || mEntriesScrollMax <= 0)
            return;
        const float scaled = (magnitude - sStickDeadzone) / (1.f - sStickDeadzone);
        mEntriesScroll = std::clamp(mEntriesScroll + std::copysign(scaled * scaled, stick) * sScrollSpeed * duration,
            0.f, static_cast<float>(mEntriesScrollMax));
        placeEntries();
    }

    // ---------------------------------------------------------------------------------------------------------------
    // Input

    void JournalScreen::activateDetails()
    {
        if (mLinks.empty())
            return;
        if (!mInDetails)
        {
            mInDetails = true;
            mLinkFocus = 0;
            mLinkHeld.reset();
            MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
            redraw();
            return;
        }
        openLink(mLinkFocus);
    }

    bool JournalScreen::stepBack()
    {
        if (!mInDetails)
            return false;
        MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
        if (mTopicPages.empty())
        {
            leaveDetails();
            return true;
        }
        mLinkFocus = mTopicPages.back().mLinkBefore;
        mTopicPages.pop_back();
        redraw();
        return true;
    }

    void JournalScreen::moveLink(int delta)
    {
        if (mLinks.empty())
            return;
        const int focus = std::clamp(mLinkFocus + delta, 0, static_cast<int>(mLinks.size()) - 1);
        if (focus == mLinkFocus)
            return;
        mLinkFocus = focus;
        redraw();
    }

    void JournalScreen::openLink(int link)
    {
        if (link < 0 || link >= static_cast<int>(mLinks.size()))
            return;
        const int before = mInDetails ? link : 0;
        mInDetails = true;
        mTopicPages.push_back(TopicPage{ mLinks[link].mTopic, before });
        mLinkFocus = 0;
        mLinkHeld.reset();
        MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
        redraw();
    }

    void JournalScreen::leaveDetails()
    {
        mInDetails = false;
        mTopicPages.clear();
        mLinkFocus = 0;
        redraw();
    }

    void JournalScreen::selectFocused()
    {
        // Moving through the list takes the focus back from the details.
        mInDetails = false;
        mTopicPages.clear();
        const std::vector<int> quests = order();
        if (quests.empty())
            return;
        mSelected = mQuests[quests[std::clamp(mList.getFocus(), 0, static_cast<int>(quests.size()) - 1)]].mName;
        redraw();
    }

    void JournalScreen::togglePinned()
    {
        // The quest stays highlighted, so the list follows it into the Pinned group or back out of it.
        for (QuestLog::Quest& quest : mQuests)
            if (quest.mName == mSelected)
            {
                QuestLog::setPinned(quest, !quest.mPinned);
                MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
                redraw();
                return;
            }
    }

    void JournalScreen::changeTab(int delta)
    {
        mInDetails = false;
        mTopicPages.clear();
        mTab = static_cast<Tab>((static_cast<int>(mTab) + delta + sTabCount) % sTabCount);
        mList.setScroll(0);
        redraw();
    }

    void JournalScreen::nextGroup()
    {
        // Moves the selection to the first quest of the next group, or back to the first group after the last one.
        const std::vector<int> quests = order();
        const std::vector<Row> rows = buildRows(quests);
        const std::vector<int> starts = groupStarts(rows);
        if (starts.size() < 2)
            return;
        const auto next = std::upper_bound(starts.begin(), starts.end(), focusIndex(quests));
        const int focus = next != starts.end() ? *next : starts.front();
        mSelected = mQuests[quests[focus]].mName;
        // The group's header goes to the top of the list, so the whole group shows.
        for (int i = 1; i < static_cast<int>(rows.size()); ++i)
            if (rows[i].mEntry == focus)
                mList.setScroll(i - 1);
        MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
        redraw();
    }
}
