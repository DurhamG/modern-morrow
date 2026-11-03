#include "statsscreen.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include <components/esm/attr.hpp>
#include <components/esm3/loadbsgn.hpp>
#include <components/esm3/loadclas.hpp>
#include <components/esm3/loadfact.hpp>
#include <components/esm3/loadgmst.hpp>
#include <components/esm3/loadnpc.hpp>
#include <components/esm3/loadrace.hpp>
#include <components/esm3/loadskil.hpp>
#include <components/esm3/loadspel.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/npcstats.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/player.hpp"

namespace MWGui
{
    namespace
    {
        using namespace ControllerUi;

        constexpr int sEntryHeight = 50;
        constexpr int sHeaderRowHeight = 28;
        constexpr int sTileGap = 12;

        // The colours of the HUD's health, magicka and fatigue bars.
        const MyGUI::Colour sHealthColour(0.78f, 0.24f, 0.12f);
        const MyGUI::Colour sMagickaColour(0.21f, 0.33f, 0.68f);
        const MyGUI::Colour sFatigueColour(0.f, 0.59f, 0.24f);

        std::string tabKey(int tab)
        {
            static const std::array<std::string_view, 3> names = { "Character", "Skills", "Factions" };
            return "#{Interface:StatsTab" + std::string(names[tab]) + "}";
        }

        MWWorld::Ptr player()
        {
            return MWMechanics::getPlayer();
        }

        const MWMechanics::NpcStats& stats()
        {
            const MWWorld::Ptr actor = player();
            return actor.getClass().getNpcStats(actor);
        }

        const MWWorld::ESMStore& store()
        {
            return *MWBase::Environment::get().getESMStore();
        }

        const ESM::NPC& playerRecord()
        {
            return *player().get<ESM::NPC>()->mBase;
        }

        const ESM::Class& playerClass()
        {
            return *store().get<ESM::Class>().find(playerRecord().mClass);
        }

        std::string attributeName(const ESM::RefId& id)
        {
            const ESM::Attribute* attribute = store().get<ESM::Attribute>().search(id);
            return attribute != nullptr ? escaped(attribute->mName) : std::string();
        }

        std::string skillName(const ESM::RefId& id)
        {
            const ESM::Skill* skill = store().get<ESM::Skill>().search(id);
            return skill != nullptr ? escaped(skill->mName) : std::string();
        }

        // Green when a stat is raised above its base value, red when it's lowered.
        MyGUI::Colour valueColour(float modified, float base)
        {
            if (modified > base)
                return sBetterColour;
            if (modified < base)
                return sWorseColour;
            return sBrightColour;
        }

        // The level-up bonus an attribute gets from the skills raised since the last level, such as 3 for "x3".
        int levelUpBonus(const ESM::RefId& attribute)
        {
            const MWMechanics::NpcStats& npcStats = stats();
            const int bonus = npcStats.getLevelupAttributeMultiplier(attribute);
            return std::max(1, std::min(bonus, static_cast<int>(100 - npcStats.getAttribute(attribute).getBase())));
        }

        int skillProgressPercent(const ESM::RefId& skill)
        {
            // This is how the normal stats menu works out the progress, so both show the same number.
            const MWMechanics::NpcStats& npcStats = stats();
            const float requirement = npcStats.getSkillProgressRequirement(skill, playerClass());
            if (requirement <= 0.f)
                return 0;
            return static_cast<int>(npcStats.getSkill(skill).getProgress() / requirement * 100.f + 0.5f);
        }

        int levelUpTotal()
        {
            return store().get<ESM::GameSetting>().find("iLevelUpTotal")->mValue.getInteger();
        }

        std::string spellTypeLabel(const ESM::Spell& spell)
        {
            switch (spell.mData.mType)
            {
                case ESM::Spell::ST_Ability:
                    return "#{Interface:MagicSourceAbility}";
                case ESM::Spell::ST_Power:
                    return "#{Interface:MagicSourcePower}";
                default:
                    return "#{Interface:MagicSourceSpell}";
            }
        }

        // The skills in each group of the Skills tab: the class's major and minor skills, then the rest.
        std::array<std::vector<ESM::RefId>, 3> skillGroups()
        {
            const ESM::Class& cls = playerClass();
            std::array<std::vector<ESM::RefId>, 3> groups;
            groups[0].assign(cls.mData.mMajorSkills.begin(), cls.mData.mMajorSkills.end());
            groups[1].assign(cls.mData.mMinorSkills.begin(), cls.mData.mMinorSkills.end());
            for (const ESM::Skill& skill : store().get<ESM::Skill>())
                if (std::find(groups[0].begin(), groups[0].end(), skill.mId) == groups[0].end()
                    && std::find(groups[1].begin(), groups[1].end(), skill.mId) == groups[1].end())
                    groups[2].push_back(skill.mId);
            for (std::vector<ESM::RefId>& group : groups)
                std::sort(group.begin(), group.end(),
                    [](const ESM::RefId& a, const ESM::RefId& b) { return skillName(a) < skillName(b); });
            return groups;
        }

        std::vector<const ESM::Faction*> playerFactions()
        {
            std::vector<const ESM::Faction*> factions;
            for (const auto& [id, rank] : stats().getFactionRanks())
            {
                const ESM::Faction* faction = store().get<ESM::Faction>().search(id);
                if (faction != nullptr && !(faction->mData.mFlags & ESM::Faction::Hidden))
                    factions.push_back(faction);
            }
            std::sort(factions.begin(), factions.end(),
                [](const ESM::Faction* a, const ESM::Faction* b) { return a->mName < b->mName; });
            return factions;
        }

