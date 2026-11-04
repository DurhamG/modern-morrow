#include "effectcraftpanel.hpp"

#include <algorithm>
#include <map>

#include <MyGUI_EditBox.h>
#include <MyGUI_Gui.h>
#include <MyGUI_ImageBox.h>
#include <MyGUI_ScrollBar.h>

#include <components/esm/attr.hpp>
#include <components/esm3/loadcrea.hpp>
#include <components/esm3/loadench.hpp>
#include <components/esm3/loadgmst.hpp>
#include <components/esm3/loadmgef.hpp>
#include <components/esm3/loadskil.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/magiceffects.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/containerstore.hpp"
#include "../mwworld/esmstore.hpp"

#include "enchantingdialog.hpp"
#include "inventoryitemmodel.hpp"
#include "sortfilteritemmodel.hpp"
#include "spellcreationdialog.hpp"
#include "textinput.hpp"
#include "widgets.hpp"

namespace MWGui
{
    namespace
    {
        using namespace ControllerUi;

        constexpr int sHeaderHeight = 30;
        constexpr int sEntryHeight = 52;
        constexpr int sLineHeight = 44;
        constexpr int sEffectLineHeight = 32;
        constexpr int sSettingHeight = 54;
        constexpr std::size_t sMaxEffects = 8;

        const MWWorld::ESMStore& store()
        {
            return *MWBase::Environment::get().getESMStore();
        }

        MWBase::WindowManager& windowManager()
        {
            return *MWBase::Environment::get().getWindowManager();
        }

        void playSound(std::string_view sound)
        {
            windowManager().playSound(ESM::RefId::stringRefId(sound));
        }

        std::string effectIcon(const ESM::MagicEffect& effect)
        {
            return std::string(Misc::ResourceHelpers::correctBigIconPath(
                effect.mIcon.getNormalized(), *MWBase::Environment::get().getResourceSystem()->getVFS())
                    .value());
        }

        std::string rangeKey(int range)
        {
            if (range == ESM::RT_Touch)
                return "#{sRangeTouch}";
            if (range == ESM::RT_Target)
                return "#{sRangeTarget}";
            return "#{sRangeSelf}";
        }

        std::string castStyleKey(int style)
        {
            switch (style)
            {
                case ESM::Enchantment::CastOnce:
                    return "#{sItemCastOnce}";
                case ESM::Enchantment::WhenStrikes:
                    return "#{sItemCastWhenStrikes}";
                case ESM::Enchantment::WhenUsed:
                    return "#{sItemCastWhenUsed}";
                default:
                    return "#{sItemCastConstant}";
            }
        }

        // An effect of the enchantment written out, such as "Fire Damage 5 to 10 pts for 3 secs on Target".
        std::string describeEffect(const ESM::ENAMstruct& effect, bool constant)
        {
            Widgets::SpellEffectParams params;
            params.mEffectID = effect.mEffectID;
            params.mSkill = effect.mSkill;
            params.mAttribute = effect.mAttribute;
            params.mDuration = effect.mDuration;
            params.mMagnMin = effect.mMagnMin;
            params.mMagnMax = effect.mMagnMax;
            params.mRange = effect.mRange;
            params.mArea = effect.mArea;
            params.mIsConstant = constant;
            return localised(Widgets::describeSpellEffect(params));
        }

        // The ranges an effect can have, such as "Self, Touch".
        std::string allowedRanges(const ESM::MagicEffect& effect, bool constant)
        {
            if (constant)
                return localised("#{sRangeSelf}");
            std::string text;
            const auto add = [&](int flag, const std::string& key) {
                if (effect.mData.mFlags & flag)
                    text += (text.empty() ? "" : ", ") + localised(key);
            };
            add(ESM::MagicEffect::CastSelf, "#{sRangeSelf}");
            add(ESM::MagicEffect::CastTouch, "#{sRangeTouch}");
            add(ESM::MagicEffect::CastTarget, "#{sRangeTarget}");
            return text;
        }

        int playerGold()
        {
            const MWWorld::Ptr player = MWMechanics::getPlayer();
            return player.getClass().getContainerStore(player).count(MWWorld::ContainerStore::sGoldId);
        }

        std::vector<MWWorld::Ptr> inventoryItems(int filter)
        {
            SortFilterItemModel model(std::make_unique<InventoryItemModel>(MWMechanics::getPlayer()));
            model.setFilter(filter);
            model.update();
            std::vector<MWWorld::Ptr> items;
            for (std::size_t i = 0; i < model.getItemCount(); ++i)
                items.push_back(model.getItem(static_cast<ItemModel::ModelIndex>(i)).mBase);
            return items;
        }
    }

    EffectCraftPanel::EffectCraftPanel(EnchantingDialog& window)
        : mWindow(window)
        , mEditor(window)
        , mEnchant(&window)
    {
    }

    EffectCraftPanel::EffectCraftPanel(SpellCreationDialog& window)
        : mWindow(window)
        , mEditor(window)
        , mSpell(&window)
    {
    }

    MWWorld::Ptr EffectCraftPanel::merchant() const
    {
        return mEnchant != nullptr ? mEnchant->mPtr : mSpell->mPtr;
    }

    MyGUI::EditBox* EffectCraftPanel::nameEdit() const
    {
        return mEnchant != nullptr ? mEnchant->mName : mSpell->mNameEdit;
    }

    void EffectCraftPanel::nameAfterFirstEffect()
    {
        // A new spell is named after its first effect, such as "Fortify Strength", so the player doesn't have to
        // type a name. Enchantments are named after their item already.
        if (mSpell == nullptr || !nameEdit()->getCaption().empty() || mEditor.mEffects.empty())
            return;
        const ESM::ENAMstruct& effect = mEditor.mEffects.front();
        const ESM::MagicEffect* magicEffect = store().get<ESM::MagicEffect>().find(effect.mEffectID);
        const std::string name = MWMechanics::getMagicEffectString(*magicEffect,
            store().get<ESM::Attribute>().search(effect.mAttribute), store().get<ESM::Skill>().search(effect.mSkill));
        nameEdit()->setCaption(MyGUI::UString(name));
    }

