#include "magicscreen.hpp"

#include <algorithm>
#include <cmath>
#include <map>

#include <components/esm3/loadbsgn.hpp>
#include <components/esm3/loadench.hpp>
#include <components/esm3/loadmgef.hpp>
#include <components/esm3/loadrace.hpp>
#include <components/esm3/loadskil.hpp>
#include <components/esm3/loadspel.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/misc/strings/format.hpp>
#include <components/resource/resourcesystem.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/creaturestats.hpp"
#include "../mwmechanics/spells.hpp"
#include "../mwmechanics/spellutil.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/inventorystore.hpp"
#include "../mwworld/player.hpp"

#include "confirmationdialog.hpp"
#include "quickkeyassigner.hpp"
#include "spellmodel.hpp"
#include "tooltips.hpp"
#include "widgets.hpp"

namespace MWGui
{
    namespace
    {
        using namespace ControllerUi;

        constexpr int sEntryHeight = 58;
        constexpr int sEffectHeight = 50;
        constexpr int sHeaderRowHeight = 28;
        constexpr int sReadyCardHeight = 150;
        constexpr std::size_t sRecentShown = 40;

        std::string tabKey(int tab)
        {
            static const std::array<std::string_view, 6> names
                = { "Recent", "Active", "Spells", "Powers", "Items", "Scrolls" };
            return "#{Interface:MagicTab" + std::string(names[tab]) + "}";
        }

        MWWorld::Ptr player()
        {
            return MWMechanics::getPlayer();
        }

        const MWWorld::ESMStore& store()
        {
            return *MWBase::Environment::get().getESMStore();
        }

        std::string bigIcon(const ESM::MagicEffect& effect)
        {
            return std::string(Misc::ResourceHelpers::correctBigIconPath(
                effect.mIcon.getNormalized(), *MWBase::Environment::get().getResourceSystem()->getVFS())
                    .value());
        }

        std::vector<std::string> describeEffects(const ESM::EffectList* effects)
        {
            std::vector<std::string> lines;
            if (effects == nullptr)
                return lines;
            for (const Widgets::SpellEffectParams& params : Widgets::MWEffectList::effectListFromESM(effects))
                lines.push_back(localised(Widgets::describeSpellEffect(params)));
            return lines;
        }

        // Whether the player got the spell from their race or birthsign, which means it can't be deleted.
        bool isInherent(const ESM::Spell& spell)
        {
            const MWWorld::Ptr actor = player();
            const ESM::Race* race = store().get<ESM::Race>().find(actor.get<ESM::NPC>()->mBase->mRace);
            if (race->mPowers.exists(spell.mId))
                return true;
            const ESM::RefId& signId = MWBase::Environment::get().getWorld()->getPlayer().getBirthSign();
            if (signId.empty())
                return false;
            return store().get<ESM::BirthSign>().find(signId)->mPowers.exists(spell.mId);
        }

        std::string remainingTime(float seconds)
        {
            // The duration text starts with ": " for a prefix, which isn't wanted here.
            std::string text = ToolTips::getDurationString(seconds, "");
            if (text.starts_with(": "))
                text.erase(0, 2);
            return text + "#{Interface:MagicLeft}";
        }
    }

    MagicScreen::MagicScreen() = default;

    void MagicScreen::clear()
    {
        mCastables.clear();
        mEffects.clear();
        mShownState.clear();
        mTab = Tab::Active;
        mLists = {};
        mEffectsFocused = false;
        mEffectList = {};
        mRecentList = {};
        mQuickKeys.cancel();
    }

    void MagicScreen::onOpen()
    {
        mHeldDirection.reset();
        mQuickKeys.cancel();
        update();
        mShownState = currentState();
        redraw();
    }

    void MagicScreen::onClose()
    {
        mQuickKeys.cancel();
    }