        int rankCount(const ESM::Faction& faction)
        {
            int count = 0;
            for (const std::string& rank : faction.mRanks)
                count += rank.empty() ? 0 : 1;
            return count;
        }
    }

    StatsScreen::StatsScreen() = default;

    void StatsScreen::clear()
    {
        mTab = Tab::Character;
        mLists = {};
        mTiles.clear();
        mShownState.clear();
    }

    void StatsScreen::onOpen()
    {
        mHeldDirection.reset();
        mShownState = currentState();
        redraw();
    }

    void StatsScreen::onFrame(float duration)
    {
        if (!isVisible())
            return;

        // Stats change while the menu is open, such as when fatigue recovers, so this checks a few times a second.
        mRefreshTimer -= duration;
        if (mRefreshTimer <= 0.f)
        {
            mRefreshTimer = 0.25f;
            std::string state = currentState();
            if (state != mShownState)
            {
                mShownState = std::move(state);
                redraw();
            }
        }

        if (MWBase::Environment::get().getWindowManager()->getActiveControllerWindow() != this)
        {
            mHeldDirection.reset();
            return;
        }
        int dx = 0;
        int dy = 0;
        if (mHeldDirection.update(duration, dx, dy))
            move(dx, dy);
    }

    bool StatsScreen::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
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

    bool StatsScreen::onControllerThumbstickEvent(const SDL_ControllerAxisEvent& arg)
    {
        // The left stick moves the selection, and is read every frame in onFrame.
        return arg.axis == SDL_CONTROLLER_AXIS_LEFTX || arg.axis == SDL_CONTROLLER_AXIS_LEFTY;
    }

    std::string StatsScreen::currentState() const
    {
        const MWWorld::Ptr actor = player();
        if (actor.isEmpty())
            return {};
        const MWMechanics::NpcStats& npcStats = stats();
        std::string state = std::to_string(npcStats.getLevel()) + "/" + std::to_string(npcStats.getLevelProgress());
        for (const auto* dynamic : { &npcStats.getHealth(), &npcStats.getMagicka(), &npcStats.getFatigue() })
            state += "/" + std::to_string(static_cast<int>(dynamic->getCurrent())) + ","
                + std::to_string(static_cast<int>(dynamic->getModified()));
        for (const ESM::Attribute& attribute : store().get<ESM::Attribute>())
            state += "/" + std::to_string(static_cast<int>(npcStats.getAttribute(attribute.mId).getModified()))
                + "x" + std::to_string(levelUpBonus(attribute.mId));
        for (const ESM::Skill& skill : store().get<ESM::Skill>())
            state += "/" + std::to_string(static_cast<int>(npcStats.getSkill(skill.mId).getModified())) + ","
                + std::to_string(skillProgressPercent(skill.mId));
        for (const auto& [faction, rank] : npcStats.getFactionRanks())
            state += "/" + faction.toDebugString() + std::to_string(rank);
        state += "/" + std::to_string(npcStats.getBounty()) + "/" + std::to_string(npcStats.getReputation());
        return state;
    }

    // ---------------------------------------------------------------------------------------------------------------
    // Drawing

    void StatsScreen::redraw()
    {
        clearContent();
        mTiles.clear();
        const MyGUI::IntSize size = mMainWidget->getSize();
        if (player().isEmpty())
            return;

        std::vector<std::string> captions;
        for (int tab = 0; tab < sTabCount; ++tab)
            captions.push_back(tabKey(tab));
        drawTabRow(mContent, size.width, captions, static_cast<int>(mTab),
            "#{sLevel}  " + std::to_string(stats().getLevel()));

        if (mTab == Tab::Character)
        {
            drawCharacter(size.width);
            return;
        }

        const int top = sContentTop;
        const int bottom = size.height - sContentBottomMargin;
        const int panelW = std::max(320, static_cast<int>(size.width * 0.36f));
        const int listW = size.width - 2 * sMargin - panelW - 16;
        drawList(sMargin, top, listW, bottom - top);

        const std::vector<Info> entries = listEntries();
        const int px = size.width - sMargin - panelW;
        if (entries.empty())
        {
            addRect(mContent, px, top, panelW, bottom - top, sPanelColour, 0.9f);
            return;
        }
        ListState& state = mLists[static_cast<int>(mTab)];
        state.mFocus = std::clamp(state.mFocus, 0, static_cast<int>(entries.size()) - 1);
        drawDetails(px, top, panelW, bottom - top, entries[state.mFocus]);

        // The button hint for jumping between the groups of the list, when there is more than one.
        if (groupStarts(buildRows(entries)).size() > 1)
        {
            addGlyph(mContent, px + 16, bottom - 40, 26, SDL_CONTROLLER_BUTTON_Y);
            addText(
                mContent, px + 50, bottom - 44, 200, 34, "#{Interface:MagicNextGroup}", sFontNormal, sBrightColour);
        }
    }