    EffectCraftPanel::~EffectCraftPanel()
    {
        if (mRoot != nullptr)
            MyGUI::Gui::getInstance().destroyWidget(mRoot);
    }

    void EffectCraftPanel::show()
    {
        if (mRoot == nullptr)
            mRoot = createPanelRoot("ControllerMenus");
        if (mRoot == nullptr)
            return;
        mRoot->setVisible(true);
        mPicker = Picker::Effects;
        mListFocused = false;
        mListFocus = 0;
        mListScroll = 0;
        mPendingEffect = nullptr;
        mHeldDirection.reset();
        // The highlight starts on the first thing still to choose.
        mLineFocus = 0;
        focusNextLine();
        update();
    }

    void EffectCraftPanel::hide()
    {
        if (mRoot != nullptr)
            mRoot->setVisible(false);
        mEntries.clear();
    }

    bool EffectCraftPanel::isSelfEnchanting() const
    {
        // The window has no enchanter only when the player enchants with their own soul gem.
        return mEnchant != nullptr && (merchant().isEmpty() || merchant() == MWMechanics::getPlayer());
    }

    bool EffectCraftPanel::isEditing() const
    {
        return mEditor.mAddEffectDialog.isVisible();
    }

    std::vector<EffectCraftPanel::Line> EffectCraftPanel::lines() const
    {
        std::vector<Line> result;
        if (mEnchant != nullptr)
            result = { { Line::Kind::Item }, { Line::Kind::SoulGem }, { Line::Kind::CastType } };
        for (std::size_t i = 0; i < mEditor.mEffects.size(); ++i)
            result.push_back({ Line::Kind::Effect, static_cast<int>(i) });
        if (mEditor.mEffects.size() < sMaxEffects)
            result.push_back({ Line::Kind::AddEffect });
        return result;
    }

    void EffectCraftPanel::focusNextLine()
    {
        // The first of the item and the soul gem that is still missing, or else adding an effect.
        const std::vector<Line> all = lines();
        for (std::size_t i = 0; i < all.size(); ++i)
        {
            const bool missing = (all[i].mKind == Line::Kind::Item && mEnchant->mEnchanting.itemEmpty())
                || (all[i].mKind == Line::Kind::SoulGem && mEnchant->mEnchanting.soulEmpty())
                || all[i].mKind == Line::Kind::AddEffect;
            if (missing)
            {
                mLineFocus = static_cast<int>(i);
                return;
            }
        }
        mLineFocus = static_cast<int>(all.size()) - 1;
    }

    std::vector<EffectCraftPanel::Setting> EffectCraftPanel::settings() const
    {
        const EditEffectDialog& dialog = mEditor.mAddEffectDialog;
        std::vector<Setting> result = { Setting::Range };
        if (dialog.mMagnitudeBox->getVisible())
        {
            result.push_back(Setting::MagnitudeMin);
            result.push_back(Setting::MagnitudeMax);
        }
        if (dialog.mDurationBox->getVisible())
            result.push_back(Setting::Duration);
        if (dialog.mAreaBox->getVisible())
            result.push_back(Setting::Area);
        return result;
    }

    void EffectCraftPanel::setPicker(Picker picker)
    {
        mPicker = picker;
        mListFocus = 0;
        mListScroll = 0;
    }

    void EffectCraftPanel::readEntries()
    {
        mEntries.clear();
        switch (mPicker)
        {
            case Picker::Effects:
            {
                // The effects the player knows from their spells, grouped by school.
                std::map<std::string, std::vector<Entry>> groups;
                for (const auto& [index, id] : mEditor.mButtonMapping)
                {
                    const ESM::MagicEffect* effect = store().get<ESM::MagicEffect>().find(id);
                    Entry entry;
                    entry.mId = id;
                    entry.mIcon = effectIcon(*effect);
                    entry.mName = escaped(effect->mName);
                    entry.mDetail = escaped(allowedRanges(*effect, mEditor.mConstantEffect));
                    const bool targetsOne
                        = effect->mData.mFlags & (ESM::MagicEffect::TargetSkill | ESM::MagicEffect::TargetAttribute);
                    entry.mAdded = !targetsOne
                        && std::any_of(mEditor.mEffects.begin(), mEditor.mEffects.end(),
                            [&](const ESM::ENAMstruct& added) { return added.mEffectID == id; });
                    const ESM::Skill* school = store().get<ESM::Skill>().search(effect->mData.mSchool);
                    groups[school != nullptr ? school->mName : std::string()].push_back(std::move(entry));
                }
                for (auto& [school, entries] : groups)
                {
                    mEntries.push_back(Entry{ .mHeader = escaped(school) });
                    std::stable_sort(entries.begin(), entries.end(),
                        [](const Entry& a, const Entry& b) { return a.mName < b.mName; });
                    for (Entry& entry : entries)
                        mEntries.push_back(std::move(entry));
                }
                break;
            }
            case Picker::Items:
            {
                const float mult = store().get<ESM::GameSetting>().find("fEnchantmentMult")->mValue.getFloat();
                for (const MWWorld::Ptr& item : inventoryItems(SortFilterItemModel::Filter_OnlyEnchantable))
                {
                    Entry entry;
                    entry.mItem = item;
                    entry.mName = escaped(item.getClass().getName(item));
                    entry.mDetail = "#{Interface:EnchantCapacity} "
                        + std::to_string(static_cast<int>(item.getClass().getEnchantmentPoints(item) * mult));
                    entry.mAdded = item == mEnchant->mEnchanting.getOldItem();
                    mEntries.push_back(std::move(entry));
                }
                break;
            }
            case Picker::SoulGems:
            {
                for (const MWWorld::Ptr& gem : inventoryItems(SortFilterItemModel::Filter_OnlyChargedSoulstones))
                {
                    const ESM::Creature* soul = store().get<ESM::Creature>().search(gem.getCellRef().getSoul());
                    Entry entry;
                    entry.mItem = gem;
                    entry.mName = escaped(gem.getClass().getName(gem));
                    if (soul != nullptr)
                        entry.mDetail = escaped(soul->mName) + ",  #{Interface:EnchantSoulStrength} "
                            + std::to_string(soul->mData.mSoul);
                    entry.mAdded = gem == mEnchant->mEnchanting.getGem();
                    mEntries.push_back(std::move(entry));
                }
                break;
            }
            case Picker::Skills:
                for (const ESM::Skill& skill : store().get<ESM::Skill>())
                {
                    Entry entry;
                    entry.mId = skill.mId;
                    entry.mIcon = std::string(skill.mIcon.getNormalized().value());
                    entry.mName = escaped(skill.mName);
                    mEntries.push_back(std::move(entry));
                }
                break;
            case Picker::Attributes:
                for (const ESM::Attribute& attribute : store().get<ESM::Attribute>())
                {
                    Entry entry;
                    entry.mId = attribute.mId;
                    entry.mIcon = std::string(attribute.mIcon.getNormalized().value());
                    entry.mName = escaped(attribute.mName);
                    mEntries.push_back(std::move(entry));
                }
                break;
        }
        mListFocus = std::clamp(mListFocus, 0, std::max(0, entryCount() - 1));
    }

