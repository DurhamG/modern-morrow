#include "alchemypanel.hpp"

#include <algorithm>
#include <set>

#include <MyGUI_EditBox.h>
#include <MyGUI_Gui.h>
#include <MyGUI_ImageBox.h>

#include <components/esm3/loadappa.hpp>
#include <components/esm3/loadingr.hpp>
#include <components/esm3/loadmgef.hpp>
#include <components/esm3/loadskil.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/widgets/numericeditbox.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/alchemy.hpp"
#include "../mwmechanics/magiceffects.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/containerstore.hpp"
#include "../mwworld/esmstore.hpp"

#include "alchemywindow.hpp"
#include "textinput.hpp"
#include "widgets.hpp"

namespace MWGui
{
    namespace
    {
        using namespace ControllerUi;

        constexpr int sHeaderHeight = 30;
        constexpr int sRowHeight = 58;
        constexpr int sSlotHeight = 36;
        constexpr int sEffectHeight = 32;

        MWWorld::Ptr player()
        {
            return MWMechanics::getPlayer();
        }

        float alchemySkill()
        {
            const MWWorld::Ptr actor = player();
            return actor.getClass().getSkill(actor, ESM::Skill::Alchemy);
        }

        std::string effectIcon(const ESM::RefId& effectId)
        {
            const ESM::MagicEffect* effect
                = MWBase::Environment::get().getESMStore()->get<ESM::MagicEffect>().find(effectId);
            return std::string(Misc::ResourceHelpers::correctBigIconPath(
                effect->mIcon.getNormalized(), *MWBase::Environment::get().getResourceSystem()->getVFS())
                    .value());
        }

        // A potion's effect written out, such as "Restore Fatigue" or "Fortify Strength".
        std::string describePotionEffect(const MWMechanics::EffectKey& key)
        {
            Widgets::SpellEffectParams params;
            params.mEffectID = key.mId;
            const ESM::MagicEffect* effect
                = MWBase::Environment::get().getESMStore()->get<ESM::MagicEffect>().find(key.mId);
            if (effect->mData.mFlags & ESM::MagicEffect::TargetSkill)
                params.mSkill = key.mArg;
            else if (effect->mData.mFlags & ESM::MagicEffect::TargetAttribute)
                params.mAttribute = key.mArg;
            params.mIsConstant = true;
            params.mNoTarget = true;
            params.mNoMagnitude = true;
            return localised(Widgets::describeSpellEffect(params));
        }

        std::string joined(const std::vector<std::string>& parts)
        {
            std::string text;
            for (const std::string& part : parts)
                text += (text.empty() ? "" : ", ") + part;
            return text;
        }