    void StatsScreen::drawCharacter(int width)
    {
        const int top = sContentTop;
        const int bottom = mMainWidget->getHeight() - sContentBottomMargin;
        const int panelW = std::max(320, static_cast<int>(width * 0.36f));
        const int leftW = width - 2 * sMargin - panelW - 16;
        const int x = sMargin;

        addText(mContent, x + 4, top - 4, leftW, 36, escaped(playerRecord().mName), sFontTitle, sBrightColour);
        int y = top + 38;

        const auto row = [&](std::vector<Info> infos, int h) {
            const int count = static_cast<int>(infos.size());
            const int tileW = (leftW - (count - 1) * sTileGap) / count;
            for (int i = 0; i < count; ++i)
                mTiles.emplace_back(infos[i], MyGUI::IntCoord(x + i * (tileW + sTileGap), y, tileW, h));
            y += h + sTileGap;
        };
        using Kind = Info::Kind;
        row({ Info{ Kind::Race, {} }, Info{ Kind::Class, {} }, Info{ Kind::BirthSign, {} } }, 64);
        row({ Info{ Kind::Level, {} } }, 70);
        row({ Info{ Kind::Health, {} }, Info{ Kind::Magicka, {} }, Info{ Kind::Fatigue, {} } }, 70);

        addText(mContent, x + 4, y - 4, leftW, 26, "#{Interface:StatsAttributes}", sFontSmall, sDimColour);
        y += 22;
        std::vector<Info> attributes;
        for (const ESM::Attribute& attribute : store().get<ESM::Attribute>())
            attributes.push_back(Info{ Kind::Attribute, attribute.mId });
        const std::size_t half = attributes.size() / 2;
        row(std::vector<Info>(attributes.begin(), attributes.begin() + half), 84);
        row(std::vector<Info>(attributes.begin() + half, attributes.end()), 84);

        ListState& state = mLists[static_cast<int>(Tab::Character)];
        state.mFocus = std::clamp(state.mFocus, 0, static_cast<int>(mTiles.size()) - 1);
        for (std::size_t i = 0; i < mTiles.size(); ++i)
        {
            const MyGUI::IntCoord& coord = mTiles[i].second;
            drawTile(coord.left, coord.top, coord.width, coord.height, mTiles[i].first,
                static_cast<int>(i) == state.mFocus);
        }

        drawDetails(width - sMargin - panelW, top, panelW, bottom - top, mTiles[state.mFocus].first);
    }

    void StatsScreen::drawBar(int x, int y, int w, int h, float fraction, const MyGUI::Colour& colour)
    {
        addRect(mContent, x, y, w, h, MyGUI::Colour::Black, 0.6f);
        const int filled = static_cast<int>(std::round(w * std::clamp(fraction, 0.f, 1.f)));
        if (filled > 0)
            addRect(mContent, x, y, filled, h, colour, 0.9f);
    }

    void StatsScreen::drawTile(int x, int y, int w, int h, const Info& info, bool focused)
    {
        addRect(mContent, x, y, w, h, sPanelColour, 0.9f);
        if (focused)
        {
            addRect(mContent, x, y, w, h, sAccentColour, 0.16f);
            addRect(mContent, x, y, 3, h, sAccentColour);
        }
        const MyGUI::Colour& nameColour = focused ? sActiveColour : sBrightColour;
        const MWMechanics::NpcStats& npcStats = stats();
        using Kind = Info::Kind;
        switch (info.mKind)
        {
            case Kind::Race:
            case Kind::Class:
            case Kind::BirthSign:
            {
                std::string label = "#{sRace}";
                std::string value;
                if (info.mKind == Kind::Race)
                {
                    if (const ESM::Race* race = store().get<ESM::Race>().search(playerRecord().mRace))
                        value = escaped(race->mName);
                }
                else if (info.mKind == Kind::Class)
                {
                    label = "#{sClass}";
                    value = escaped(playerClass().mName);
                }
                else
                {
                    label = "#{sBirthSign}";
                    const ESM::RefId& signId = MWBase::Environment::get().getWorld()->getPlayer().getBirthSign();
                    if (const ESM::BirthSign* sign = store().get<ESM::BirthSign>().search(signId))
                        value = escaped(sign->mName);
                }
                addText(mContent, x + 14, y + 6, w - 28, 22, label, sFontSmall, sDimColour);
                addText(mContent, x + 14, y + 28, w - 28, 28, value, sFontNormal, nameColour);
                break;
            }
            case Kind::Level:
            {
                addText(mContent, x + 14, y + 6, 120, 22, "#{sLevel}", sFontSmall, sDimColour);
                addText(mContent, x + 14, y + 26, 120, 36, std::to_string(npcStats.getLevel()), sFontTitle, nameColour);
                // Progress toward the next level, in skill increases.
                const int barX = x + 140;
                const int barW = w - 140 - 16;
                const int total = levelUpTotal();
                addText(mContent, barX, y + 10, barW, 24,
                    std::to_string(npcStats.getLevelProgress()) + " / " + std::to_string(total) + "  "
                        + "#{Interface:StatsSkillIncreases}",
                    sFontSmall, sTextColour);
                drawBar(barX, y + 40, barW, 10,
                    total > 0 ? static_cast<float>(npcStats.getLevelProgress()) / total : 0.f, sAccentColour);
                break;
            }
            case Kind::Health:
            case Kind::Magicka:
            case Kind::Fatigue:
            {
                const MWMechanics::DynamicStat<float>& dynamic = info.mKind == Kind::Health ? npcStats.getHealth()
                    : info.mKind == Kind::Magicka                                            ? npcStats.getMagicka()
                                                                                             : npcStats.getFatigue();
                const std::string label = info.mKind == Kind::Health ? "#{sHealth}"
                    : info.mKind == Kind::Magicka                    ? "#{sMagic}"
                                                                     : "#{sFatigue}";
                const MyGUI::Colour& colour = info.mKind == Kind::Health ? sHealthColour
                    : info.mKind == Kind::Magicka                         ? sMagickaColour
                                                                          : sFatigueColour;
                addText(mContent, x + 14, y + 8, w - 28, 22, label, sFontSmall, sDimColour);
                addText(mContent, x + 14, y + 8, w - 28, 22,
                    std::to_string(static_cast<int>(dynamic.getCurrent())) + " / "
                        + std::to_string(static_cast<int>(dynamic.getModified())),
                    sFontNormal, nameColour, MyGUI::Align::Right | MyGUI::Align::VCenter);
                const float fraction = dynamic.getModified() > 0 ? dynamic.getCurrent() / dynamic.getModified() : 0.f;
                drawBar(x + 14, y + 42, w - 28, 12, fraction, colour);
                break;
            }
            case Kind::Attribute:
            {
                const ESM::Attribute* attribute = store().get<ESM::Attribute>().search(info.mId);
                if (attribute == nullptr)
                    break;
                const MWMechanics::AttributeValue& value = npcStats.getAttribute(info.mId);
                addImage(mContent, x + 12, y + 12, 36, std::string(attribute->mIcon.getNormalized().value()));
                addText(mContent, x + 56, y + 8, w - 64, 22, escaped(attribute->mName), sFontSmall, sDimColour);
                addText(mContent, x + 56, y + 32, w - 64, 40, std::to_string(static_cast<int>(value.getModified())),
                    sFontTitle, valueColour(value.getModified(), value.getBase()));
                // The bonus this attribute gets at the next level up, when it is more than the usual one point.
                const int bonus = levelUpBonus(info.mId);
                if (bonus > 1)
                    addText(mContent, x + w - 60, y + 40, 48, 28, "x" + std::to_string(bonus), sFontLarge, sNewColour,
                        MyGUI::Align::Right | MyGUI::Align::VCenter);
                break;
            }
            default:
                break;
        }
    }