    int EffectCraftPanel::entryCount() const
    {
        return static_cast<int>(
            std::count_if(mEntries.begin(), mEntries.end(), [](const Entry& entry) { return entry.mHeader.empty(); }));
    }

    const EffectCraftPanel::Entry* EffectCraftPanel::focusedEntry() const
    {
        int index = 0;
        for (const Entry& entry : mEntries)
        {
            if (!entry.mHeader.empty())
                continue;
            if (index++ == mListFocus)
                return &entry;
        }
        return nullptr;
    }

    void EffectCraftPanel::update()
    {
        if (mRoot == nullptr || !mRoot->getVisible())
            return;
        readEntries();
        mLineFocus = std::clamp(mLineFocus, 0, static_cast<int>(lines().size()) - 1);
        if (isEditing())
            mSettingFocus = std::clamp(mSettingFocus, 0, static_cast<int>(settings().size()) - 1);

        clearPanel(mRoot);
        const MyGUI::IntSize size = mRoot->getSize();
        addRect(mRoot, 0, 0, size.width, size.height, MyGUI::Colour::Black)->setNeedMouseFocus(true);

        // The title, and the skill or the gold that decides what the player can make.
        const bool self = isSelfEnchanting();
        addText(mRoot, sMargin, 20, size.width - 2 * sMargin, 40,
            mSpell != nullptr ? "#{Interface:SpellmakingTitle}" : "#{Interface:EnchantTitle}", sFontTitle,
            sBrightColour);
        std::string status;
        if (self)
        {
            const MWWorld::Ptr player = MWMechanics::getPlayer();
            status = "#{sSkillEnchant}  "
                + std::to_string(static_cast<int>(player.getClass().getSkill(player, ESM::Skill::Enchant)));
        }
        else if (!merchant().isEmpty())
            status = escaped(merchant().getClass().getName(merchant())) + "      #{Interface:BarterYourGold}  "
                + std::to_string(playerGold());
        addText(mRoot, sMargin, 20, size.width - 2 * sMargin, 40, status, sFontSmall, sDimColour,
            MyGUI::Align::Right | MyGUI::Align::VCenter);
        addRect(mRoot, sMargin, 66, size.width - 2 * sMargin, 1, sAccentColour, 0.2f);

        const int top = 82;
        const int bottom = size.height - sContentBottomMargin;
        const int listW = static_cast<int>((size.width - 2 * sMargin - 16) * 0.42f);
        if (isEditing())
            drawEditor(sMargin, top, listW, bottom - top);
        else
            drawList(sMargin, top, listW, bottom - top);
        const int px = sMargin + listW + 16;
        drawSummary(px, top, size.width - sMargin - px, bottom - top);
    }