    void MagicScreen::onFrame(float duration)
    {
        if (!isVisible())
            return;

        // Spells, charges and effects change while the screen is open, such as when an effect runs out. Checking a
        // few times a second keeps the screen up to date.
        mRefreshTimer -= duration;
        if (mRefreshTimer <= 0.f)
        {
            mRefreshTimer = 0.2f;
            update();
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

        // While the player chooses a quick key, the d-pad picks the slot instead of moving the selection.
        if (mQuickKeys.hasDpad())
        {
            mHeldDirection.reset();
            if (mQuickKeys.update(duration))
                redraw();
            return;
        }

        int dx = 0;
        int dy = 0;
        if (mHeldDirection.update(duration, dx, dy))
            move(dx, dy);
    }

    bool MagicScreen::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
    {
        MWBase::WindowManager* winMgr = MWBase::Environment::get().getWindowManager();
        if (mQuickKeys.hasDpad())
        {
            if (mQuickKeys.onButton(arg.button))
                redraw();
            return true;
        }
        switch (arg.button)
        {
            case SDL_CONTROLLER_BUTTON_A:
                if (const int item = focusedCastable(); item >= 0)
                    ready(item);
                break;
            case SDL_CONTROLLER_BUTTON_LEFTSTICK:
                startQuickKey();
                break;
            case SDL_CONTROLLER_BUTTON_B:
                winMgr->exitCurrentGuiMode();
                break;
            case SDL_CONTROLLER_BUTTON_X:
                if (const int item = focusedCastable(); item >= 0)
                    askDelete(item);
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

    bool MagicScreen::onControllerThumbstickEvent(const SDL_ControllerAxisEvent& arg)
    {
        // The left stick moves the selection, and is read every frame in onFrame.
        return arg.axis == SDL_CONTROLLER_AXIS_LEFTX || arg.axis == SDL_CONTROLLER_AXIS_LEFTY;
    }

    // ---------------------------------------------------------------------------------------------------------------
    // What the screen shows

    void MagicScreen::update()
    {
        mCastables.clear();
        mEffects.clear();
        const MWWorld::Ptr actor = player();
        if (actor.isEmpty())
            return;
        MWMechanics::CreatureStats& stats = actor.getClass().getCreatureStats(actor);
        const MWWorld::ESMStore& esmStore = store();

        // The spell model decides what the player can ready, as the normal magic menu does.
        SpellModel model(actor);
        model.update();
        for (std::size_t i = 0; i < model.getItemCount(); ++i)
        {
            const Spell entry = model.getItem(static_cast<SpellModel::ModelIndex>(i));
            Castable castable;
            castable.mId = entry.mId;
            castable.mName = entry.mName;
            castable.mReady = entry.mSelected;
            if (entry.mType == Spell::Type_EnchantedItem)
            {
                const ESM::Enchantment* enchantment
                    = esmStore.get<ESM::Enchantment>().search(entry.mItem.getClass().getEnchantment(entry.mItem));
                if (enchantment == nullptr)
                    continue;
                castable.mKind = enchantment->mData.mType == ESM::Enchantment::CastOnce ? Castable::Kind::Scroll
                                                                                         : Castable::Kind::Item;
                castable.mItem = entry.mItem;
                castable.mEffects = &enchantment->mEffects;
                castable.mCount = entry.mCount;
                castable.mCost = MWMechanics::getEffectiveEnchantmentCastCost(*enchantment, actor);
                castable.mMaxCharge = MWMechanics::getEnchantmentCharge(*enchantment);
                const float charge = entry.mItem.getCellRef().getEnchantmentCharge();
                castable.mCharge = charge < 0 ? castable.mMaxCharge : static_cast<int>(charge);
            }
            else
            {
                const ESM::Spell* spell = esmStore.get<ESM::Spell>().find(entry.mId);
                castable.mKind
                    = entry.mType == Spell::Type_Power ? Castable::Kind::Power : Castable::Kind::Spell;
                castable.mEffects = &spell->mEffects;
                if (!spell->mEffects.mList.empty())
                    castable.mIcon = bigIcon(
                        *esmStore.get<ESM::MagicEffect>().find(spell->mEffects.mList.front().mData.mEffectID));
                castable.mSchool = MWMechanics::getSpellSchool(spell, actor);
                castable.mCost = MWMechanics::calcSpellCost(*spell);
                castable.mChance = static_cast<int>(MWMechanics::getSpellSuccessChance(spell, actor));
                castable.mDeletable = castable.mKind == Castable::Kind::Spell && !isInherent(*spell);
                castable.mUsedToday
                    = castable.mKind == Castable::Kind::Power && !stats.getSpells().canUsePower(spell);
            }
            mCastables.push_back(std::move(castable));
        }

        for (const auto& params : stats.getActiveSpells())
        {
            // Where the effect came from, such as "Shield, Spell" or "Ring of Frost, Worn".
            std::string source = escaped(params.getDisplayName());
            if (const ESM::Spell* spell = params.getSpell())
            {
                static const std::map<int, std::string_view> kinds = {
                    { ESM::Spell::ST_Spell, "Spell" },
                    { ESM::Spell::ST_Ability, "Ability" },
                    { ESM::Spell::ST_Blight, "Blight" },
                    { ESM::Spell::ST_Disease, "Disease" },
                    { ESM::Spell::ST_Curse, "Curse" },
                    { ESM::Spell::ST_Power, "Power" },
                };
                if (const auto it = kinds.find(spell->mData.mType); it != kinds.end())
                    source += ", #{Interface:MagicSource" + std::string(it->second) + "}";
            }
            else if (params.hasFlag(ESM::ActiveSpells::Flag_Equipment))
                source += ", #{Interface:MagicSourceWorn}";

            for (const ESM::ActiveEffect& effect : params.getEffects())
            {
                if (!(effect.mFlags & ESM::ActiveEffect::Flag_Applied))
                    continue;
                if (effect.mDuration != -1.f && effect.mTimeLeft <= 0.f)
                    continue;
                const ESM::MagicEffect* magicEffect = esmStore.get<ESM::MagicEffect>().find(effect.mEffectId);
                Widgets::SpellEffectParams description;
                description.mEffectID = effect.mEffectId;
                if (magicEffect->mData.mFlags & ESM::MagicEffect::TargetSkill)
                    description.mSkill = effect.getSkillOrAttribute();
                if (magicEffect->mData.mFlags & ESM::MagicEffect::TargetAttribute)
                    description.mAttribute = effect.getSkillOrAttribute();
                description.mMagnMin = description.mMagnMax = static_cast<int>(effect.mMagnitude);
                description.mIsConstant = true;

                ActiveEffect active;
                active.mIcon = bigIcon(*magicEffect);
                active.mText = localised(Widgets::describeSpellEffect(description));
                active.mSource = source;
                active.mTimeLeft = effect.mDuration == -1.f ? -1.f : effect.mTimeLeft;
                mEffects.push_back(std::move(active));
            }
        }
        // Effects that run out come first, soonest first. Lasting effects, such as abilities, follow.
        std::stable_sort(mEffects.begin(), mEffects.end(), [](const ActiveEffect& a, const ActiveEffect& b) {
            const float left = a.mTimeLeft < 0 ? 1e9f : a.mTimeLeft;
            const float right = b.mTimeLeft < 0 ? 1e9f : b.mTimeLeft;
            return left < right;
        });
    }

    std::string MagicScreen::currentState() const
    {
        std::string state;
        for (const Castable& castable : mCastables)
            state += castable.mId.toDebugString() + (castable.mReady ? "*" : "") + std::to_string(castable.mCharge)
                + "/" + std::to_string(castable.mCount) + "/" + std::to_string(castable.mChance)
                + (castable.mUsedToday ? "u" : "") + ";";
        for (const ActiveEffect& effect : mEffects)
            state += effect.mText + effect.mSource + std::to_string(static_cast<int>(effect.mTimeLeft)) + ";";
        const MWWorld::Ptr actor = player();
        if (!actor.isEmpty())
            state += std::to_string(
                static_cast<int>(actor.getClass().getCreatureStats(actor).getMagicka().getCurrent()));
        for (const ESM::RefId& recent : MWBase::Environment::get().getWorld()->getPlayer().getRecentSpells())
            state += recent.toDebugString();
        return state;
    }

    std::vector<int> MagicScreen::recentCastables(bool skipReady) const
    {
        std::vector<int> result;
        for (const ESM::RefId& id : MWBase::Environment::get().getWorld()->getPlayer().getRecentSpells())
        {
            for (std::size_t i = 0; i < mCastables.size(); ++i)
            {
                if (mCastables[i].mId != id || (skipReady && mCastables[i].mReady))
                    continue;
                if (std::find(result.begin(), result.end(), static_cast<int>(i)) == result.end())
                    result.push_back(static_cast<int>(i));
                break;
            }
            if (result.size() >= sRecentShown)
                break;
        }
        return result;
    }

    int MagicScreen::readyCastable() const
    {
        for (std::size_t i = 0; i < mCastables.size(); ++i)
            if (mCastables[i].mReady)
                return static_cast<int>(i);
        return -1;
    }

    std::vector<MagicScreen::Row> MagicScreen::buildRows()
    {
        std::vector<Row> rows;
        if (mTab == Tab::Recent)
        {
            for (int item : recentCastables(false))
                rows.push_back(Row{ {}, item });
            return rows;
        }

        Castable::Kind kind = Castable::Kind::Spell;
        if (mTab == Tab::Powers)
            kind = Castable::Kind::Power;
        else if (mTab == Tab::Items)
            kind = Castable::Kind::Item;
        else if (mTab == Tab::Scrolls)
            kind = Castable::Kind::Scroll;
        std::vector<int> items;
        for (std::size_t i = 0; i < mCastables.size(); ++i)
            if (mCastables[i].mKind == kind)
                items.push_back(static_cast<int>(i));

        std::stable_sort(
            items.begin(), items.end(), [&](int a, int b) { return mCastables[a].mName < mCastables[b].mName; });

        if (mTab == Tab::Powers)
        {
            for (int item : items)
                rows.push_back(Row{ {}, item });
            return rows;
        }

        // Spells are grouped by their school, such as Destruction. Items and scrolls are grouped by the range of their
        // first effect, such as Self or Target.
        std::map<std::string, std::vector<int>> groups;
        for (int item : items)
        {
            const Castable& castable = mCastables[item];
            if (mTab == Tab::Spells)
            {
                const ESM::Skill* school = store().get<ESM::Skill>().search(castable.mSchool);
                groups[school != nullptr ? escaped(school->mName) : std::string()].push_back(item);
                continue;
            }
            const int range = castable.mEffects != nullptr && !castable.mEffects->mList.empty()
                ? castable.mEffects->mList.front().mData.mRange
                : ESM::RT_Self;
            // The keys sort the groups from nearest to farthest: Self, Touch, then Target.
            groups[std::to_string(range)].push_back(item);
        }
        for (const auto& [group, members] : groups)
        {
            std::string header = group;
            if (mTab != Tab::Spells)
            {
                const int range = std::stoi(group);
                header = range == ESM::RT_Touch ? "#{sRangeTouch}"
                    : range == ESM::RT_Target   ? "#{sRangeTarget}"
                                                : "#{sRangeSelf}";
            }
            rows.push_back(Row{ header, -1 });
            for (int item : members)
                rows.push_back(Row{ {}, item });
        }
        return rows;
    }

    int MagicScreen::focusedCastable()
    {
        if (mTab == Tab::Active)
        {
            if (mEffectsFocused)
                return -1;
            const std::vector<int> recent = recentCastables(true);
            if (recent.empty())
                return -1;
            return recent[std::clamp(mRecentList.getFocus(), 0, static_cast<int>(recent.size()) - 1)];
        }
        const std::vector<Row> rows = buildRows();
        const int focus = mLists[static_cast<int>(mTab)].getFocus();
        int index = 0;
        for (const Row& row : rows)
        {
            if (row.mItem < 0)
                continue;
            if (index == focus)
                return row.mItem;
            ++index;
        }
        return -1;
    }

    // ---------------------------------------------------------------------------------------------------------------
    // Drawing

    void MagicScreen::redraw()
    {
        clearContent();
        const MyGUI::IntSize size = mMainWidget->getSize();
        const MWWorld::Ptr actor = player();
        if (actor.isEmpty())
            return;

        std::vector<std::string> captions;
        for (int tab = 0; tab < sTabCount; ++tab)
            captions.push_back(tabKey(tab));
        const auto& magicka = actor.getClass().getCreatureStats(actor).getMagicka();
        const std::string status = "#{sMagic}  " + std::to_string(static_cast<int>(magicka.getCurrent())) + " / "
            + std::to_string(static_cast<int>(magicka.getModified()));
        drawTabRow(mContent, size.width, captions, static_cast<int>(mTab), status);

        if (mTab == Tab::Active)
        {
            drawActive(size.width);
            return;
        }

        const int top = sContentTop;
        const int bottom = size.height - sContentBottomMargin;
        const int panelW = std::max(320, static_cast<int>(size.width * 0.34f));
        const int listW = size.width - 2 * sMargin - panelW - 16;
        const std::vector<Row> rows = buildRows();
        addRect(mContent, sMargin, top, listW, bottom - top, sPanelColour, 0.9f);
        drawList(sMargin, top, listW, bottom - top, rows, mLists[static_cast<int>(mTab)], true);
        const int item = focusedCastable();
        const int px = size.width - sMargin - panelW;
        if (item >= 0)
            drawDetails(px, top, panelW, bottom - top, item);
        else
        {
            addRect(mContent, px, top, panelW, bottom - top, sPanelColour, 0.9f);
            addText(mContent, px + 16, top + 16, panelW - 32, 30,
                mTab == Tab::Recent ? "#{Interface:MagicNoRecentSpells}" : "#{Interface:MagicNoSpells}", sFontNormal,
                sDimColour);
        }
    }

    void MagicScreen::drawActive(int width)
    {
        const int top = sContentTop;
        const int bottom = mMainWidget->getHeight() - sContentBottomMargin;
        const int columnW = (width - 2 * sMargin - 16) / 2;

        // The effects on the player.
        drawEffects(sMargin, top, columnW, bottom - top);

        // The spell or item ready to cast, and the ones used recently.
        const int x = sMargin + columnW + 16;
        addRect(mContent, x, top, columnW, bottom - top, sPanelColour, 0.9f);
        addText(mContent, x + 16, top + 8, columnW - 32, 30, "#{Interface:MagicReady}", sFontLarge, sBrightColour);
        drawReadyCard(x + 12, top + 44, columnW - 24, sReadyCardHeight, readyCastable());

        const int recentTop = top + 44 + sReadyCardHeight + 12;
        const int listTop = recentTop + 32;
        if (mQuickKeys.isChoosing())
        {
            const int item = focusedCastable();
            addText(mContent, x + 16, recentTop, columnW - 32, 30,
                item >= 0 ? escaped(mCastables[item].mName) : std::string(), sFontLarge, sBrightColour);
            const int hintsTop = ControllerUi::drawHints(mContent, x + 16, columnW - 32, bottom,
                { { SDL_CONTROLLER_BUTTON_DPAD_UP, "#{Interface:QuickKeyChoose}" } });
            mQuickKeys.draw(mContent, x + 16, listTop, columnW - 32, hintsTop - 12 - listTop);
            return;
        }

        addText(mContent, x + 16, recentTop, columnW - 32, 30, "#{Interface:MagicRecent}", sFontLarge, sBrightColour);
        std::vector<Row> rows;
        for (int item : recentCastables(true))
            rows.push_back(Row{ {}, item });
        const int hintsTop = mEffectsFocused ? bottom : drawHints(x, top, columnW, bottom - top, focusedCastable());
        const int listBottom = hintsTop - 12;
        if (rows.empty())
            addText(mContent, x + 16, listTop, columnW - 32, 30, "#{Interface:MagicNoRecentSpells}", sFontNormal,
                sDimColour);
        else
            drawList(x + 4, listTop, columnW - 8, listBottom - listTop, rows, mRecentList, !mEffectsFocused);
    }

    void MagicScreen::drawEffects(int x, int y, int w, int h)
    {
        addRect(mContent, x, y, w, h, sPanelColour, 0.9f);
        addText(mContent, x + 16, y + 8, w - 32, 30, "#{Interface:MagicActiveEffects}", sFontLarge, sBrightColour);
        if (mEffects.empty())
        {
            addText(mContent, x + 16, y + 48, w - 32, 30, "#{Interface:MagicNoEffects}", sFontNormal, sDimColour);
            return;
        }

        const int listTop = y + 44;
        const int listBottom = y + h - 8;
        const std::vector<ControllerList::Row> rows(mEffects.size(), ControllerList::Row{ sEffectHeight, true });
        int focusedRowY = -1;
        const auto drawRow = [&](std::size_t i, int rowY, bool listFocused) {
            const ActiveEffect& effect = mEffects[i];
            const bool focused = mEffectsFocused && listFocused;
            if (listFocused)
                focusedRowY = rowY;
            if (focused)
            {
                addRect(mContent, x + 4, rowY, w - 8, sEffectHeight - 2, sAccentColour, 0.16f);
                addRect(mContent, x + 4, rowY, 3, sEffectHeight - 2, sAccentColour);
            }
            addImage(mContent, x + 12, rowY + 6, sEffectHeight - 14, effect.mIcon);
            const int textX = x + 12 + sEffectHeight - 14 + 12;
            const int timeW = 130;
            addText(mContent, textX, rowY + 3, x + w - textX - timeW - 16, 24, escaped(effect.mText), sFontNormal,
                focused ? sActiveColour : sBrightColour);
            addText(mContent, textX, rowY + 25, x + w - textX - 16, 20, effect.mSource, sFontSmall, sDimColour);
            addText(mContent, x + w - timeW - 16, rowY + 3, timeW, 24,
                effect.mTimeLeft < 0 ? "#{Interface:MagicConstant}" : remainingTime(effect.mTimeLeft), sFontSmall,
                sDimColour, MyGUI::Align::Right | MyGUI::Align::VCenter);
        };
        // Pointing at an effect or clicking it highlights it, moving the selection there from the recent spells.
        // A does nothing on an effect, so a click does nothing more.
        mEffectList.draw(
            mContent, x, listTop, w, listBottom - listTop, rows, drawRow,
            [this] {
                mEffectsFocused = true;
                redraw();
            },
            nullptr);
        coverList(x, listTop, w, listBottom - listTop, focusedRowY, sEffectHeight, mEffectsFocused, true);
    }

    void MagicScreen::drawReadyCard(int x, int y, int w, int h, int item)
    {
        addRect(mContent, x, y, w, h, sAccentColour, 0.08f);
        addRect(mContent, x, y, 3, h, sAccentColour, 0.6f);
        if (item < 0)
        {
            addText(mContent, x + 20, y + 16, w - 40, 30, "#{Interface:MagicNothingReady}", sFontNormal, sDimColour);
            return;
        }
        const Castable& castable = mCastables[item];
        if (castable.mItem.isEmpty())
            addImage(mContent, x + 16, y + 16, 64, castable.mIcon);
        else
            addIcon(mContent, x + 16, y + 16, 64, castable.mItem, false);
        addText(mContent, x + 96, y + 12, w - 112, 32, escaped(castable.mName), sFontLarge, sActiveColour);

        std::string numbers;
        if (castable.mKind == Castable::Kind::Spell)
            numbers = "#{Interface:MagicCost} " + std::to_string(castable.mCost) + "      #{Interface:MagicChance} "
                + std::to_string(castable.mChance) + "%";
        else if (castable.mKind == Castable::Kind::Power)
            numbers = castable.mUsedToday ? "#{Interface:MagicUsedToday}" : "#{Interface:MagicKindPower}";
        else if (castable.mKind == Castable::Kind::Item)
            numbers = "#{Interface:MagicCharge} " + std::to_string(castable.mCharge) + " / "
                + std::to_string(castable.mMaxCharge);
        else
            numbers = "#{Interface:InventoryColumnCount} " + std::to_string(castable.mCount);
        addText(mContent, x + 96, y + 42, w - 112, 24, numbers, sFontSmall, sTextColour);

        int ey = y + 90;
        for (const std::string& line : describeEffects(castable.mEffects))
        {
            if (ey + 24 > y + h - 4)
                break;
            addText(mContent, x + 16, ey, w - 32, 24, escaped(line), sFontNormal, sNewColour);
            ey += 24;
        }
    }

    void MagicScreen::drawList(
        int x, int y, int w, int h, const std::vector<Row>& rows, ControllerList& list, bool focused)
    {
        const int top = y + 8;
        const int listBottom = y + h - 4;

        std::vector<ControllerList::Row> listRows;
        for (const Row& row : rows)
            listRows.push_back({ row.mItem < 0 ? sHeaderRowHeight : sEntryHeight, row.mItem >= 0 });
        int focusedRowY = -1;
        const auto drawRow = [&](std::size_t i, int rowY, bool listFocused) {
            const Row& row = rows[i];
            if (row.mItem < 0)
            {
                addText(mContent, x + 12, rowY, w - 24, 24, row.mHeader, sFontSmall, sBrightColour);
                addRect(mContent, x + 12, rowY + 23, w - 24, 1, sAccentColour, 0.25f);
                return;
            }
            if (listFocused)
                focusedRowY = rowY;
            drawEntry(x + 4, rowY, w - 8, sEntryHeight - 2, row.mItem, focused && listFocused);
        };
        // Pointing at an entry highlights it, and clicking it readies it, as A does. On the Active tab, either one
        // also moves the selection from the effects to the recent spells.
        if (!list.draw(
                mContent, x, top, w, listBottom - top, listRows, drawRow,
                [this] {
                    if (mTab == Tab::Active)
                        mEffectsFocused = false;
                    redraw();
                },
                [] { pressButton(SDL_CONTROLLER_BUTTON_A); }))
        {
            addText(mContent, x + 16, top + 8, w - 32, 30, "#{Interface:MagicNoSpells}", sFontNormal, sDimColour);
            return;
        }
        coverList(x, top, w, listBottom - top, focusedRowY, sEntryHeight, focused, false);
    }

    void MagicScreen::coverList(int x, int y, int w, int h, int focusedRowY, int rowHeight, bool focused, bool effects)
    {
        // While the player picks a quick key, the d-pad doesn't move the selection, so neither does the mouse. A
        // target that does nothing covers the list's own targets.
        if (mQuickKeys.hasDpad())
        {
            addPointerTarget(mContent, x, y, w, h, nullptr, nullptr);
            return;
        }
        // In the Active tab's other column, the list keeps its focused row, so pointing at that row doesn't change
        // the list's focus, and the list does nothing. A target over that row moves the selection to its column
        // instead, as the d-pad does.
        if (focused || focusedRowY < 0)
            return;
        const auto focusColumn = [this, effects] {
            mEffectsFocused = effects;
            redraw();
        };
        if (effects)
            addPointerTarget(mContent, x, focusedRowY, w, rowHeight, focusColumn, focusColumn);
        else
            addPointerTarget(mContent, x, focusedRowY, w, rowHeight, focusColumn, [focusColumn] {
                focusColumn();
                pressButton(SDL_CONTROLLER_BUTTON_A);
            });
    }

    void MagicScreen::drawEntry(int x, int y, int w, int h, int item, bool focused)
    {
        const Castable& castable = mCastables[item];
        if (focused)
        {
            addRect(mContent, x, y, w, h, sAccentColour, 0.16f);
            addRect(mContent, x, y, 3, h, sAccentColour);
        }
        const int iconSize = h - 12;
        if (castable.mItem.isEmpty())
            addImage(mContent, x + 8, y + 6, iconSize, castable.mIcon);
        else
            addIcon(mContent, x + 8, y + 6, iconSize, castable.mItem, false);

        const int textX = x + 8 + iconSize + 12;
        const int numbersW = 130;
        std::string name = escaped(castable.mName);
        if (castable.mKind == Castable::Kind::Scroll && castable.mCount > 1)
            name += " (" + std::to_string(castable.mCount) + ")";
        addText(mContent, textX, y + 4, x + w - textX - numbersW - 8, 26, name, sFontNormal,
            focused ? sActiveColour : sBrightColour);

        // Spells show their cost and chance, and items their charge, as the normal magic menu does.
        std::string numbers;
        if (castable.mKind == Castable::Kind::Spell)
            numbers = std::to_string(castable.mCost) + " / " + std::to_string(castable.mChance) + "%";
        else if (castable.mKind == Castable::Kind::Item)
            numbers = std::to_string(castable.mCharge) + " / " + std::to_string(castable.mMaxCharge);
        addText(mContent, x + w - numbersW - 8, y + 4, numbersW, 26, numbers, sFontNormal, sTextColour,
            MyGUI::Align::Right | MyGUI::Align::VCenter);

        const std::vector<std::string> effects = describeEffects(castable.mEffects);
        std::string summary = effects.empty() ? std::string() : escaped(effects.front());
        if (effects.size() > 1)
            summary += ",  +" + std::to_string(effects.size() - 1);
        addText(mContent, textX, y + 30, x + w - textX - numbersW - 8, 22, summary, sFontSmall, sDimColour);

        std::string marker;
        MyGUI::Colour markerColour = sNewColour;
        if (castable.mReady)
            marker = "#{Interface:MagicReady}";
        else if (castable.mUsedToday)
        {
            marker = "#{Interface:MagicUsedToday}";
            markerColour = sDimColour;
        }
        int markerRight = x + w - 8;
        if (const int quickKey = dpadSlotOf(castable.mItem, castable.mId); quickKey > 0)
        {
            drawQuickKeyMarker(mContent, markerRight - 20, y + 31, 4, quickKey);
            markerRight -= 20 + 10;
        }
        addText(mContent, markerRight - numbersW, y + 30, numbersW, 22, marker, sFontSmall, markerColour,
            MyGUI::Align::Right | MyGUI::Align::VCenter);
    }

    void MagicScreen::drawDetails(int x, int y, int w, int h, int item)
    {
        const Castable& castable = mCastables[item];
        addRect(mContent, x, y, w, h, sPanelColour, 0.9f);
        addRect(mContent, x, y, w, 1, sAccentColour, 0.3f);
        if (castable.mItem.isEmpty())
            addImage(mContent, x + 16, y + 16, 64, castable.mIcon);
        else
            addIcon(mContent, x + 16, y + 16, 64, castable.mItem, false);
        addText(mContent, x + 92, y + 14, w - 108, 34, escaped(castable.mName), sFontLarge, sBrightColour);

        std::string kind;
        std::vector<std::pair<std::string, std::string>> rows;
        switch (castable.mKind)
        {
            case Castable::Kind::Spell:
            {
                const ESM::Skill* school = store().get<ESM::Skill>().search(castable.mSchool);
                kind = "#{sSchool}: " + (school != nullptr ? escaped(school->mName) : std::string());
                rows = { { "#{Interface:MagicCost}", std::to_string(castable.mCost) },
                    { "#{Interface:MagicChance}", std::to_string(castable.mChance) + "%" } };
                break;
            }
            case Castable::Kind::Power:
                kind = "#{Interface:MagicKindPower}";
                break;
            case Castable::Kind::Item:
                kind = "#{sItemCastWhenUsed}";
                rows = { { "#{sCastCost}", std::to_string(castable.mCost) },
                    { "#{Interface:MagicCharge}",
                        std::to_string(castable.mCharge) + " / " + std::to_string(castable.mMaxCharge) },
                    { "#{Interface:MagicCasts}",
                        std::to_string(castable.mCost > 0 ? castable.mCharge / castable.mCost : 0) } };
                break;
            case Castable::Kind::Scroll:
                kind = "#{sItemCastOnce}";
                rows = { { "#{Interface:InventoryColumnCount}", std::to_string(castable.mCount) } };
                break;
        }
        addText(mContent, x + 92, y + 46, w - 108, 26, kind, sFontSmall, sDimColour);
        int tx = x + 92;
        const auto addTag = [&](std::string_view label, const MyGUI::Colour& colour) {
            MyGUI::TextBox* tag = addText(mContent, tx, y + 70, 160, 20, label, sFontSmall, colour);
            tx += tag->getTextSize().width + 12;
        };
        if (castable.mReady)
            addTag("#{Interface:MagicReady}", sNewColour);
        if (castable.mUsedToday)
            addTag("#{Interface:MagicUsedToday}", sDimColour);

        if (mQuickKeys.isChoosing())
        {
            const int hintsTop = ControllerUi::drawHints(mContent, x + 16, w - 32, y + h, QuickKeyAssigner::hints());
            mQuickKeys.draw(mContent, x + 16, y + 104, w - 32, hintsTop - 12 - (y + 104));
            return;
        }

        const int hintsTop = drawHints(x, y, w, h, item);
        int ry = y + 104;
        for (const auto& [label, value] : rows)
        {
            addText(mContent, x + 16, ry, w - 32, 28, label, sFontNormal, sTextColour);
            addText(mContent, x + w - 16 - 120, ry, 120, 28, value, sFontNormal, sBrightColour,
                MyGUI::Align::Right | MyGUI::Align::VCenter);
            ry += 30;
        }

        // Each effect, written out in full.
        ry += 8;
        addText(mContent, x + 16, ry, w - 32, 24, "#{Interface:MagicEffects}", sFontSmall, sDimColour);
        ry += 26;
        const std::vector<Widgets::SpellEffectParams> effects
            = castable.mEffects != nullptr ? Widgets::MWEffectList::effectListFromESM(castable.mEffects)
                                           : std::vector<Widgets::SpellEffectParams>();
        for (const Widgets::SpellEffectParams& params : effects)
        {
            const std::vector<std::string> lines = wrapText(
                mContent, localised(Widgets::describeSpellEffect(params)), w - 32 - 36, sFontNormal, 3);
            const int effectH = static_cast<int>(lines.size()) * 24;
            if (ry + effectH > hintsTop - 16)
                break;
            addImage(mContent, x + 16, ry, 26, bigIcon(*store().get<ESM::MagicEffect>().find(params.mEffectID)));
            for (std::size_t line = 0; line < lines.size(); ++line)
                addText(mContent, x + 16 + 36, ry + static_cast<int>(line) * 24, w - 32 - 36, 24,
                    escaped(lines[line]), sFontNormal, sNewColour);
            ry += std::max(effectH, 28) + 6;
        }
    }

    int MagicScreen::drawHints(int x, int y, int w, int h, int item)
    {
        if (item < 0)
            return y + h;
        const Castable& castable = mCastables[item];
        std::vector<Hint> hints;
        if (!castable.mReady)
            hints.emplace_back(SDL_CONTROLLER_BUTTON_A, "#{Interface:MagicActionReady}");
        if (castable.mDeletable)
            hints.emplace_back(SDL_CONTROLLER_BUTTON_X, "#{sDelete}");
        if (mTab != Tab::Active)
        {
            const std::vector<Row> rows = buildRows();
            if (std::count_if(rows.begin(), rows.end(), [](const Row& row) { return row.mItem < 0; }) > 1)
                hints.emplace_back(SDL_CONTROLLER_BUTTON_Y, "#{Interface:MagicNextGroup}");
        }
        hints.emplace_back(SDL_CONTROLLER_BUTTON_LEFTSTICK, "#{Interface:QuickKey}");
        return ControllerUi::drawHints(mContent, x + 16, w - 32, y + h, hints);
    }

    // ---------------------------------------------------------------------------------------------------------------
    // Input

    void MagicScreen::move(int dx, int dy)
    {
        if (mTab == Tab::Active)
        {
            if (dx < 0)
                mEffectsFocused = true;
            else if (dx > 0)
                mEffectsFocused = false;
            ControllerList& list = mEffectsFocused ? mEffectList : mRecentList;
            const int count
                = mEffectsFocused ? static_cast<int>(mEffects.size()) : static_cast<int>(recentCastables(true).size());
            if (count > 0)
                list.setFocus(std::clamp(list.getFocus() + dy, 0, count - 1));
        }
        else if (dy != 0)
        {
            int itemCount = 0;
            for (const Row& row : buildRows())
                itemCount += row.mItem >= 0 ? 1 : 0;
            ControllerList& list = mLists[static_cast<int>(mTab)];
            if (itemCount > 0)
                list.setFocus(std::clamp(list.getFocus() + dy, 0, itemCount - 1));
        }
        redraw();
    }

    void MagicScreen::changeTab(int delta)
    {
        mTab = static_cast<Tab>((static_cast<int>(mTab) + delta + sTabCount) % sTabCount);
        redraw();
    }

    void MagicScreen::nextGroup()
    {
        // Moves the selection to the first entry of the next group, or back to the first group after the last one.
        if (mTab == Tab::Active)
            return;
        const std::vector<Row> rows = buildRows();
        ControllerList& list = mLists[static_cast<int>(mTab)];
        // Each group's first entry, and the row of its header.
        std::vector<std::pair<int, int>> groupStarts;
        for (int i = 0, index = 0; i < static_cast<int>(rows.size()); ++i)
        {
            if (rows[i].mItem < 0)
            {
                groupStarts.emplace_back(index, i);
                continue;
            }
            ++index;
        }
        if (groupStarts.size() < 2)
            return;
        auto next = std::find_if(groupStarts.begin(), groupStarts.end(),
            [&](const std::pair<int, int>& start) { return start.first > list.getFocus(); });
        if (next == groupStarts.end())
            next = groupStarts.begin();
        // The group's header goes to the top of the list, so the whole group shows.
        list.setFocus(next->first);
        list.setScroll(next->second);
        MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
        redraw();
    }

    void MagicScreen::ready(int item)
    {
        const Castable castable = mCastables[item];
        if (castable.mReady)
            return;
        MWBase::WindowManager* winMgr = MWBase::Environment::get().getWindowManager();
        const MWWorld::Ptr actor = player();
        MWWorld::InventoryStore& inventory = actor.getClass().getInventoryStore(actor);
        if (castable.mItem.isEmpty())
        {
            inventory.setSelectedEnchantItem(inventory.end());
            winMgr->setSelectedSpell(
                castable.mId, static_cast<int>(MWMechanics::getSpellSuccessChance(castable.mId, actor)));
        }
        else
        {
            // An item with a cast when used enchantment is equipped before it can be readied, as in the normal
            // magic menu.
            if (castable.mKind == Castable::Kind::Item && !inventory.isEquipped(castable.mItem)
                && !castable.mItem.getClass().getEquipmentSlots(castable.mItem).first.empty())
            {
                winMgr->useItem(castable.mItem);
                if (!inventory.isEquipped(castable.mItem))
                    return;
            }
            for (auto it = inventory.begin(); it != inventory.end(); ++it)
            {
                if (*it != castable.mItem)
                    continue;
                inventory.setSelectedEnchantItem(it);
                winMgr->setSelectedEnchantItem(*it);
                break;
            }
        }
        winMgr->playSound(ESM::RefId::stringRefId("Menu Click"));
        winMgr->updateSpellWindow();
        update();
        mShownState = currentState();
        redraw();
    }

    void MagicScreen::startQuickKey()
    {
        const int item = focusedCastable();
        if (item < 0)
            return;
        const Castable& castable = mCastables[item];
        // Spells and powers go on a quick key by their ID. Magic items and scrolls go on as the item, so the quick
        // key readies the item's enchantment.
        if (castable.mItem.isEmpty())
            mQuickKeys.start(QuickKeyAssigner::Target{ ESM::QuickKeys::Type::Magic, {}, castable.mId });
        else
            mQuickKeys.start(QuickKeyAssigner::Target{ ESM::QuickKeys::Type::MagicItem, castable.mItem, {} });
        MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
        redraw();
    }

    void MagicScreen::askDelete(int item)
    {
        const Castable& castable = mCastables[item];
        if (castable.mKind != Castable::Kind::Spell)
            return;
        MWBase::WindowManager* winMgr = MWBase::Environment::get().getWindowManager();
        if (!castable.mDeletable)
        {
            winMgr->messageBox("#{sDeleteSpellError}");
            return;
        }
        mSpellToDelete = castable.mId;
        ConfirmationDialog* dialog = winMgr->getConfirmationDialog();
        std::string question{ winMgr->getGameSettingString("sQuestionDeleteSpell", "Delete %s?") };
        dialog->askForConfirmation(Misc::StringUtils::format(question, castable.mName));
        dialog->eventOkClicked.clear();
        dialog->eventOkClicked += MyGUI::newDelegate(this, &MagicScreen::onDeleteAccepted);
        dialog->eventCancelClicked.clear();
    }

    void MagicScreen::onDeleteAccepted()
    {
        MWBase::WindowManager* winMgr = MWBase::Environment::get().getWindowManager();
        if (winMgr->getSelectedSpell() == mSpellToDelete)
            winMgr->unsetSelectedSpell();
        const MWWorld::Ptr actor = player();
        actor.getClass().getCreatureStats(actor).getSpells().remove(mSpellToDelete);
        winMgr->updateSpellWindow();
        update();
        mShownState = currentState();
        redraw();
    }
}