    std::vector<StatsScreen::Info> StatsScreen::listEntries() const
    {
        std::vector<Info> entries;
        if (mTab == Tab::Skills)
        {
            for (const std::vector<ESM::RefId>& group : skillGroups())
                for (const ESM::RefId& skill : group)
                    entries.push_back(Info{ Info::Kind::Skill, skill });
        }
        else if (mTab == Tab::Factions)
        {
            entries.push_back(Info{ Info::Kind::Reputation, {} });
            entries.push_back(Info{ Info::Kind::Bounty, {} });
            for (const ESM::Faction* faction : playerFactions())
                entries.push_back(Info{ Info::Kind::Faction, faction->mId });
        }
        return entries;
    }

    std::vector<int> StatsScreen::groupStarts(const std::vector<Row>& rows)
    {
        // The first entry under each group header that has entries.
        std::vector<int> starts;
        for (std::size_t i = 0; i + 1 < rows.size(); ++i)
            if (rows[i].mEntry == -1 && rows[i + 1].mEntry >= 0)
                starts.push_back(rows[i + 1].mEntry);
        return starts;
    }

    std::vector<StatsScreen::Row> StatsScreen::buildRows(const std::vector<Info>& entries) const
    {
        std::vector<Row> rows;
        if (mTab == Tab::Skills)
        {
            static const std::array<std::string_view, 3> headers
                = { "#{sSkillClassMajor}", "#{sSkillClassMinor}", "#{sSkillClassMisc}" };
            int entry = 0;
            const auto groups = skillGroups();
            for (std::size_t group = 0; group < groups.size(); ++group)
            {
                if (groups[group].empty())
                    continue;
                rows.push_back(Row{ std::string(headers[group]), -1 });
                for (std::size_t i = 0; i < groups[group].size(); ++i)
                    rows.push_back(Row{ {}, entry++ });
            }
            return rows;
        }
        rows.push_back(Row{ "#{Interface:StatsStanding}", -1 });
        rows.push_back(Row{ {}, 0 });
        rows.push_back(Row{ {}, 1 });
        rows.push_back(Row{ "#{sFaction}", -1 });
        if (entries.size() <= 2)
            rows.push_back(Row{ "#{Interface:StatsNoFactions}", -2 });
        for (int i = 2; i < static_cast<int>(entries.size()); ++i)
            rows.push_back(Row{ {}, i });
        return rows;
    }

    void StatsScreen::drawList(int x, int y, int w, int h)
    {
        addRect(mContent, x, y, w, h, sPanelColour, 0.9f);
        const std::vector<Info> entries = listEntries();
        const std::vector<Row> rows = buildRows(entries);
        if (entries.empty())
            return;
        ListState& state = mLists[static_cast<int>(mTab)];
        state.mFocus = std::clamp(state.mFocus, 0, static_cast<int>(entries.size()) - 1);

        // Scroll so the focused row is in view.
        const int top = y + 8;
        const int listBottom = y + h - 4;
        int focusRow = 0;
        for (int i = 0; i < static_cast<int>(rows.size()); ++i)
            if (rows[i].mEntry == state.mFocus)
                focusRow = i;
        const auto rowHeight = [&](int i) { return rows[i].mEntry < 0 ? sHeaderRowHeight : sEntryHeight; };
        if (focusRow < state.mScroll)
            state.mScroll = focusRow > 0 && rows[focusRow - 1].mEntry == -1 ? focusRow - 1 : focusRow;
        while (true)
        {
            int bottomOfFocus = top;
            for (int i = state.mScroll; i <= focusRow; ++i)
                bottomOfFocus += rowHeight(i);
            if (bottomOfFocus <= listBottom || state.mScroll >= focusRow)
                break;
            ++state.mScroll;
        }
        // Don't scroll past the end of the list, which would leave empty space below it.
        int lastScroll = static_cast<int>(rows.size());
        for (int i = static_cast<int>(rows.size()) - 1, used = 0; i >= 0; --i)
        {
            used += rowHeight(i);
            if (top + used > listBottom)
                break;
            lastScroll = i;
        }
        state.mScroll = std::min(state.mScroll, lastScroll);

        int rowY = top;
        for (int i = state.mScroll; i < static_cast<int>(rows.size()); ++i)
        {
            const Row& row = rows[i];
            if (rowY + rowHeight(i) > listBottom)
                break;
            if (row.mEntry == -1)
            {
                addText(mContent, x + 12, rowY, w - 24, 24, row.mHeader, sFontSmall, sBrightColour);
                addRect(mContent, x + 12, rowY + 23, w - 24, 1, sAccentColour, 0.25f);
            }
            else if (row.mEntry == -2)
                addText(mContent, x + 16, rowY, w - 32, 24, row.mHeader, sFontNormal, sDimColour);
            else
            {
                drawEntry(x + 4, rowY, w - 8, sEntryHeight - 2, entries[row.mEntry], row.mEntry == state.mFocus);
                rowY += sEntryHeight;
                continue;
            }
            rowY += sHeaderRowHeight;
        }
    }