    void EffectCraftPanel::drawList(int x, int y, int w, int h)
    {
        addRect(mRoot, x, y, w, h, sPanelColour, 0.9f);
        std::string title = "#{sMagicEffects}";
        std::string empty = "#{Interface:EnchantNoEffects}";
        if (mPicker == Picker::Items)
        {
            title = "#{sItem}";
            empty = "#{Interface:EnchantNoItems}";
        }
        else if (mPicker == Picker::SoulGems)
        {
            title = "#{sSoulGem}";
            empty = "#{Interface:EnchantNoSoulGems}";
        }
        else if (mPicker == Picker::Skills)
            title = "#{Interface:EnchantWhichSkill}";
        else if (mPicker == Picker::Attributes)
            title = "#{Interface:EnchantWhichAttribute}";
        addText(mRoot, x + 16, y + 8, w - 32, 30, title, sFontLarge, sBrightColour);

        const int listTop = y + 44;
        const int listBottom = y + h - 6;
        if (entryCount() == 0)
        {
            int ty = listTop;
            for (const std::string& line : wrapText(mRoot, localised(empty), w - 32, sFontNormal, 3))
            {
                addText(mRoot, x + 16, ty, w - 32, 26, escaped(line), sFontNormal, sDimColour);
                ty += 26;
            }
            return;
        }

        // Scroll so the focused entry, and the header above it, are in view.
        int focusRow = 0;
        for (int i = 0, index = 0; i < static_cast<int>(mEntries.size()); ++i)
        {
            if (!mEntries[i].mHeader.empty())
                continue;
            if (index++ == mListFocus)
            {
                focusRow = i;
                break;
            }
        }
        const auto rowHeight = [&](int i) { return mEntries[i].mHeader.empty() ? sEntryHeight : sHeaderHeight; };
        mListScroll = std::min(mListScroll, static_cast<int>(mEntries.size()) - 1);
        if (focusRow < mListScroll)
            mListScroll = focusRow > 0 && !mEntries[focusRow - 1].mHeader.empty() ? focusRow - 1 : focusRow;
        while (true)
        {
            int bottomOfFocus = listTop;
            for (int i = mListScroll; i <= focusRow; ++i)
                bottomOfFocus += rowHeight(i);
            if (bottomOfFocus <= listBottom || mListScroll >= focusRow)
                break;
            ++mListScroll;
        }

        int rowY = listTop;
        int index = 0;
        for (int i = 0; i < mListScroll; ++i)
            index += mEntries[i].mHeader.empty() ? 1 : 0;
        for (int i = mListScroll; i < static_cast<int>(mEntries.size()); ++i)
        {
            const Entry& entry = mEntries[i];
            if (rowY + rowHeight(i) > listBottom)
                break;
            if (!entry.mHeader.empty())
            {
                addText(mRoot, x + 12, rowY + 4, w - 24, 24, entry.mHeader, sFontSmall, sBrightColour);
                addRect(mRoot, x + 12, rowY + 27, w - 24, 1, sAccentColour, 0.25f);
                rowY += sHeaderHeight;
                continue;
            }
            const bool focused = mListFocused && index == mListFocus;
            const int rx = x + 4;
            const int rw = w - 8;
            if (focused)
            {
                addRect(mRoot, rx, rowY, rw, sEntryHeight - 2, sAccentColour, 0.16f);
                addRect(mRoot, rx, rowY, 3, sEntryHeight - 2, sAccentColour);
            }
            const int iconSize = sEntryHeight - 16;
            if (!entry.mItem.isEmpty())
                addIcon(mRoot, rx + 10, rowY + 7, iconSize, entry.mItem, false);
            else if (!entry.mIcon.empty())
                addImage(mRoot, rx + 10, rowY + 7, iconSize, entry.mIcon);
            const int textX = rx + 10 + iconSize + 12;
            const int textW = rx + rw - 12 - textX;
            const bool hasDetail = !entry.mDetail.empty();
            addText(mRoot, textX, rowY + (hasDetail ? 2 : 0), textW, hasDetail ? 26 : sEntryHeight - 2, entry.mName,
                sFontNormal, focused ? sActiveColour : (entry.mAdded ? sNewColour : sBrightColour));
            if (hasDetail)
                addText(mRoot, textX, rowY + 26, textW, 22, entry.mDetail, sFontSmall, sDimColour);
            rowY += sEntryHeight;
            ++index;
        }
    }

    void EffectCraftPanel::drawEditor(int x, int y, int w, int h)
    {
        const EditEffectDialog& dialog = mEditor.mAddEffectDialog;
        const ESM::ENAMstruct& effect = dialog.mEffect;
        addRect(mRoot, x, y, w, h, sPanelColour, 0.9f);
        addRect(mRoot, x, y, w, 1, sAccentColour, 0.3f);

        // The effect, and the skill or attribute it works on.
        addImage(mRoot, x + 16, y + 14, 48, effectIcon(*dialog.mMagicEffect));
        std::string name = escaped(dialog.mMagicEffect->mName);
        if (const ESM::Skill* skill = store().get<ESM::Skill>().search(effect.mSkill))
            name += ": " + escaped(skill->mName);
        else if (const ESM::Attribute* attribute = store().get<ESM::Attribute>().search(effect.mAttribute))
            name += ": " + escaped(attribute->mName);
        addText(mRoot, x + 76, y + 14, w - 92, 48, name, sFontLarge, sBrightColour);

        // Each setting, with its value and a bar for where the value sits in its range.
        const std::vector<Setting> all = settings();
        int ry = y + 80;
        for (std::size_t i = 0; i < all.size(); ++i)
        {
            const bool focused = static_cast<int>(i) == mSettingFocus;
            if (focused)
            {
                addRect(mRoot, x + 4, ry, w - 8, sSettingHeight - 4, sAccentColour, 0.16f);
                addRect(mRoot, x + 4, ry, 3, sSettingHeight - 4, sAccentColour);
            }
            std::string label;
            std::string value;
            const MyGUI::ScrollBar* slider = nullptr;
            switch (all[i])
            {
                case Setting::Range:
                    label = "#{sRange}";
                    value = rangeKey(effect.mRange);
                    break;
                case Setting::MagnitudeMin:
                    label = "#{sMagnitude}";
                    value = std::to_string(effect.mMagnMin);
                    slider = dialog.mMagnitudeMinSlider;
                    break;
                case Setting::MagnitudeMax:
                    label = "#{sMagnitude} #{sTo}";
                    value = std::to_string(effect.mMagnMax);
                    slider = dialog.mMagnitudeMaxSlider;
                    break;
                case Setting::Duration:
                    label = "#{sDuration}";
                    value = std::to_string(effect.mDuration);
                    slider = dialog.mDurationSlider;
                    break;
                case Setting::Area:
                    label = "#{sArea}";
                    value = std::to_string(effect.mArea);
                    slider = dialog.mAreaSlider;
                    break;
            }
            addText(mRoot, x + 20, ry + 2, w - 40, 28, label, sFontNormal, focused ? sActiveColour : sTextColour);
            addText(mRoot, x + 20, ry + 2, w - 40, 28, value, sFontLarge, sBrightColour,
                MyGUI::Align::Right | MyGUI::Align::VCenter);
            if (slider != nullptr && slider->getScrollRange() > 1)
            {
                const float fraction = static_cast<float>(slider->getScrollPosition()) / (slider->getScrollRange() - 1);
                addRect(mRoot, x + 20, ry + 36, w - 40, 6, MyGUI::Colour::Black, 0.6f);
                addRect(
                    mRoot, x + 20, ry + 36, std::max(2, static_cast<int>((w - 40) * fraction)), 6, sAccentColour, 0.8f);
            }
            ry += sSettingHeight;
        }

        // What the highlighted setting does, in the game's own words.
        std::vector<Hint> hints = { { SDL_CONTROLLER_BUTTON_DPAD_LEFT, "#{Interface:EnchantChange}" },
            { SDL_CONTROLLER_BUTTON_A, "#{Interface:OK}" } };
        if (dialog.mEditing)
            hints.emplace_back(SDL_CONTROLLER_BUTTON_X, "#{sDelete}");
        const int hintsTop = drawHints(mRoot, x + 16, w - 32, y + h, hints);
        if (!all.empty())
        {
            std::string help = "#{sRangeDes}";
            const Setting focused = all[std::clamp(mSettingFocus, 0, static_cast<int>(all.size()) - 1)];
            if (focused == Setting::MagnitudeMin || focused == Setting::MagnitudeMax)
                help = "#{sMagnitudeDes}";
            else if (focused == Setting::Duration)
                help = "#{sDurationDes}";
            else if (focused == Setting::Area)
                help = "#{sAreaDes}";
            // The game's description of the setting, and how to change it faster, as two paragraphs.
            std::vector<std::string> lines = wrapText(mRoot, localised(help), w - 32, sFontSmall, 3);
            if (focused != Setting::Range)
            {
                lines.emplace_back();
                for (std::string& line :
                    wrapText(mRoot, localised("#{Interface:EnchantFaster}"), w - 32, sFontSmall, 2))
                    lines.push_back(std::move(line));
            }
            int hy = hintsTop - 12 - static_cast<int>(lines.size()) * 21;
            for (const std::string& line : lines)
            {
                addText(mRoot, x + 16, hy, w - 32, 21, escaped(line), sFontSmall, sDimColour);
                hy += 21;
            }
        }
    }

