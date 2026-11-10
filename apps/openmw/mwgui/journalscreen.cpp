#include "journalscreen.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

#include <MyGUI_ImageBox.h>

#include <components/misc/strings/algorithm.hpp>
#include <components/misc/strings/lower.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwdialogue/journalentry.hpp"

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
        mEntriesQuest.clear();
    }

    void JournalScreen::onOpen()
    {
        mList.resetHeld();
        // The journal can't change while the menus are open, so the quests are read once.
        mQuests = QuestLog::collect();
        redraw();
    }

    void JournalScreen::onFrame(float duration)
    {
        if (!isVisible() || MWBase::Environment::get().getWindowManager()->getActiveControllerWindow() != this)
        {
            mList.resetHeld();
            return;
        }
        // Every tab lists all the quests, in its own order.
        if (mList.updateHeld(duration, static_cast<int>(mQuests.size())))
            selectFocused();
        scrollEntries(
            MWBase::Environment::get().getInputManager()->getControllerAxisValue(SDL_CONTROLLER_AXIS_RIGHTY), duration);
    }

    bool JournalScreen::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
    {
        MWBase::WindowManager* winMgr = MWBase::Environment::get().getWindowManager();
        switch (arg.button)
        {
            case SDL_CONTROLLER_BUTTON_B:
                winMgr->exitCurrentGuiMode();
                break;
            case SDL_CONTROLLER_BUTTON_Y:
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
        // mQuests is already in order of the most recent activity. Quests in progress come first, and on the Places
        // and Factions tabs they are grouped by name, with quests that have no place or faction after the rest.
        std::vector<int> result(mQuests.size());
        for (std::size_t i = 0; i < result.size(); ++i)
            result[i] = static_cast<int>(i);
        const auto groupKey = [&](const QuestLog::Quest& quest) -> std::pair<int, std::string> {
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
        ty += 8;
        addRect(mContent, innerX, ty, innerW, 1, sAccentColour, 0.25f);
        ty += 10;

        // The entries, newest first, on a page the right stick scrolls when they don't fit.
        constexpr int barWidth = 4;
        const int areaBottom = y + h - 56;
        MyGUI::Widget* area = mContent->createWidget<MyGUI::Widget>(
            "", MyGUI::IntCoord(innerX, ty, innerW, areaBottom - ty), MyGUI::Align::Default);
        area->setNeedMouseFocus(false);
        // The wheel scrolls the entries, rather than moving through the quests.
        addWheelTarget(mContent, innerX, ty, innerW, areaBottom - ty, [this](int steps) {
            mEntriesScroll = std::clamp(
                mEntriesScroll - steps * sWheelScrollStep, 0.f, static_cast<float>(std::max(0, mEntriesScrollMax)));
            placeEntries();
        });
        constexpr int unlimitedHeight = 1 << 20;
        mEntriesPaper = area->createWidget<MyGUI::Widget>(
            "", MyGUI::IntCoord(0, 0, innerW - 12, unlimitedHeight), MyGUI::Align::Default);
        mEntriesPaper->setNeedMouseFocus(false);
        int py = 0;
        for (auto it = quest.mEntries.rbegin(); it != quest.mEntries.rend(); ++it)
        {
            const bool latest = it == quest.mEntries.rbegin();
            addText(mEntriesPaper, 0, py, innerW - 12, 22, escaped(QuestLog::timestamp(**it)), sFontSmall,
                latest ? sNewColour : sDimColour);
            py += 24;
            for (const std::string& line :
                wrapText(mEntriesPaper, (*it)->getText(), innerW - 12, sFontNormal, std::size_t(-1)))
            {
                addText(mEntriesPaper, 0, py, innerW - 12, sFontNormal + 6, escaped(line), sFontNormal,
                    latest ? sBrightColour : sTextColour);
                py += sFontNormal + 6;
            }
            py += 14;
        }
        mEntriesPaper->setSize(innerW - 12, py);

        if (quest.mName != mEntriesQuest)
        {
            mEntriesQuest = quest.mName;
            mEntriesScroll = 0.f;
        }
        mEntriesScrollMax = std::max(0, py - area->getHeight());
        mEntriesScroll = std::clamp(mEntriesScroll, 0.f, static_cast<float>(mEntriesScrollMax));

        // The button hints along the bottom of the panel.
        int hx = innerX;
        if (groups)
        {
            addGlyph(mContent, hx, y + h - 40, 26, SDL_CONTROLLER_BUTTON_Y);
            MyGUI::TextBox* label = addText(
                mContent, hx + 34, y + h - 44, 200, 34, "#{Interface:MagicNextGroup}", sFontNormal, sBrightColour);
            // Clicking the hint presses its button.
            addPointerTarget(mContent, hx, y + h - 44, 34 + label->getTextSize().width, 34, nullptr,
                [] { pressButton(SDL_CONTROLLER_BUTTON_Y); });
            hx += 34 + label->getTextSize().width + 24;
        }
        if (mEntriesScrollMax > 0)
        {
            mEntriesTrack = MyGUI::IntCoord(innerX + innerW - barWidth, ty, barWidth, areaBottom - ty);
            addRect(mContent, mEntriesTrack.left, mEntriesTrack.top, mEntriesTrack.width, mEntriesTrack.height,
                sAccentColour, 0.12f);
            const float visible = static_cast<float>(area->getHeight()) / py;
            mEntriesScrollThumb = addRect(mContent, mEntriesTrack.left, mEntriesTrack.top, mEntriesTrack.width,
                std::max(16, static_cast<int>(mEntriesTrack.height * visible)), sAccentColour, 0.6f);

            addGlyph(mContent, hx, y + h - 40, 26, SDL_CONTROLLER_BUTTON_RIGHTSTICK);
            addText(mContent, hx + 34, y + h - 44, 160, 34, "#{Interface:InventoryScroll}", sFontNormal, sBrightColour);
        }
        placeEntries();
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

    void JournalScreen::selectFocused()
    {
        const std::vector<int> quests = order();
        if (quests.empty())
            return;
        mSelected = mQuests[quests[std::clamp(mList.getFocus(), 0, static_cast<int>(quests.size()) - 1)]].mName;
        redraw();
    }

    void JournalScreen::changeTab(int delta)
    {
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