    void StatsScreen::drawEntry(int x, int y, int w, int h, const Info& info, bool focused)
    {
        if (focused)
        {
            addRect(mContent, x, y, w, h, sAccentColour, 0.16f);
            addRect(mContent, x, y, 3, h, sAccentColour);
        }
        const MyGUI::Colour& nameColour = focused ? sActiveColour : sBrightColour;
        const MWMechanics::NpcStats& npcStats = stats();
        constexpr int valueW = 120;
        switch (info.mKind)
        {
            case Info::Kind::Skill:
            {
                const ESM::Skill* skill = store().get<ESM::Skill>().search(info.mId);
                if (skill == nullptr)
                    break;
                const MWMechanics::SkillValue& value = npcStats.getSkill(info.mId);
                const int iconSize = h - 12;
                addImage(mContent, x + 8, y + 6, iconSize, std::string(skill->mIcon.getNormalized().value()));
                const int textX = x + 8 + iconSize + 12;
                addText(mContent, textX, y + 2, x + w - textX - valueW, 26, escaped(skill->mName), sFontNormal,
                    nameColour);
                addText(mContent, x + w - valueW - 8, y + 2, valueW, 26,
                    std::to_string(static_cast<int>(value.getModified())), sFontNormal,
                    valueColour(value.getModified(), value.getBase()), MyGUI::Align::Right | MyGUI::Align::VCenter);
                addText(mContent, textX, y + 26, x + w - textX - valueW - 140, 20,
                    attributeName(skill->mData.mAttribute), sFontSmall, sDimColour);
                // Progress toward the skill's next increase.
                const int percent = skillProgressPercent(info.mId);
                drawBar(x + w - 8 - 220, y + 33, 160, 6, percent / 100.f, sAccentColour);
                addText(mContent, x + w - 8 - 52, y + 26, 52, 20, std::to_string(percent) + "%", sFontSmall,
                    sDimColour, MyGUI::Align::Right | MyGUI::Align::VCenter);
                break;
            }
            case Info::Kind::Reputation:
            case Info::Kind::Bounty:
            {
                const bool reputation = info.mKind == Info::Kind::Reputation;
                addText(mContent, x + 16, y, w - 32, h, reputation ? "#{sReputation}" : "#{sBounty}", sFontNormal,
                    nameColour);
                const int value = reputation ? npcStats.getReputation() : npcStats.getBounty();
                addText(mContent, x + w - valueW - 8, y, valueW, h, std::to_string(value), sFontNormal,
                    !reputation && value > 0 ? sWorseColour : sBrightColour,
                    MyGUI::Align::Right | MyGUI::Align::VCenter);
                break;
            }
            case Info::Kind::Faction:
            {
                const ESM::Faction* faction = store().get<ESM::Faction>().search(info.mId);
                if (faction == nullptr)
                    break;
                addText(mContent, x + 16, y + 2, w - 32, 26, escaped(faction->mName), sFontNormal, nameColour);
                std::string second;
                MyGUI::Colour secondColour = sDimColour;
                if (npcStats.getExpelled(info.mId))
                {
                    second = "#{sExpelled}";
                    secondColour = sWorseColour;
                }
                else
                {
                    const auto& ranks = npcStats.getFactionRanks();
                    const auto it = ranks.find(info.mId);
                    const int rank = it != ranks.end() ? std::max(0, it->second) : 0;
                    if (rank < static_cast<int>(faction->mRanks.size()))
                        second = escaped(faction->mRanks[rank]);
                    addText(mContent, x + w - valueW - 8, y + 2, valueW, 26,
                        "#{Interface:StatsRank} " + std::to_string(rank + 1) + " / "
                            + std::to_string(rankCount(*faction)),
                        sFontSmall, sTextColour, MyGUI::Align::Right | MyGUI::Align::VCenter);
                }
                addText(mContent, x + 16, y + 26, w - 32, 20, second, sFontSmall, secondColour);
                break;
            }
            default:
                break;
        }
    }

    int StatsScreen::drawParagraph(
        int x, int y, int w, const std::string& text, int fontHeight, const MyGUI::Colour& colour)
    {
        // Paragraphs can be several lines long, so they're cut off at the bottom of the panel by the caller.
        for (const std::string& line : wrapText(mContent, localised(text), w, fontHeight, 12))
        {
            addText(mContent, x, y, w, fontHeight + 6, escaped(line), fontHeight, colour);
            y += fontHeight + 6;
        }
        return y;
    }

    int StatsScreen::drawValueRow(
        int x, int y, int w, const std::string& label, const std::string& value, const MyGUI::Colour& colour)
    {
        addText(mContent, x, y, w, 28, label, sFontNormal, sTextColour);
        addText(mContent, x + w - 160, y, 160, 28, value, sFontNormal, colour,
            MyGUI::Align::Right | MyGUI::Align::VCenter);
        return y + 30;
    }