    void EffectCraftPanel::drawSummary(int x, int y, int w, int h)
    {
        addRect(mRoot, x, y, w, h, sPanelColour, 0.9f);
        addRect(mRoot, x, y, w, 1, sAccentColour, 0.3f);

        // The name of the new item or spell.
        const std::string name = nameEdit()->getCaption().asUTF8();
        const std::string placeholder
            = mSpell != nullptr ? "#{Interface:SpellmakingNewSpell}" : "#{Interface:EnchantNewItem}";
        addText(mRoot, x + 16, y + 10, w - 32, 34, name.empty() ? placeholder : escaped(name), sFontLarge,
            name.empty() ? sDimColour : sBrightColour);

        const std::vector<Line> all = lines();
        const bool rightFocused = !mListFocused && !isEditing();
        int ry = y + 52;
        const auto highlight = [&](std::size_t i, int height) {
            if (rightFocused && static_cast<int>(i) == mLineFocus)
            {
                addRect(mRoot, x + 4, ry, w - 8, height - 2, sAccentColour, 0.16f);
                addRect(mRoot, x + 4, ry, 3, height - 2, sAccentColour);
            }
            return rightFocused && static_cast<int>(i) == mLineFocus;
        };
        const int labelW = 130;
        const int valueX = x + 16 + labelW;
        const int valueW = x + w - 16 - valueX;
        bool effectsHeading = false;
        for (std::size_t i = 0; i < all.size(); ++i)
        {
            const Line& line = all[i];
            if (line.mKind == Line::Kind::Effect || line.mKind == Line::Kind::AddEffect)
            {
                if (!effectsHeading)
                {
                    effectsHeading = true;
                    ry += 6;
                    addText(mRoot, x + 16, ry, w - 32, 22, "#{sEffects}", sFontSmall, sDimColour);
                    ry += 24;
                }
                if (ry + sEffectLineHeight > y + h - 190)
                    break;
                const bool focused = highlight(i, sEffectLineHeight);
                if (line.mKind == Line::Kind::AddEffect)
                    addText(mRoot, x + 20, ry, w - 40, sEffectLineHeight - 2, "+  #{Interface:EnchantAddEffect}",
                        sFontNormal, focused ? sActiveColour : sTextColour);
                else
                {
                    const ESM::ENAMstruct& effect = mEditor.mEffects[line.mEffect];
                    const ESM::MagicEffect* magicEffect = store().get<ESM::MagicEffect>().find(effect.mEffectID);
                    addImage(mRoot, x + 20, ry + 3, sEffectLineHeight - 6, effectIcon(*magicEffect));
                    addText(mRoot, x + 20 + sEffectLineHeight + 4, ry, w - 44 - sEffectLineHeight,
                        sEffectLineHeight - 2, escaped(describeEffect(effect, mEditor.mConstantEffect)), sFontNormal,
                        focused ? sActiveColour : sNewColour);
                }
                ry += sEffectLineHeight;
                continue;
            }

            const bool focused = highlight(i, sLineHeight);
            const MyGUI::Colour labelColour = focused ? sActiveColour : sTextColour;
            MWMechanics::Enchanting& enchanting = mEnchant->mEnchanting;
            if (line.mKind == Line::Kind::Item || line.mKind == Line::Kind::SoulGem)
            {
                const bool isItem = line.mKind == Line::Kind::Item;
                const MWWorld::Ptr ptr = isItem ? enchanting.getOldItem() : enchanting.getGem();
                addText(mRoot, x + 20, ry, labelW, sLineHeight - 2, isItem ? "#{sItem}" : "#{sSoulGem}", sFontNormal,
                    labelColour);
                if (ptr.isEmpty())
                    addText(mRoot, valueX, ry, valueW, sLineHeight - 2,
                        isItem ? "#{Interface:EnchantChooseItem}" : "#{Interface:EnchantChooseSoulGem}", sFontNormal,
                        sDimColour);
                else
                {
                    addIcon(mRoot, valueX, ry + 4, sLineHeight - 10, ptr, false);
                    std::string text = escaped(ptr.getClass().getName(ptr));
                    if (!isItem)
                        if (const ESM::Creature* soul = store().get<ESM::Creature>().search(ptr.getCellRef().getSoul()))
                            text += " (" + escaped(soul->mName) + ")";
                    addText(mRoot, valueX + sLineHeight - 4, ry, valueW - sLineHeight + 4, sLineHeight - 2, text,
                        sFontNormal, sBrightColour);
                }
            }
            else
            {
                addText(
                    mRoot, x + 20, ry, labelW, sLineHeight - 2, "#{OMWEngine:EnchantType}", sFontNormal, labelColour);
                addText(mRoot, valueX, ry, valueW, sLineHeight - 2,
                    enchanting.itemEmpty() ? "-" : castStyleKey(enchanting.getCastStyle()), sFontNormal,
                    enchanting.itemEmpty() ? sDimColour : sBrightColour);
            }
            ry += sLineHeight;
        }

        // From the bottom up: the button hints, what the highlighted line is for, and the numbers.
        std::vector<Hint> hints;
        if (rightFocused && mLineFocus < static_cast<int>(all.size()))
        {
            switch (all[mLineFocus].mKind)
            {
                case Line::Kind::Item:
                case Line::Kind::SoulGem:
                case Line::Kind::AddEffect:
                    hints.emplace_back(SDL_CONTROLLER_BUTTON_A, "#{Interface:EnchantChoose}");
                    break;
                case Line::Kind::CastType:
                    if (!mEnchant->mEnchanting.itemEmpty())
                        hints.emplace_back(SDL_CONTROLLER_BUTTON_A, "#{Interface:EnchantChange}");
                    break;
                case Line::Kind::Effect:
                    hints.emplace_back(SDL_CONTROLLER_BUTTON_A, "#{Interface:EnchantEdit}");
                    break;
            }
        }
        else if (mListFocused && !isEditing() && focusedEntry() != nullptr)
            hints.emplace_back(SDL_CONTROLLER_BUTTON_A,
                mPicker == Picker::Effects ? "#{Interface:EnchantAdd}" : "#{Interface:EnchantChoose}");
        if (!isEditing())
        {
            hints.emplace_back(
                SDL_CONTROLLER_BUTTON_X, isSelfEnchanting() ? "#{Interface:Create}" : "#{Interface:Buy}");
            hints.emplace_back(SDL_CONTROLLER_BUTTON_LEFTSTICK, "#{Interface:AlchemyRename}");
        }
        int by = drawHints(mRoot, x + 16, w - 32, y + h, hints) - 10;

        if (rightFocused && mLineFocus < static_cast<int>(all.size()))
        {
            std::string help;
            switch (all[mLineFocus].mKind)
            {
                case Line::Kind::Item:
                    help = "#{sEnchantmentHelp1}";
                    break;
                case Line::Kind::SoulGem:
                    help = "#{sEnchantmentHelp2}";
                    break;
                case Line::Kind::CastType:
                    help = "#{Interface:EnchantCastTypeHelp}";
                    break;
                default:
                    help = mSpell != nullptr ? "#{sSpellmakingHelp6}" : "#{sEnchantmentHelp10}";
                    break;
            }
            const std::vector<std::string> lines = wrapText(mRoot, localised(help), w - 32, sFontSmall, 3);
            by -= static_cast<int>(lines.size()) * 21;
            int hy = by;
            for (const std::string& line : lines)
            {
                addText(mRoot, x + 16, hy, w - 32, 21, escaped(line), sFontSmall, sDimColour);
                hy += 21;
            }
            by -= 10;
        }

        // The numbers, in two columns.
        std::vector<std::pair<std::string, std::pair<std::string, MyGUI::Colour>>> facts;
        if (mSpell != nullptr)
        {
            // The spellmaking window works these out whenever the effects change.
            const int price = MyGUI::utility::parseInt(mSpell->mPriceLabel->getCaption());
            facts.push_back({ "#{sEnchantmentMenu4}", { mSpell->mMagickaCost->getCaption().asUTF8(), sBrightColour } });
            facts.push_back(
                { "#{sSpellmakingMenu1}", { mSpell->mSuccessChance->getCaption().asUTF8() + "%", sBrightColour } });
            facts.push_back({ "#{sBarterDialog7}",
                { std::to_string(price) + " #{Interface:BarterGoldShort}",
                    price > playerGold() ? sWorseColour : sBrightColour } });
        }
        else
        {
            MWMechanics::Enchanting& enchanting = mEnchant->mEnchanting;
            const int points = static_cast<int>(enchanting.getEnchantPoints(false));
            const int maxPoints = enchanting.getMaxEnchantValue();
            facts.push_back({ "#{Interface:EnchantPoints}",
                { std::to_string(points) + " / " + std::to_string(maxPoints),
                    points > maxPoints ? sWorseColour : sBrightColour } });
            facts.push_back({ "#{sCastCost}", { std::to_string(enchanting.getEffectiveCastCost()), sBrightColour } });
            facts.push_back(
                { "#{Interface:MagicCharge}", { std::to_string(enchanting.getGemCharge()), sBrightColour } });
            if (!isSelfEnchanting())
            {
                const int price = enchanting.getEnchantPrice();
                facts.push_back({ "#{sBarterDialog7}",
                    { std::to_string(price) + " #{Interface:BarterGoldShort}",
                        price > playerGold() ? sWorseColour : sBrightColour } });
            }
            else if (Settings::game().mShowEnchantChance)
                facts.push_back({ "#{Interface:MagicChance}",
                    { std::to_string(std::clamp(enchanting.getEnchantChance(), 0, 100)) + "%", sBrightColour } });
        }
        const int rows = (static_cast<int>(facts.size()) + 1) / 2;
        by -= rows * 28;
        const int columnW = (w - 32 - 24) / 2;
        for (std::size_t i = 0; i < facts.size(); ++i)
        {
            const int fx = x + 16 + static_cast<int>(i % 2) * (columnW + 24);
            const int fy = by + static_cast<int>(i / 2) * 28;
            addText(mRoot, fx, fy, columnW, 26, facts[i].first, sFontNormal, sTextColour);
            addText(mRoot, fx, fy, columnW, 26, facts[i].second.first, sFontNormal, facts[i].second.second,
                MyGUI::Align::Right | MyGUI::Align::VCenter);
        }
        addRect(mRoot, x + 16, by - 8, w - 32, 1, sAccentColour, 0.2f);
    }