        void playSound(std::string_view sound)
        {
            MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId(sound));
        }
    }

    AlchemyPanel::AlchemyPanel(AlchemyWindow& window)
        : mWindow(window)
    {
    }

    AlchemyPanel::~AlchemyPanel()
    {
        if (mRoot != nullptr)
            MyGUI::Gui::getInstance().destroyWidget(mRoot);
    }

    void AlchemyPanel::show()
    {
        if (mRoot == nullptr)
            mRoot = createPanelRoot("ControllerMenus");
        if (mRoot == nullptr)
            return;
        mRoot->setVisible(true);
        mFocus = 0;
        mScroll = 0;
        mHeldDirection.reset();
        update();
    }

    void AlchemyPanel::hide()
    {
        if (mRoot != nullptr)
            mRoot->setVisible(false);
        mRows.clear();
    }

    int AlchemyPanel::potionSlotOf(const MWWorld::Ptr& item) const
    {
        int slot = 0;
        for (auto it = mWindow.mAlchemy->beginIngredients(); it != mWindow.mAlchemy->endIngredients(); ++it, ++slot)
            if (!it->isEmpty() && *it == item)
                return slot;
        return -1;
    }

    int AlchemyPanel::ingredientCount() const
    {
        return static_cast<int>(std::count_if(mWindow.mAlchemy->beginIngredients(), mWindow.mAlchemy->endIngredients(),
            [](const MWWorld::Ptr& item) { return !item.isEmpty(); }));
    }

    const AlchemyPanel::Row* AlchemyPanel::focusedRow() const
    {
        int index = 0;
        for (const Row& row : mRows)
        {
            if (row.mItem.isEmpty())
                continue;
            if (index++ == mFocus)
                return &row;
        }
        return nullptr;
    }

    void AlchemyPanel::readIngredients()
    {
        // The selection stays on the same ingredient when it moves between the groups.
        const Row* focused = focusedRow();
        const MWWorld::Ptr focusedItem = focused != nullptr ? focused->mItem : MWWorld::Ptr();

        // The effects the player knows on the potion's ingredients. An ingredient shares an effect with the potion
        // when one of its own known effects is among them. Effects the player doesn't know yet don't count, so the
        // list never gives them away.
        const float skill = alchemySkill();
        std::set<std::string> potionEffects;
        for (auto it = mWindow.mAlchemy->beginIngredients(); it != mWindow.mAlchemy->endIngredients(); ++it)
            if (!it->isEmpty())
                for (std::string& effect : MWMechanics::Alchemy::effectsDescription(*it, skill))
                    potionEffects.insert(std::move(effect));

        std::vector<Row> inPotion;
        std::vector<Row> sharing;
        std::vector<Row> others;
        const MWWorld::Ptr actor = player();
        MWWorld::ContainerStore& store = actor.getClass().getContainerStore(actor);
        for (auto it = store.begin(MWWorld::ContainerStore::Type_Ingredient); it != store.end(); ++it)
        {
            Row row;
            row.mItem = *it;
            row.mEffects = MWMechanics::Alchemy::effectsDescription(*it, skill);
            row.mInPotion = potionSlotOf(*it) >= 0;
            const bool shares = std::any_of(row.mEffects.begin(), row.mEffects.end(),
                [&](const std::string& effect) { return potionEffects.count(effect) > 0; });
            if (row.mInPotion)
                inPotion.push_back(std::move(row));
            else if (shares)
                sharing.push_back(std::move(row));
            else
                others.push_back(std::move(row));
        }

        const auto byName = [](const Row& a, const Row& b) {
            return a.mItem.getClass().getName(a.mItem) < b.mItem.getClass().getName(b.mItem);
        };
        // The groups only get headers once the potion holds an ingredient. Before that, the list is one group.
        const bool grouped = !inPotion.empty();
        mRows.clear();
        const auto addGroup = [&](const std::string& header, std::vector<Row>& group) {
            if (group.empty())
                return;
            std::stable_sort(group.begin(), group.end(), byName);
            if (grouped)
                mRows.push_back(Row{ .mHeader = header });
            for (Row& row : group)
                mRows.push_back(std::move(row));
        };
        addGroup("#{Interface:AlchemyInPotion}", inPotion);
        addGroup("#{Interface:AlchemySharing}", sharing);
        addGroup("#{Interface:AlchemyOther}", others);

        int index = 0;
        for (const Row& row : mRows)
        {
            if (row.mItem.isEmpty())
                continue;
            if (!focusedItem.isEmpty() && row.mItem == focusedItem)
                mFocus = index;
            ++index;
        }
        mFocus = std::clamp(mFocus, 0, std::max(0, index - 1));
    }

    void AlchemyPanel::update()
    {
        if (mRoot == nullptr || !mRoot->getVisible())
            return;
        // The alchemy rules only count how many potions the ingredients make once the potion has a name. The
        // window gives it one when it brews, so the panel gives it the same one to show the count beforehand.
        mWindow.mAlchemy->setPotionName(mWindow.mNameEdit->getCaption());
        readIngredients();
        clearPanel(mRoot);
        const MyGUI::IntSize size = mRoot->getSize();
        addRect(mRoot, 0, 0, size.width, size.height, MyGUI::Colour::Black)->setNeedMouseFocus(true);

        addText(mRoot, sMargin, 20, size.width - 2 * sMargin, 40, "#{sSkillAlchemy}", sFontTitle, sBrightColour);
        addText(mRoot, sMargin, 20, size.width - 2 * sMargin, 40,
            "#{Interface:AlchemySkill}  " + std::to_string(static_cast<int>(alchemySkill())), sFontSmall, sDimColour,
            MyGUI::Align::Right | MyGUI::Align::VCenter);
        addRect(mRoot, sMargin, 66, size.width - 2 * sMargin, 1, sAccentColour, 0.2f);

        const int top = 82;
        const int bottom = size.height - sContentBottomMargin;
        const int listW = (size.width - 2 * sMargin - 16) / 2;
        drawList(sMargin, top, listW, bottom - top);
        const int px = sMargin + listW + 16;
        drawPotion(px, top, size.width - sMargin - px, bottom - top);
    }

    void AlchemyPanel::drawList(int x, int y, int w, int h)
    {
        addRect(mRoot, x, y, w, h, sPanelColour, 0.9f);
        addText(mRoot, x + 16, y + 8, w - 32, 30, "#{sIngredients}", sFontLarge, sBrightColour);

        int count = 0;
        for (const Row& row : mRows)
            count += row.mItem.isEmpty() ? 0 : 1;
        const int listTop = y + 44;
        const int listBottom = y + h - 6;
        if (count == 0)
        {
            addText(mRoot, x + 16, listTop, w - 32, 30, "#{Interface:AlchemyNoIngredients}", sFontNormal, sDimColour);
            return;
        }

        // Scroll so the focused row, and the header above it, are in view.
        int focusRow = 0;
        for (int i = 0, index = 0; i < static_cast<int>(mRows.size()); ++i)
        {
            if (mRows[i].mItem.isEmpty())
                continue;
            if (index++ == mFocus)
            {
                focusRow = i;
                break;
            }
        }
        const auto rowHeight = [&](int i) { return mRows[i].mItem.isEmpty() ? sHeaderHeight : sRowHeight; };
        mScroll = std::min(mScroll, static_cast<int>(mRows.size()) - 1);
        if (focusRow < mScroll)
            mScroll = focusRow > 0 && mRows[focusRow - 1].mItem.isEmpty() ? focusRow - 1 : focusRow;
        while (true)
        {
            int bottomOfFocus = listTop;
            for (int i = mScroll; i <= focusRow; ++i)
                bottomOfFocus += rowHeight(i);
            if (bottomOfFocus <= listBottom || mScroll >= focusRow)
                break;
            ++mScroll;
        }

        int rowY = listTop;
        int index = 0;
        for (int i = 0; i < mScroll; ++i)
            index += mRows[i].mItem.isEmpty() ? 0 : 1;
        for (int i = mScroll; i < static_cast<int>(mRows.size()); ++i)
        {
            const Row& row = mRows[i];
            if (rowY + rowHeight(i) > listBottom)
                break;
            if (row.mItem.isEmpty())
            {
                addText(mRoot, x + 12, rowY + 4, w - 24, 24, row.mHeader, sFontSmall, sBrightColour);
                addRect(mRoot, x + 12, rowY + 27, w - 24, 1, sAccentColour, 0.25f);
                rowY += sHeaderHeight;
                continue;
            }
            const bool focused = index == mFocus;
            const int rx = x + 4;
            const int rw = w - 8;
            if (focused)
            {
                addRect(mRoot, rx, rowY, rw, sRowHeight - 2, sAccentColour, 0.16f);
                addRect(mRoot, rx, rowY, 3, sRowHeight - 2, sAccentColour);
            }
            addIcon(mRoot, rx + 10, rowY + 8, 40, row.mItem, false);
            const int textX = rx + 62;
            const int textW = rw - 62 - 12;
            std::string name = escaped(row.mItem.getClass().getName(row.mItem));
            if (row.mItem.getCellRef().getCount() > 1)
                name += " (" + std::to_string(row.mItem.getCellRef().getCount()) + ")";
            addText(mRoot, textX, rowY + 4, textW, 26, name, sFontNormal,
                focused ? sActiveColour : (row.mInPotion ? sNewColour : sBrightColour));
            const std::string effects
                = row.mEffects.empty() ? "#{Interface:AlchemyNoKnownEffects}" : escaped(joined(row.mEffects));
            addText(mRoot, textX, rowY + 30, textW, 22, effects, sFontSmall, sDimColour);
            rowY += sRowHeight;
            ++index;
        }
    }

    void AlchemyPanel::drawPotion(int x, int y, int w, int h)
    {
        addRect(mRoot, x, y, w, h, sPanelColour, 0.9f);
        addRect(mRoot, x, y, w, 1, sAccentColour, 0.3f);
        MWMechanics::Alchemy& alchemy = *mWindow.mAlchemy;
        const std::vector<MWMechanics::EffectKey> effects = alchemy.listEffects();
        const int ingredients = ingredientCount();
        const bool canBrew = ingredients >= 2 && !effects.empty();

        // The potion's name, or what to do first.
        int ry = y + 12;
        if (canBrew)
            addText(mRoot, x + 16, ry, w - 32, 34, escaped(mWindow.mNameEdit->getCaption().asUTF8()), sFontLarge,
                sBrightColour);
        else
        {
            const std::string text = ingredients >= 2 ? "#{Interface:AlchemyNoEffects}" : "#{Interface:AlchemyStart}";
            for (const std::string& line : wrapText(mRoot, localised(text), w - 32, sFontNormal, 2))
            {
                addText(mRoot, x + 16, ry, w - 32, 26, escaped(line), sFontNormal,
                    ingredients >= 2 ? sWorseColour : sDimColour);
                ry += 26;
            }
            ry -= 26;
        }
        ry += 44;

        // The four ingredient slots.
        addText(mRoot, x + 16, ry, w - 32, 22, "#{sIngredients}", sFontSmall, sDimColour);
        ry += 26;
        for (auto it = alchemy.beginIngredients(); it != alchemy.endIngredients(); ++it)
        {
            if (it->isEmpty())
            {
                addRect(mRoot, x + 16, ry + 2, sSlotHeight - 6, sSlotHeight - 6, sIconBackColour, 0.9f);
                addText(mRoot, x + 16 + sSlotHeight + 4, ry, w - 32 - sSlotHeight, sSlotHeight - 2,
                    "#{Interface:AlchemyEmptySlot}", sFontNormal, sDimColour);
            }
            else
            {
                addIcon(mRoot, x + 16, ry + 2, sSlotHeight - 6, *it, false);
                addText(mRoot, x + 16 + sSlotHeight + 4, ry, w - 32 - sSlotHeight, sSlotHeight - 2,
                    escaped(it->getClass().getName(*it)), sFontNormal, sBrightColour);
            }
            ry += sSlotHeight;
        }
        ry += 10;

        // The potion's effects. The ones the player doesn't know yet show as unknown, as in the alchemy window.
        addText(mRoot, x + 16, ry, w - 32, 22, "#{sCreatedEffects}", sFontSmall, sDimColour);
        ry += 26;
        const MWWorld::Ptr actor = player();
        for (std::size_t i = 0; i < effects.size(); ++i)
        {
            if (MWMechanics::Alchemy::knownEffect(i, actor))
            {
                addImage(mRoot, x + 16, ry + 2, sEffectHeight - 6, effectIcon(effects[i].mId));
                addText(mRoot, x + 16 + sEffectHeight + 4, ry, w - 32 - sEffectHeight, sEffectHeight - 2,
                    escaped(describePotionEffect(effects[i])), sFontNormal, sNewColour);
            }
            else
            {
                addRect(mRoot, x + 16, ry + 2, sEffectHeight - 6, sEffectHeight - 6, sIconBackColour, 0.9f);
                addText(mRoot, x + 16 + sEffectHeight + 4, ry, w - 32 - sEffectHeight, sEffectHeight - 2,
                    "#{Interface:AlchemyUnknownEffect}", sFontNormal, sDimColour);
            }
            ry += sEffectHeight;
        }
        if (effects.empty())
            addText(mRoot, x + 16, ry, w - 32, sEffectHeight - 2, "-", sFontNormal, sDimColour);

        // From the bottom up: the button hints, how many to brew, and the tools.
        std::vector<Hint> hints;
        if (const Row* row = focusedRow())
            hints.emplace_back(
                SDL_CONTROLLER_BUTTON_A, row->mInPotion ? "#{Interface:AlchemyRemove}" : "#{Interface:AlchemyAdd}");
        if (canBrew)
        {
            hints.emplace_back(SDL_CONTROLLER_BUTTON_X, "#{Interface:AlchemyBrew}");
            hints.emplace_back(SDL_CONTROLLER_BUTTON_LEFTSTICK, "#{Interface:AlchemyRename}");
        }
        int by = drawHints(mRoot, x + 16, w - 32, y + h, hints) - 12;

        if (canBrew)
        {
            const int most = alchemy.countPotionsToBrew();
            const int brew = std::min(mWindow.mBrewCountEdit->getValue(), most);
            by -= 34;
            addText(mRoot, x + 16, by, w - 32, 30, "#{Interface:AlchemyCount}", sFontNormal, sTextColour);
            MyGUI::TextBox* value
                = addText(mRoot, x + 16, by, w - 32 - 34, 30, std::to_string(brew) + " / " + std::to_string(most),
                    sFontLarge, sBrightColour, MyGUI::Align::Right | MyGUI::Align::VCenter);
            const int valueLeft = x + w - 16 - 34 - value->getTextSize().width;
            addGlyph(mRoot, valueLeft - 34, by + 2, 26, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
            addGlyph(mRoot, x + w - 16 - 26, by + 2, 26, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
            by -= 8;
        }

        std::vector<std::string> tools;
        for (auto it = alchemy.beginTools(); it != alchemy.endTools(); ++it)
            if (!it->isEmpty())
                tools.push_back(std::string(it->getClass().getName(*it)));
        const std::vector<std::string> lines = wrapText(mRoot, joined(tools), w - 32, sFontSmall, 3);
        by -= static_cast<int>(lines.size()) * 22;
        addText(mRoot, x + 16, by - 24, w - 32, 22, "#{sApparatus}", sFontSmall, sDimColour);
        for (const std::string& line : lines)
        {
            addText(mRoot, x + 16, by, w - 32, 22, escaped(line), sFontSmall, sTextColour);
            by += 22;
        }
    }

    void AlchemyPanel::onFrame(float duration)
    {
        if (mRoot == nullptr || !mRoot->getVisible())
            return;
        if (MWBase::Environment::get().getWindowManager()->getActiveControllerWindow() != &mWindow)
        {
            mHeldDirection.reset();
            return;
        }
        int dx = 0;
        int dy = 0;
        if (mHeldDirection.update(duration, dx, dy) && dy != 0)
        {
            mFocus = std::max(0, mFocus + dy);
            update();
        }
    }

    bool AlchemyPanel::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
    {
        switch (arg.button)
        {
            case SDL_CONTROLLER_BUTTON_A:
                toggleFocused();
                break;
            case SDL_CONTROLLER_BUTTON_B:
                mWindow.onCancelButtonClicked(nullptr);
                break;
            case SDL_CONTROLLER_BUTTON_X:
                brew();
                break;
            case SDL_CONTROLLER_BUTTON_LEFTSTICK:
                rename();
                break;
            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                mWindow.onDecreaseButtonTriggered();
                playSound("Menu Click");
                update();
                break;
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                if (mWindow.mBrewCountEdit->getValue() < mWindow.mAlchemy->countPotionsToBrew())
                    mWindow.onIncreaseButtonTriggered();
                playSound("Menu Click");
                update();
                break;
            default:
                // The d-pad and the left stick are read every frame in onFrame, so they repeat while held.
                break;
        }
        return true;
    }

    void AlchemyPanel::toggleFocused()
    {
        const Row* row = focusedRow();
        if (row == nullptr)
            return;
        const MWWorld::Ptr item = row->mItem;
        if (const int slot = potionSlotOf(item); slot >= 0)
        {
            mWindow.mAlchemy->removeIngredient(slot);
            MWBase::Environment::get().getWindowManager()->playSound(item.getClass().getDownSoundId(item));
        }
        else if (mWindow.mAlchemy->addIngredient(item) >= 0)
            MWBase::Environment::get().getWindowManager()->playSound(item.getClass().getUpSoundId(item));
        else
        {
            if (ingredientCount() >= 4)
                MWBase::Environment::get().getWindowManager()->messageBox("#{Interface:AlchemyFull}");
            return;
        }
        mWindow.update();
        update();
    }

    void AlchemyPanel::brew()
    {
        if (ingredientCount() < 2 || mWindow.mAlchemy->listEffects().empty())
            return;
        mWindow.onCreateButtonClicked(nullptr);
        update();
    }

    void AlchemyPanel::rename()
    {
        if (ingredientCount() < 2 || mWindow.mAlchemy->listEffects().empty())
            return;
        MWBase::WindowManager* winMgr = MWBase::Environment::get().getWindowManager();
        mRenameDialog = std::make_unique<TextInputDialog>();
        mRenameDialog->setTextLabel(winMgr->getGameSettingString("sName", "Name"));
        mRenameDialog->setTextInput(mWindow.mNameEdit->getCaption().asUTF8());
        mRenameDialog->setNextButtonShow(false);
        mRenameDialog->eventDone += MyGUI::newDelegate(this, &AlchemyPanel::onRenameDone);
        mRenameDialog->setVisible(true);
    }

    void AlchemyPanel::onRenameDone(WindowBase* /*sender*/)
    {
        const std::string name = mRenameDialog->getTextInput();
        if (!name.empty())
            mWindow.mNameEdit->setCaption(MyGUI::UString(name));
        MWBase::Environment::get().getWindowManager()->removeDialog(std::move(mRenameDialog));
        update();
    }
}