    void StatsScreen::drawDetails(int x, int y, int w, int h, const Info& info)
    {
        addRect(mContent, x, y, w, h, sPanelColour, 0.9f);
        addRect(mContent, x, y, w, 1, sAccentColour, 0.3f);
        const MWMechanics::NpcStats& npcStats = stats();
        const int innerX = x + 16;
        const int innerW = w - 32;
        int ty = y + 14;

        const auto title = [&](const std::string& text, const std::string& subtitle, const std::string& icon) {
            int textX = innerX;
            if (!icon.empty())
            {
                addImage(mContent, innerX, y + 16, 64, icon);
                textX = innerX + 76;
            }
            addText(mContent, textX, y + 14, x + w - 16 - textX, 34, text, sFontLarge, sBrightColour);
            addText(mContent, textX, y + 46, x + w - 16 - textX, 26, subtitle, sFontSmall, sDimColour);
            ty = y + (icon.empty() ? 84 : 96);
        };
        const auto heading = [&](const std::string& text) {
            ty += 6;
            addText(mContent, innerX, ty, innerW, 24, text, sFontSmall, sDimColour);
            ty += 26;
        };
        const auto spellList = [&](const ESM::SpellList& spells) {
            for (const ESM::RefId& id : spells.mList)
                if (const ESM::Spell* spell = store().get<ESM::Spell>().search(id))
                    ty = drawParagraph(innerX, ty, innerW, spell->mName + "  (" + spellTypeLabel(*spell) + ")",
                        sFontNormal, sNewColour);
        };

        using Kind = Info::Kind;
        switch (info.mKind)
        {
            case Kind::Race:
            {
                const ESM::Race* race = store().get<ESM::Race>().search(playerRecord().mRace);
                if (race == nullptr)
                    break;
                title(escaped(race->mName), "#{sRace}", {});
                ty = drawParagraph(innerX, ty, innerW, race->mDescription, sFontNormal, sTextColour);
                if (!race->mPowers.mList.empty())
                {
                    heading("#{Interface:StatsAbilitiesAndPowers}");
                    spellList(race->mPowers);
                }
                break;
            }
            case Kind::Class:
            {
                const ESM::Class& cls = playerClass();
                title(escaped(cls.mName), "#{sClass}", {});
                ty = drawValueRow(innerX, ty, innerW, "#{sSpecialization}",
                    "#{" + std::string(ESM::Class::sGmstSpecializationIds[cls.mData.mSpecialization]) + "}",
                    sBrightColour);
                ty = drawValueRow(innerX, ty, innerW, "#{Interface:StatsFavouredAttributes}",
                    attributeName(cls.mData.mAttribute[0]) + ", " + attributeName(cls.mData.mAttribute[1]),
                    sBrightColour);
                std::string major;
                std::string minor;
                for (const ESM::RefId& skill : cls.mData.mMajorSkills)
                    major += (major.empty() ? "" : ", ") + skillName(skill);
                for (const ESM::RefId& skill : cls.mData.mMinorSkills)
                    minor += (minor.empty() ? "" : ", ") + skillName(skill);
                heading("#{sSkillClassMajor}");
                ty = drawParagraph(innerX, ty, innerW, major, sFontNormal, sBrightColour);
                heading("#{sSkillClassMinor}");
                ty = drawParagraph(innerX, ty, innerW, minor, sFontNormal, sBrightColour);
                ty += 8;
                ty = drawParagraph(innerX, ty, innerW, cls.mDescription, sFontNormal, sTextColour);
                break;
            }
            case Kind::BirthSign:
            {
                const ESM::RefId& signId = MWBase::Environment::get().getWorld()->getPlayer().getBirthSign();
                const ESM::BirthSign* sign = store().get<ESM::BirthSign>().search(signId);
                if (sign == nullptr)
                    break;
                title(escaped(sign->mName), "#{sBirthSign}", {});
                ty = drawParagraph(innerX, ty, innerW, sign->mDescription, sFontNormal, sTextColour);
                if (!sign->mPowers.mList.empty())
                {
                    heading("#{Interface:StatsAbilitiesAndPowers}");
                    spellList(sign->mPowers);
                }
                break;
            }
            case Kind::Level:
            {
                title("#{sLevel} " + std::to_string(npcStats.getLevel()), escaped(playerRecord().mName), {});
                ty = drawValueRow(innerX, ty, innerW, "#{Interface:StatsLevelProgress}",
                    std::to_string(npcStats.getLevelProgress()) + " / " + std::to_string(levelUpTotal()),
                    sBrightColour);
                heading("#{Interface:StatsNextLevelBonus}");
                for (const ESM::Attribute& attribute : store().get<ESM::Attribute>())
                {
                    const int bonus = levelUpBonus(attribute.mId);
                    ty = drawValueRow(innerX, ty, innerW, escaped(attribute.mName), "x" + std::to_string(bonus),
                        bonus > 1 ? sNewColour : sDimColour);
                }
                ty += 8;
                ty = drawParagraph(innerX, ty, innerW, "#{Interface:StatsLevelHelp}", sFontSmall, sTextColour);
                break;
            }
            case Kind::Health:
            case Kind::Magicka:
            case Kind::Fatigue:
            {
                const MWMechanics::DynamicStat<float>& dynamic = info.mKind == Kind::Health ? npcStats.getHealth()
                    : info.mKind == Kind::Magicka                                            ? npcStats.getMagicka()
                                                                                             : npcStats.getFatigue();
                const std::string name = info.mKind == Kind::Health ? "#{sHealth}"
                    : info.mKind == Kind::Magicka                   ? "#{sMagic}"
                                                                    : "#{sFatigue}";
                const std::string description = info.mKind == Kind::Health ? "#{sHealthDesc}"
                    : info.mKind == Kind::Magicka                          ? "#{sMagDesc}"
                                                                           : "#{sFatDesc}";
                title(name, {}, {});
                ty = drawValueRow(innerX, ty, innerW, "#{Interface:StatsValue}",
                    std::to_string(static_cast<int>(dynamic.getCurrent())) + " / "
                        + std::to_string(static_cast<int>(dynamic.getModified())),
                    sBrightColour);
                ty += 8;
                ty = drawParagraph(innerX, ty, innerW, description, sFontNormal, sTextColour);
                break;
            }
            case Kind::Attribute:
            {
                const ESM::Attribute* attribute = store().get<ESM::Attribute>().search(info.mId);
                if (attribute == nullptr)
                    break;
                const MWMechanics::AttributeValue& value = npcStats.getAttribute(info.mId);
                title(escaped(attribute->mName), "#{Interface:StatsAttribute}",
                    std::string(attribute->mIcon.getNormalized().value()));
                ty = drawValueRow(innerX, ty, innerW, "#{Interface:StatsValue}",
                    std::to_string(static_cast<int>(value.getModified())),
                    valueColour(value.getModified(), value.getBase()));
                if (value.getModified() != value.getBase())
                    ty = drawValueRow(innerX, ty, innerW, "#{Interface:StatsBase}",
                        std::to_string(static_cast<int>(value.getBase())), sBrightColour);
                const int bonus = levelUpBonus(info.mId);
                ty = drawValueRow(innerX, ty, innerW, "#{Interface:StatsNextLevelBonus}", "x" + std::to_string(bonus),
                    bonus > 1 ? sNewColour : sBrightColour);
                ty = drawValueRow(innerX, ty, innerW, "#{Interface:StatsAttributeIncreases}",
                    std::to_string(npcStats.getSkillIncreasesForAttribute(info.mId)), sBrightColour);
                ty += 4;
                ty = drawParagraph(innerX, ty, innerW, attribute->mDescription, sFontNormal, sTextColour);
                heading("#{Interface:StatsGoverns}");
                for (const ESM::Skill& skill : store().get<ESM::Skill>())
                {
                    if (skill.mData.mAttribute != info.mId || ty + 30 > y + h - 8)
                        continue;
                    ty = drawValueRow(innerX, ty, innerW, escaped(skill.mName),
                        std::to_string(static_cast<int>(npcStats.getSkill(skill.mId).getModified())), sBrightColour);
                }
                break;
            }
            case Kind::Skill:
            {
                const ESM::Skill* skill = store().get<ESM::Skill>().search(info.mId);
                if (skill == nullptr)
                    break;
                const MWMechanics::SkillValue& value = npcStats.getSkill(info.mId);
                title(escaped(skill->mName), "#{sGoverningAttribute}: " + attributeName(skill->mData.mAttribute),
                    std::string(skill->mIcon.getNormalized().value()));
                ty = drawValueRow(innerX, ty, innerW, "#{Interface:StatsValue}",
                    std::to_string(static_cast<int>(value.getModified())),
                    valueColour(value.getModified(), value.getBase()));
                if (value.getModified() != value.getBase())
                    ty = drawValueRow(innerX, ty, innerW, "#{Interface:StatsBase}",
                        std::to_string(static_cast<int>(value.getBase())), sBrightColour);
                const int percent = skillProgressPercent(info.mId);
                ty = drawValueRow(
                    innerX, ty, innerW, "#{Interface:StatsProgress}", std::to_string(percent) + "%", sBrightColour);
                drawBar(innerX, ty, innerW, 8, percent / 100.f, sAccentColour);
                ty += 18;
                ty = drawValueRow(innerX, ty, innerW, "#{sSpecialization}",
                    "#{" + std::string(ESM::Class::sGmstSpecializationIds[skill->mData.mSpecialization]) + "}",
                    sBrightColour);
                const ESM::Class& cls = playerClass();
                const bool counts = std::find(cls.mData.mMajorSkills.begin(), cls.mData.mMajorSkills.end(), info.mId)
                        != cls.mData.mMajorSkills.end()
                    || std::find(cls.mData.mMinorSkills.begin(), cls.mData.mMinorSkills.end(), info.mId)
                        != cls.mData.mMinorSkills.end();
                ty += 4;
                ty = drawParagraph(innerX, ty, innerW,
                    std::string(counts ? "#{Interface:StatsCountsToLevel}" : "#{Interface:StatsNotCountsToLevel}")
                        + " #{Interface:StatsAddsToBonus} " + attributeName(skill->mData.mAttribute) + ".",
                    sFontNormal, sNewColour);
                ty += 4;
                ty = drawParagraph(innerX, ty, innerW, skill->mDescription, sFontNormal, sTextColour);
                break;
            }
            case Kind::Reputation:
            case Kind::Bounty:
            {
                const bool reputation = info.mKind == Kind::Reputation;
                title(reputation ? "#{sReputation}" : "#{sBounty}", {}, {});
                ty = drawValueRow(innerX, ty, innerW, "#{Interface:StatsValue}",
                    std::to_string(reputation ? npcStats.getReputation() : npcStats.getBounty()), sBrightColour);
                ty += 8;
                ty = drawParagraph(innerX, ty, innerW, reputation ? "#{sSkillsMenuReputationHelp}" : "#{sCrimeHelp}",
                    sFontNormal, sTextColour);
                break;
            }
            case Kind::Faction:
            {
                const ESM::Faction* faction = store().get<ESM::Faction>().search(info.mId);
                if (faction == nullptr)
                    break;
                const auto& ranks = npcStats.getFactionRanks();
                const auto it = ranks.find(info.mId);
                const int rank = it != ranks.end() ? std::max(0, it->second) : 0;
                const bool expelled = npcStats.getExpelled(info.mId);
                title(escaped(faction->mName),
                    expelled ? std::string("#{sExpelled}")
                             : (rank < static_cast<int>(faction->mRanks.size()) ? escaped(faction->mRanks[rank])
                                                                                : std::string()),
                    {});
                ty = drawValueRow(innerX, ty, innerW, "#{sReputation}",
                    std::to_string(npcStats.getFactionReputation(info.mId)), sBrightColour);
                const int next = rank + 1;
                if (expelled)
                    break;
                if (next >= static_cast<int>(faction->mRanks.size()) || faction->mRanks[next].empty())
                {
                    ty += 8;
                    ty = drawParagraph(innerX, ty, innerW, "#{Interface:StatsHighestRank}", sFontNormal, sTextColour);
                    break;
                }

                // What the next rank needs, green where the player meets it, and red where they don't.
                heading("#{sNextRank} " + escaped(faction->mRanks[next]));
                const ESM::RankData& needs = faction->mData.mRankData[next];
                const auto needRow = [&](const std::string& label, int have, int need) {
                    ty = drawValueRow(innerX, ty, innerW, label, std::to_string(have) + " / " + std::to_string(need),
                        have >= need ? sBetterColour : sWorseColour);
                };
                needRow(attributeName(faction->mData.mAttribute[0]),
                    static_cast<int>(npcStats.getAttribute(faction->mData.mAttribute[0]).getBase()),
                    needs.mAttribute1);
                needRow(attributeName(faction->mData.mAttribute[1]),
                    static_cast<int>(npcStats.getAttribute(faction->mData.mAttribute[1]).getBase()),
                    needs.mAttribute2);
                needRow("#{sReputation}", npcStats.getFactionReputation(info.mId), needs.mFactReputation);
                if (needs.mPrimarySkill > 0)
                {
                    std::string skills = "#{Interface:StatsOneFavouredSkill} " + std::to_string(needs.mPrimarySkill);
                    if (needs.mFavouredSkill > 0)
                        skills += ", #{Interface:StatsTwoFavouredSkills} " + std::to_string(needs.mFavouredSkill);
                    ty += 4;
                    ty = drawParagraph(innerX, ty, innerW, skills, sFontNormal,
                        npcStats.hasSkillsForRank(info.mId, next) ? sBetterColour : sWorseColour);
                }

                heading("#{sFavoriteSkills}");
                for (const ESM::RefId& skill : faction->mData.mSkills)
                {
                    if (skill.empty() || ty + 30 > y + h - 56)
                        continue;
                    ty = drawValueRow(innerX, ty, innerW, skillName(skill),
                        std::to_string(static_cast<int>(npcStats.getSkill(skill).getBase())), sBrightColour);
                }
                break;
            }
        }
    }