    void EffectCraftPanel::onFrame(float duration)
    {
        if (mRoot == nullptr || !mRoot->getVisible())
            return;
        const WindowBase* active = windowManager().getActiveControllerWindow();
        if (active != &mWindow && active != &mEditor.mAddEffectDialog)
        {
            mHeldDirection.reset();
            mAdjustTime = 0.f;
            return;
        }

        // While a value is being changed, holding left or right changes it faster and faster.
        const MWBase::InputManager& input = *MWBase::Environment::get().getInputManager();
        const bool sideways = input.isControllerButtonPressed(SDL_CONTROLLER_BUTTON_DPAD_LEFT)
            || input.isControllerButtonPressed(SDL_CONTROLLER_BUTTON_DPAD_RIGHT)
            || std::abs(input.getControllerAxisValue(SDL_CONTROLLER_AXIS_LEFTX)) > 0.5f;
        mAdjustTime = sideways ? mAdjustTime + duration : 0.f;

        int dx = 0;
        int dy = 0;
        if (!mHeldDirection.update(duration, dx, dy))
            return;
        if (isEditing())
        {
            if (dy != 0)
                mSettingFocus = std::clamp(mSettingFocus + dy, 0, static_cast<int>(settings().size()) - 1);
            else if (dx != 0)
            {
                const int step = mAdjustTime > 3.f ? 10 : (mAdjustTime > 1.2f ? 5 : 1);
                adjust(dx * step);
            }
        }
        else if (dx < 0 && !mListFocused)
            mListFocused = true;
        else if (dx > 0 && mListFocused)
            mListFocused = false;
        else if (dy != 0 && mListFocused)
            mListFocus = std::clamp(mListFocus + dy, 0, std::max(0, entryCount() - 1));
        else if (dy != 0)
            mLineFocus = std::clamp(mLineFocus + dy, 0, static_cast<int>(lines().size()) - 1);
        update();
    }

    bool EffectCraftPanel::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
    {
        switch (arg.button)
        {
            case SDL_CONTROLLER_BUTTON_A:
                if (mListFocused)
                    chooseEntry();
                else
                    chooseLine();
                break;
            case SDL_CONTROLLER_BUTTON_B:
                if (mListFocused && mPicker != Picker::Effects)
                {
                    setPicker(Picker::Effects);
                    mListFocused = false;
                    mPendingEffect = nullptr;
                }
                else if (mListFocused)
                    mListFocused = false;
                else
                {
                    if (mSpell != nullptr)
                        mSpell->onCancelButtonClicked(nullptr);
                    else
                        mEnchant->onCancelButtonClicked(nullptr);
                    return true;
                }
                break;
            case SDL_CONTROLLER_BUTTON_X:
                if (mSpell != nullptr)
                    mSpell->onBuyButtonClicked(nullptr);
                else
                    mEnchant->onBuyButtonClicked(nullptr);
                break;
            case SDL_CONTROLLER_BUTTON_LEFTSTICK:
                rename();
                return true;
            default:
                // The d-pad and the left stick are read every frame in onFrame, so they repeat while held.
                return true;
        }
        update();
        return true;
    }

    bool EffectCraftPanel::onEditorButton(const SDL_ControllerButtonEvent& arg)
    {
        EditEffectDialog& dialog = mEditor.mAddEffectDialog;
        switch (arg.button)
        {
            case SDL_CONTROLLER_BUTTON_A:
                dialog.onOkButtonClicked(nullptr);
                playSound("Menu Click");
                break;
            case SDL_CONTROLLER_BUTTON_B:
                dialog.onCancelButtonClicked(nullptr);
                break;
            case SDL_CONTROLLER_BUTTON_X:
                if (dialog.mEditing)
                    dialog.onDeleteButtonClicked(nullptr);
                break;
            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                adjust(-10);
                break;
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                adjust(10);
                break;
            default:
                // The d-pad and the left stick are read every frame in onFrame, so they repeat while held.
                return true;
        }
        if (!isEditing())
        {
            nameAfterFirstEffect();
            // Back on the right side, on the effect just added or edited.
            mListFocused = false;
            setPicker(Picker::Effects);
            const std::vector<Line> all = lines();
            for (std::size_t i = 0; i < all.size(); ++i)
                if (all[i].mKind == Line::Kind::Effect && all[i].mEffect == mEditor.mSelectedEffect)
                    mLineFocus = static_cast<int>(i);
            mLineFocus = std::clamp(mLineFocus, 0, static_cast<int>(all.size()) - 1);
        }
        update();
        return true;
    }

    void EffectCraftPanel::chooseEntry()
    {
        const Entry* entry = focusedEntry();
        if (entry == nullptr)
            return;
        switch (mPicker)
        {
            case Picker::Effects:
                addEffect(entry->mId);
                break;
            case Picker::Items:
                pickItem(entry->mItem);
                break;
            case Picker::SoulGems:
                pickSoulGem(entry->mItem);
                break;
            case Picker::Skills:
            case Picker::Attributes:
            {
                if (mPendingEffect == nullptr)
                    return;
                const ESM::RefId id = entry->mId;
                const bool skill = mPicker == Picker::Skills;
                setPicker(Picker::Effects);
                EditEffectDialog& dialog = mEditor.mAddEffectDialog;
                dialog.newEffect(mPendingEffect);
                if (skill)
                    dialog.setSkill(id);
                else
                    dialog.setAttribute(id);
                mPendingEffect = nullptr;
                mSettingFocus = 0;
                playSound("Menu Click");
                break;
            }
        }
    }