    // ---------------------------------------------------------------------------------------------------------------
    // Input

    void StatsScreen::move(int dx, int dy)
    {
        if (mTab == Tab::Character)
            moveOnTiles(dx, dy);
        else if (dy != 0)
        {
            const int count = static_cast<int>(listEntries().size());
            ListState& state = mLists[static_cast<int>(mTab)];
            if (count > 0)
                state.mFocus = std::clamp(state.mFocus + dy, 0, count - 1);
        }
        redraw();
    }

    void StatsScreen::moveOnTiles(int dx, int dy)
    {
        // Moves to the nearest tile in the pressed direction, preferring tiles in line with the current one.
        ListState& state = mLists[static_cast<int>(Tab::Character)];
        if (mTiles.empty())
            return;
        const MyGUI::IntCoord& from = mTiles[std::clamp(state.mFocus, 0, static_cast<int>(mTiles.size()) - 1)].second;
        const float fromX = from.left + from.width / 2.f;
        const float fromY = from.top + from.height / 2.f;
        float best = std::numeric_limits<float>::max();
        for (std::size_t i = 0; i < mTiles.size(); ++i)
        {
            const MyGUI::IntCoord& to = mTiles[i].second;
            const float offsetX = to.left + to.width / 2.f - fromX;
            const float offsetY = to.top + to.height / 2.f - fromY;
            const float along = dx != 0 ? offsetX * dx : offsetY * dy;
            // How far the tile is to the side, as the gap between the two tiles' edges. Tiles that overlap the
            // current one to the side count as in line, such as the wide level tile below the race tile.
            const float across = dx != 0
                ? static_cast<float>(std::max({ 0, to.top - from.bottom(), from.top - to.bottom() }))
                : static_cast<float>(std::max({ 0, to.left - from.right(), from.left - to.right() }));
            if (along <= 1.f || (dx != 0 && across > 0.f))
                continue;
            const float score = along + 2.f * across;
            if (score < best)
            {
                best = score;
                state.mFocus = static_cast<int>(i);
            }
        }
    }

    void StatsScreen::changeTab(int delta)
    {
        mTab = static_cast<Tab>((static_cast<int>(mTab) + delta + sTabCount) % sTabCount);
        redraw();
    }

    void StatsScreen::nextGroup()
    {
        // Moves the selection to the first entry of the next group, or back to the first group after the last one.
        if (mTab == Tab::Character)
            return;
        const std::vector<Row> rows = buildRows(listEntries());
        const std::vector<int> starts = groupStarts(rows);
        if (starts.size() < 2)
            return;
        ListState& state = mLists[static_cast<int>(mTab)];
        const auto next = std::upper_bound(starts.begin(), starts.end(), state.mFocus);
        state.mFocus = next != starts.end() ? *next : starts.front();
        // The group's header goes to the top of the list, so the whole group shows.
        for (int i = 1; i < static_cast<int>(rows.size()); ++i)
            if (rows[i].mEntry == state.mFocus)
                state.mScroll = i - 1;
        MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
        redraw();
    }
}