    void EffectCraftPanel::chooseLine()
    {
        const std::vector<Line> all = lines();
        if (mLineFocus >= static_cast<int>(all.size()))
            return;
        const Line& line = all[mLineFocus];
        switch (line.mKind)
        {
            case Line::Kind::Item:
                setPicker(Picker::Items);
                mListFocused = true;
                break;
            case Line::Kind::SoulGem:
                setPicker(Picker::SoulGems);
                mListFocused = true;
                break;
            case Line::Kind::CastType:
                if (mEnchant->mEnchanting.itemEmpty())
                    return;
                mEnchant->onTypeButtonClicked(nullptr);
                break;
            case Line::Kind::Effect:
                mEditor.mSelectedEffect = line.mEffect;
                mEditor.mAddEffectDialog.editEffect(mEditor.mEffects[line.mEffect]);
                mEditor.mAddEffectDialog.setVisible(true);
                mSettingFocus = 0;
                break;
            case Line::Kind::AddEffect:
                setPicker(Picker::Effects);
                mListFocused = true;
                break;
        }
        playSound("Menu Click");
    }

    void EffectCraftPanel::addEffect(const ESM::RefId& id)
    {
        // The same checks as the enchanting window makes when an effect is clicked.
        if (mEditor.mEffects.size() >= sMaxEffects)
        {
            windowManager().messageBox("#{sNotifyMessage28}");
            return;
        }
        const ESM::MagicEffect* effect = store().get<ESM::MagicEffect>().find(id);
        const bool constant = mEditor.mConstantEffect;
        const bool allowSelf = (effect->mData.mFlags & ESM::MagicEffect::CastSelf) != 0 || constant;
        const bool allowTouch = (effect->mData.mFlags & ESM::MagicEffect::CastTouch) && !constant;
        const bool allowTarget = (effect->mData.mFlags & ESM::MagicEffect::CastTarget) && !constant;
        if (!allowSelf && !allowTouch && !allowTarget)
            return;

        // An effect on a skill or an attribute asks which one first, in the list.
        if (effect->mData.mFlags & (ESM::MagicEffect::TargetSkill | ESM::MagicEffect::TargetAttribute))
        {
            mPendingEffect = effect;
            setPicker(effect->mData.mFlags & ESM::MagicEffect::TargetSkill ? Picker::Skills : Picker::Attributes);
            playSound("Menu Click");
            return;
        }
        for (const ESM::ENAMstruct& added : mEditor.mEffects)
        {
            if (added.mEffectID == id)
            {
                windowManager().messageBox("#{sOnetypeEffectMessage}");
                return;
            }
        }
        mEditor.mAddEffectDialog.newEffect(effect);
        mSettingFocus = 0;
        playSound("Menu Click");
    }

    void EffectCraftPanel::pickItem(const MWWorld::Ptr& item)
    {
        // The same steps as the enchanting window takes when an item is picked.
        mEnchant->setItem(item);
        windowManager().playSound(item.getClass().getDownSoundId(item));
        mEnchant->mEnchanting.nextCastStyle();
        mEnchant->updateLabels();
        mEditor.updateEffectsView();
        setPicker(Picker::Effects);
        mListFocused = false;
        focusNextLine();
    }

    void EffectCraftPanel::pickSoulGem(const MWWorld::Ptr& gem)
    {
        // The same steps as the enchanting window takes when a soul gem is picked.
        mEnchant->mEnchanting.setSoulGem(gem);
        if (mEnchant->mEnchanting.getGemCharge() == 0)
        {
            windowManager().messageBox("#{sNotifyMessage32}");
            return;
        }
        mEnchant->setSoulGem(gem);
        windowManager().playSound(gem.getClass().getDownSoundId(gem));
        mEnchant->updateLabels();
        setPicker(Picker::Effects);
        mListFocused = false;
        focusNextLine();
    }

    void EffectCraftPanel::adjust(int delta)
    {
        EditEffectDialog& dialog = mEditor.mAddEffectDialog;
        const std::vector<Setting> all = settings();
        if (all.empty() || delta == 0)
            return;
        const Setting setting = all[std::clamp(mSettingFocus, 0, static_cast<int>(all.size()) - 1)];
        const auto move = [&](MyGUI::ScrollBar* slider, int lowest) {
            const int highest = static_cast<int>(slider->getScrollRange()) - 1;
            const int position = std::clamp(static_cast<int>(slider->getScrollPosition()) + delta, lowest, highest);
            slider->setScrollPosition(position);
            return static_cast<size_t>(position);
        };
        switch (setting)
        {
            case Setting::Range:
                dialog.onRangeButtonClicked(nullptr);
                mSettingFocus = 0;
                break;
            case Setting::MagnitudeMin:
                dialog.onMagnitudeMinChanged(dialog.mMagnitudeMinSlider, move(dialog.mMagnitudeMinSlider, 0));
                break;
            case Setting::MagnitudeMax:
                dialog.onMagnitudeMaxChanged(dialog.mMagnitudeMaxSlider,
                    move(
                        dialog.mMagnitudeMaxSlider, static_cast<int>(dialog.mMagnitudeMinSlider->getScrollPosition())));
                break;
            case Setting::Duration:
                dialog.onDurationChanged(dialog.mDurationSlider, move(dialog.mDurationSlider, 0));
                break;
            case Setting::Area:
                dialog.onAreaChanged(dialog.mAreaSlider, move(dialog.mAreaSlider, 0));
                break;
        }
        playSound("Menu Click");
    }

    void EffectCraftPanel::rename()
    {
        mRenameDialog = std::make_unique<TextInputDialog>();
        mRenameDialog->setTextLabel(windowManager().getGameSettingString("sName", "Name"));
        mRenameDialog->setTextInput(nameEdit()->getCaption().asUTF8());
        mRenameDialog->setNextButtonShow(false);
        mRenameDialog->eventDone += MyGUI::newDelegate(this, &EffectCraftPanel::onRenameDone);
        mRenameDialog->setVisible(true);
    }

    void EffectCraftPanel::onRenameDone(WindowBase* /*sender*/)
    {
        const std::string name = mRenameDialog->getTextInput();
        if (!name.empty())
            nameEdit()->setCaption(MyGUI::UString(name));
        windowManager().removeDialog(std::move(mRenameDialog));
        update();
    }
}
