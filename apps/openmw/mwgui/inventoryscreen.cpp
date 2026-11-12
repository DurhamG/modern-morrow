#include "inventoryscreen.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

#include <MyGUI_ImageBox.h>
#include <MyGUI_TextBox.h>

#include <components/esm3/loadalch.hpp>
#include <components/esm3/loadappa.hpp>
#include <components/esm3/loadarmo.hpp>
#include <components/esm3/loadbook.hpp>
#include <components/esm3/loadclot.hpp>
#include <components/esm3/loadingr.hpp>
#include <components/esm3/loadlock.hpp>
#include <components/esm3/loadmisc.hpp>
#include <components/esm3/loadprob.hpp>
#include <components/esm3/loadrepa.hpp>
#include <components/esm3/loadskil.hpp>
#include <components/esm3/loadweap.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/luamanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwmechanics/actorutil.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/inventorystore.hpp"
#include "../mwworld/player.hpp"

#include "controllerscreen.hpp"
#include "countdialog.hpp"
#include "formatting.hpp"
#include "inventoryitemmodel.hpp"
#include "itemdetails.hpp"
#include "quickkeyassigner.hpp"
#include "worlditemmodel.hpp"

namespace MWGui
{
    namespace
    {
        using namespace ControllerUi;
        using namespace ItemDetails;
        using Inventory = MWWorld::InventoryStore;

        constexpr int sRowHeight = 42;
        constexpr int sHeaderRowHeight = 28;

        // How far a book scrolls per second with the right stick pushed all the way, in canvas units.
        constexpr float sBookScrollSpeed = 600.f;

        // Book text is dark, as the books set it, so it sits on a page of this colour.
        const MyGUI::Colour sPageColour = MyGUI::Colour::parse("#d8c7a0");

        struct AreaDef
        {
            std::string_view mName;
            std::vector<int> mSlots;
            // Which slot's item the overview shows, from the outermost layer in.
            std::vector<int> mTopOrder;
            // Whether the item list has a group for each slot.
            bool mGroupBySlot;
        };

        // The left column of the overview holds the first six areas, the right column the rest.
        const std::array<AreaDef, 12> sAreas = { {
            { "Head", { Inventory::Slot_Helmet }, { Inventory::Slot_Helmet }, false },
            { "Shoulders", { Inventory::Slot_LeftPauldron, Inventory::Slot_RightPauldron },
                { Inventory::Slot_LeftPauldron, Inventory::Slot_RightPauldron }, true },
            { "Chest", { Inventory::Slot_Cuirass, Inventory::Slot_Shirt, Inventory::Slot_Robe },
                { Inventory::Slot_Cuirass, Inventory::Slot_Robe, Inventory::Slot_Shirt }, true },
            { "Arms", { Inventory::Slot_LeftGauntlet, Inventory::Slot_RightGauntlet },
                { Inventory::Slot_LeftGauntlet, Inventory::Slot_RightGauntlet }, true },
            { "Legs", { Inventory::Slot_Greaves, Inventory::Slot_Pants, Inventory::Slot_Skirt },
                { Inventory::Slot_Greaves, Inventory::Slot_Skirt, Inventory::Slot_Pants }, true },
            { "Feet", { Inventory::Slot_Boots }, { Inventory::Slot_Boots }, false },
            { "Weapon", { Inventory::Slot_CarriedRight }, { Inventory::Slot_CarriedRight }, false },
            { "OffHand", { Inventory::Slot_CarriedLeft }, { Inventory::Slot_CarriedLeft }, false },
            { "Ammunition", { Inventory::Slot_Ammunition }, { Inventory::Slot_Ammunition }, false },
            { "Amulet", { Inventory::Slot_Amulet }, { Inventory::Slot_Amulet }, false },
            { "Rings", { Inventory::Slot_LeftRing, Inventory::Slot_RightRing },
                { Inventory::Slot_LeftRing, Inventory::Slot_RightRing }, false },
            { "Belt", { Inventory::Slot_Belt }, { Inventory::Slot_Belt }, false },
        } };
        constexpr int sAreasPerColumn = 6;
        constexpr int sWeaponArea = 6;
        constexpr int sAmmunitionArea = 8;
        constexpr int sFirstJewelleryArea = 9;

        std::string slotKey(int slot)
        {
            static const std::array<std::string_view, Inventory::Slots> names = { "Helmet", "Cuirass", "Greaves",
                "LeftPauldron", "RightPauldron", "LeftGauntlet", "RightGauntlet", "Boots", "Shirt", "Pants", "Skirt",
                "Robe", "LeftRing", "RightRing", "Amulet", "Belt", "Weapon", "OffHand", "Ammunition" };
            return "#{Interface:InventorySlot" + std::string(names[slot]) + "}";
        }

        std::string areaKey(int area)
        {
            return "#{Interface:InventoryArea" + std::string(sAreas[area].mName) + "}";
        }

        MWWorld::Ptr player()
        {
            return MWMechanics::getPlayer();
        }

        MWWorld::InventoryStore& playerInventory()
        {
            const MWWorld::Ptr actor = player();
            return actor.getClass().getInventoryStore(actor);
        }

        const MWWorld::ESMStore& store()
        {
            return *MWBase::Environment::get().getESMStore();
        }

        enum class Stat
        {
            Armor,
            Damage,
            Speed,
            Weight,
            // The weight of the whole stack: one item's weight times the count.
            TotalWeight,
            Value,
            Count,
            Effect,
            Quality,
            Uses,
            Name,
        };

        std::string statLabel(Stat stat)
        {
            static const std::array<std::string_view, 11> names = { "Armor", "Damage", "Speed", "Weight", "TotalWeight",
                "Value", "Count", "Effect", "Quality", "Uses", "Name" };
            return "#{Interface:InventoryColumn" + std::string(names[static_cast<int>(stat)]) + "}";
        }

        struct Column
        {
            Stat mStat;
            int mWidth;
        };

        std::string statText(Stat stat, const ItemStack& item)
        {
            const MWWorld::Ptr& ptr = item.mBase;
            const unsigned int type = ptr.getType();
            switch (stat)
            {
                case Stat::Armor:
                    if (type != ESM::Armor::sRecordId)
                        return "-";
                    return formatNumber(armorRating(ptr));
                case Stat::Damage:
                {
                    if (type != ESM::Weapon::sRecordId)
                        return "-";
                    const auto [low, high] = bestAttack(*ptr.get<ESM::Weapon>()->mBase);
                    return rangeText(low, high);
                }
                case Stat::Speed:
                    if (type != ESM::Weapon::sRecordId)
                        return "-";
                    return formatNumber(ptr.get<ESM::Weapon>()->mBase->mData.mSpeed);
                case Stat::Weight:
                    return formatNumber(ptr.getClass().getWeight(ptr));
                case Stat::TotalWeight:
                    return formatNumber(ptr.getClass().getWeight(ptr) * item.mCount);
                case Stat::Value:
                    return std::to_string(ptr.getClass().getValue(ptr));
                case Stat::Count:
                    return std::to_string(item.mCount);
                case Stat::Effect:
                {
                    const std::vector<std::string> effects = itemEffects(ptr);
                    return effects.empty() ? "-" : effects.front();
                }
                case Stat::Quality:
                {
                    const float quality = toolQuality(ptr);
                    return quality < 0 ? "-" : formatNumber(quality);
                }
                case Stat::Uses:
                    if (ptr.getClass().hasItemHealth(ptr))
                        return std::to_string(ptr.getClass().getItemHealth(ptr));
                    return "-";
                case Stat::Name:
                    return escaped(ptr.getClass().getName(ptr));
            }
            return {};
        }

        std::vector<Column> areaColumns(int area)
        {
            if (area == sWeaponArea)
                return { { Stat::Damage, 70 }, { Stat::Speed, 58 }, { Stat::Weight, 62 }, { Stat::Value, 62 } };
            if (area == sAmmunitionArea)
                return { { Stat::Damage, 70 }, { Stat::Count, 58 }, { Stat::Value, 62 } };
            if (area >= sFirstJewelleryArea)
                return { { Stat::Weight, 62 }, { Stat::Value, 62 } };
            return { { Stat::Armor, 64 }, { Stat::Weight, 64 }, { Stat::Value, 64 } };
        }

        std::vector<Column> tabColumns(int tab)
        {
            switch (tab)
            {
                case 2:
                    return { { Stat::Effect, 200 }, { Stat::Count, 58 }, { Stat::Value, 62 } };
                case 4:
                    return { { Stat::Quality, 70 }, { Stat::Uses, 58 }, { Stat::Weight, 62 }, { Stat::Value, 62 } };
                case 3:
                    return { { Stat::Weight, 62 }, { Stat::Value, 62 } };
                case 6:
                    return { { Stat::Count, 58 }, { Stat::TotalWeight, 130 } };
                default:
                    return { { Stat::Count, 58 }, { Stat::Weight, 62 }, { Stat::Value, 62 } };
            }
        }

        std::vector<Column> listColumns(int tab, int area)
        {
            return tab == 1 ? areaColumns(area) : tabColumns(tab);
        }

        // The orders the sort button cycles through. The first is the list's main stat, such as the armor rating
        // for armor and the damage for weapons. The list's other columns follow, then the name.
        std::vector<Stat> sortOrders(int tab, int area)
        {
            Stat first = Stat::Value;
            if (tab == 1 && (area == sWeaponArea || area == sAmmunitionArea))
                first = Stat::Damage;
            else if (tab == 1 && area < sFirstJewelleryArea)
                first = Stat::Armor;
            else if (tab == 4)
                first = Stat::Quality;
            else if (tab == 6)
                first = Stat::TotalWeight;
            std::vector<Stat> orders{ first };
            for (const Column& column : listColumns(tab, area))
                if (column.mStat != first && column.mStat != Stat::Effect)
                    orders.push_back(column.mStat);
            orders.push_back(Stat::Name);
            return orders;
        }

        // The number an item's list stat shows, used to compare it with the equipped item. Items without the stat
        // count as 0. Weapons compare by their highest damage.
        float compareNumber(Stat stat, const MWWorld::Ptr& ptr)
        {
            const unsigned int type = ptr.getType();
            if (stat == Stat::Armor && type == ESM::Armor::sRecordId)
                return armorRating(ptr);
            if (stat == Stat::Damage && type == ESM::Weapon::sRecordId)
                return static_cast<float>(bestAttack(*ptr.get<ESM::Weapon>()->mBase).second);
            return 0.f;
        }

        // The number to sort items by, highest first. Items without the stat sort last.
        float sortNumber(Stat stat, const ItemStack& item)
        {
            const MWWorld::Ptr& ptr = item.mBase;
            const MWWorld::Class& cls = ptr.getClass();
            const unsigned int type = ptr.getType();
            switch (stat)
            {
                case Stat::Armor:
                    return type == ESM::Armor::sRecordId ? cls.getSkillAdjustedArmorRating(ptr, player(), true) : -1.f;
                case Stat::Damage:
                {
                    if (type != ESM::Weapon::sRecordId)
                        return -1.f;
                    // The highest damage decides, and the lowest breaks ties.
                    const auto [low, high] = bestAttack(*ptr.get<ESM::Weapon>()->mBase);
                    return static_cast<float>(high) + static_cast<float>(low) / 1000.f;
                }
                case Stat::Speed:
                    return type == ESM::Weapon::sRecordId ? ptr.get<ESM::Weapon>()->mBase->mData.mSpeed : -1.f;
                case Stat::Weight:
                    return cls.getWeight(ptr);
                case Stat::TotalWeight:
                    return cls.getWeight(ptr) * item.mCount;
                case Stat::Value:
                    return static_cast<float>(cls.getValue(ptr));
                case Stat::Count:
                    return static_cast<float>(item.mCount);
                case Stat::Quality:
                    return toolQuality(ptr);
                case Stat::Uses:
                    return cls.hasItemHealth(ptr) ? static_cast<float>(cls.getItemHealth(ptr)) : -1.f;
                default:
                    return 0.f;
            }
        }
    }

    InventoryScreen::InventoryScreen() = default;

    InventoryScreen::~InventoryScreen() = default;

    void InventoryScreen::clear()
    {
        mModel.reset();
        mItems.clear();
        mTab = Tab::Equipment;
        mFocus = Focus::Areas;
        mArea = 0;
        mSlotFilter = -1;
        mPendingRing = -1;
        mLists = {};
        mTabSort = {};
        mAreaSort = {};
        mAreaList.reset();
        mHighlighted = ESM::RefId();
        mQuickKeys.cancel();
    }

    void InventoryScreen::onOpen()
    {
        mModel = std::make_unique<InventoryItemModel>(player());
        mPendingRing = -1;
        mHeldDirection.reset();
        mQuickKeys.cancel();
        updateItems();
        redraw();
    }

    void InventoryScreen::onClose()
    {
        setHighlighted(ESM::RefId());
        mQuickKeys.cancel();
    }

    void InventoryScreen::onInventoryUpdate(const MWWorld::Ptr& ptr)
    {
        if (ptr == player())
            mNeedsUpdate = true;
    }

    void InventoryScreen::updateItems()
    {
        mNeedsUpdate = false;
        if (!mModel)
            return;
        mModel->update();
        mItems.clear();
        for (std::size_t i = 0; i < mModel->getItemCount(); ++i)
        {
            ItemStack item = mModel->getItem(static_cast<ItemModel::ModelIndex>(i));
            if (item.mBase.getClass().isGold(item.mBase))
                continue;
            mItems.push_back(std::move(item));
        }
    }

    void InventoryScreen::setHighlighted(const ESM::RefId& item)
    {
        if (item == mHighlighted)
            return;
        // The item stays marked as new while it is highlighted, and stops being new once the selection moves on.
        if (!mHighlighted.empty())
            MWBase::Environment::get().getWorld()->getPlayer().markRecentItemSeen(mHighlighted);
        mHighlighted = item;
    }

    void InventoryScreen::onFrame(float duration)
    {
        if (!isVisible() || !mModel)
            return;

        // Equipping goes through Lua, a frame later, and doesn't always report an inventory update, so this compares
        // the inventory with the last one shown.
        std::vector<std::pair<MWWorld::Ptr, std::pair<std::size_t, int>>> before;
        for (const ItemStack& item : mItems)
            before.emplace_back(item.mBase, std::make_pair(item.mCount, static_cast<int>(item.mType)));
        updateItems();
        bool changed = before.size() != mItems.size();
        for (std::size_t i = 0; !changed && i < mItems.size(); ++i)
            changed = before[i].first != mItems[i].mBase || before[i].second.first != mItems[i].mCount
                || before[i].second.second != static_cast<int>(mItems[i].mType);
        if (changed)
            redraw();

        MWBase::WindowManager* winMgr = MWBase::Environment::get().getWindowManager();
        if (winMgr->getActiveControllerWindow() != this)
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

        // The d-pad and the left stick move the selection, repeating while held.
        int dx = 0;
        int dy = 0;
        if (mHeldDirection.update(duration, dx, dy))
            move(dx, dy);

        if (mBookPaper != nullptr)
            scrollBook(MWBase::Environment::get().getInputManager()->getControllerAxisValue(SDL_CONTROLLER_AXIS_RIGHTY),
                duration);
    }

    void InventoryScreen::scrollBook(float stick, float duration)
    {
        // Squaring the deflection gives finer control near the centre of the stick.
        const float magnitude = std::abs(stick);
        if (magnitude < sStickDeadzone || mBookScrollMax <= 0)
            return;
        const float scaled = (magnitude - sStickDeadzone) / (1.f - sStickDeadzone);
        mBookScroll = std::clamp(mBookScroll + std::copysign(scaled * scaled, stick) * sBookScrollSpeed * duration, 0.f,
            static_cast<float>(mBookScrollMax));
        placeBookText();
    }

    bool InventoryScreen::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
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
                activate();
                break;
            case SDL_CONTROLLER_BUTTON_B:
                back();
                break;
            case SDL_CONTROLLER_BUTTON_X:
                dropFocused();
                break;
            case SDL_CONTROLLER_BUTTON_LEFTSTICK:
                startQuickKey();
                break;
            case SDL_CONTROLLER_BUTTON_Y:
                cycleSort();
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

    bool InventoryScreen::onControllerThumbstickEvent(const SDL_ControllerAxisEvent& arg)
    {
        // The left stick moves the selection and the right stick scrolls a book. Both are read every frame in
        // onFrame.
        return arg.axis == SDL_CONTROLLER_AXIS_LEFTX || arg.axis == SDL_CONTROLLER_AXIS_LEFTY
            || arg.axis == SDL_CONTROLLER_AXIS_RIGHTX || arg.axis == SDL_CONTROLLER_AXIS_RIGHTY;
    }

    void InventoryScreen::changeTab(int delta)
    {
        setHighlighted(ESM::RefId());
        mTab = static_cast<Tab>((static_cast<int>(mTab) + delta + sTabCount) % sTabCount);
        mPendingRing = -1;
        redraw();
    }

    // ---------------------------------------------------------------------------------------------------------------
    // Item lists

    std::vector<int> InventoryScreen::areaItems(int area) const
    {
        std::vector<int> result;
        const AreaDef& def = sAreas[area];
        for (std::size_t i = 0; i < mItems.size(); ++i)
        {
            const MWWorld::Ptr& ptr = mItems[i].mBase;
            if (!isEquipment(ptr))
                continue;
            if (area == sWeaponArea && ptr.getType() != ESM::Weapon::sRecordId)
                continue;
            const std::vector<int> slots = ptr.getClass().getEquipmentSlots(ptr).first;
            const bool fits = std::any_of(slots.begin(), slots.end(), [&](int slot) {
                return std::find(def.mSlots.begin(), def.mSlots.end(), slot) != def.mSlots.end();
            });
            if (fits)
                result.push_back(static_cast<int>(i));
        }
        return result;
    }

    int InventoryScreen::areaTopItem(int area) const
    {
        MWWorld::InventoryStore& inventory = playerInventory();
        for (int slot : sAreas[area].mTopOrder)
        {
            const auto it = inventory.getSlot(slot);
            if (it == inventory.end())
                continue;
            for (std::size_t i = 0; i < mItems.size(); ++i)
                if (mItems[i].mBase == *it)
                    return static_cast<int>(i);
        }
        return -1;
    }

    bool InventoryScreen::areaHasNew(int area) const
    {
        const MWWorld::Player& worldPlayer = MWBase::Environment::get().getWorld()->getPlayer();
        for (int item : areaItems(area))
            if (mItems[item].mType != ItemStack::Type_Equipped
                && worldPlayer.isRecentItemNew(mItems[item].mBase.getCellRef().getRefId()))
                return true;
        return false;
    }

    std::vector<InventoryScreen::Row> InventoryScreen::buildRows()
    {
        std::vector<Row> rows;
        const int tab = static_cast<int>(mTab);

        if (mTab == Tab::Recent)
        {
            for (const ESM::RecentItem& recent :
                MWBase::Environment::get().getWorld()->getPlayer().getRecentItems())
                for (std::size_t i = 0; i < mItems.size(); ++i)
                    if (mItems[i].mBase.getCellRef().getRefId() == recent.mItem
                        && rows.size() < MWWorld::Player::sRecentItemsShown)
                        rows.push_back(Row{ {}, static_cast<int>(i) });
            return rows;
        }

        const std::vector<Stat> orders = sortOrders(tab, mArea);
        const Stat sort = orders[std::min<std::size_t>(sortIndex(), orders.size() - 1)];
        const auto sorter = [&](int a, int b) {
            if (sort != Stat::Name)
            {
                const float numberA = sortNumber(sort, mItems[a]);
                const float numberB = sortNumber(sort, mItems[b]);
                if (numberA != numberB)
                    return numberA > numberB;
            }
            return mItems[a].mBase.getClass().getName(mItems[a].mBase)
                < mItems[b].mBase.getClass().getName(mItems[b].mBase);
        };

        if (mTab == Tab::Equipment)
        {
            const AreaDef& def = sAreas[mArea];
            std::vector<int> items = areaItems(mArea);
            if (mSlotFilter >= 0)
            {
                std::erase_if(items, [&](int item) {
                    const MWWorld::Ptr& ptr = mItems[item].mBase;
                    const std::vector<int> slots = ptr.getClass().getEquipmentSlots(ptr).first;
                    return std::find(slots.begin(), slots.end(), mSlotFilter) == slots.end();
                });
            }
            std::stable_sort(items.begin(), items.end(), sorter);
            if (!def.mGroupBySlot || mSlotFilter >= 0)
            {
                for (int item : items)
                    rows.push_back(Row{ {}, item });
                return rows;
            }
            for (int slot : def.mSlots)
            {
                bool header = false;
                for (int item : items)
                {
                    const MWWorld::Ptr& ptr = mItems[item].mBase;
                    const std::vector<int> slots = ptr.getClass().getEquipmentSlots(ptr).first;
                    if (slots.empty() || slots.front() != slot)
                        continue;
                    if (!header)
                    {
                        rows.push_back(Row{ slotKey(slot), -1 });
                        header = true;
                    }
                    rows.push_back(Row{ {}, item });
                }
            }
            return rows;
        }

        // The Weight tab lists every item, whichever tab it belongs in.
        std::vector<int> items;
        for (std::size_t i = 0; i < mItems.size(); ++i)
            if (mTab == Tab::Weight || inventoryTab(mItems[i].mBase) == tab)
                items.push_back(static_cast<int>(i));
        std::stable_sort(items.begin(), items.end(), sorter);

        if (mTab == Tab::Consumables)
        {
            // Consumables are grouped by their main effect, so all the Restore Health potions sit together.
            std::vector<std::pair<std::string, int>> grouped;
            for (int item : items)
            {
                const std::vector<std::string> effects = itemEffects(mItems[item].mBase);
                std::string group = effects.empty() ? std::string() : effects.front();
                if (group == "?")
                    group = "#{Interface:InventoryUnknownEffect}";
                grouped.emplace_back(std::move(group), item);
            }
            std::stable_sort(grouped.begin(), grouped.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });
            std::string group = "\n";
            for (const auto& [effect, item] : grouped)
            {
                if (effect != group)
                {
                    rows.push_back(Row{ effect.empty() ? "#{Interface:None}" : effect, -1 });
                    group = effect;
                }
                rows.push_back(Row{ {}, item });
            }
            return rows;
        }

        // Some tabs are split into groups by kind, in a fixed order, each under its own header. The sort order still
        // applies within each group.
        const auto grouped = [&](const std::vector<std::string>& headers, const auto& groupOf) {
            for (std::size_t group = 0; group < headers.size(); ++group)
            {
                bool header = false;
                for (int item : items)
                {
                    if (groupOf(mItems[item].mBase) != static_cast<int>(group))
                        continue;
                    if (!header)
                    {
                        rows.push_back(Row{ headers[group], -1 });
                        header = true;
                    }
                    rows.push_back(Row{ {}, item });
                }
            }
            return rows;
        };
        if (mTab == Tab::Misc)
            return grouped({ "#{Interface:InventoryGroupKeys}", "#{Interface:InventoryGroupSoulGems}",
                               "#{Interface:InventoryGroupOther}" },
                [](const MWWorld::Ptr& ptr) {
                    if (ptr.getClass().isKey(ptr))
                        return 0;
                    if (ptr.getClass().isSoulGem(ptr))
                        return 1;
                    return 2;
                });
        // Notes are books that teach a skill, other books, then letters and notes. The game calls loose papers
        // scrolls, even when they hold no spell.
        if (mTab == Tab::Notes)
            return grouped({ "#{Interface:InventoryGroupSkillBooks}", "#{Interface:InventoryGroupBooks}",
                               "#{Interface:InventoryGroupLetters}" },
                [](const MWWorld::Ptr& ptr) {
                    const ESM::Book& book = *ptr.get<ESM::Book>()->mBase;
                    if (book.mData.mIsScroll)
                        return 2;
                    if (store().get<ESM::Skill>().search(book.mData.mSkillId) != nullptr)
                        return 0;
                    return 1;
                });

        for (int item : items)
            rows.push_back(Row{ {}, item });
        return rows;
    }

    int InventoryScreen::focusedItem()
    {
        if (mTab == Tab::Equipment)
        {
            if (mFocus == Focus::Areas)
                return areaTopItem(mArea);
            if (mFocus == Focus::Slots)
            {
                MWWorld::InventoryStore& inventory = playerInventory();
                const auto it = inventory.getSlot(sAreas[mArea].mSlots[mSlotFocus]);
                if (it == inventory.end())
                    return -1;
                for (std::size_t i = 0; i < mItems.size(); ++i)
                    if (mItems[i].mBase == *it)
                        return static_cast<int>(i);
                return -1;
            }
        }
        const std::vector<Row> rows = buildRows();
        const ControllerList& list = mTab == Tab::Equipment ? mAreaList : mLists[static_cast<int>(mTab)];
        int index = 0;
        for (const Row& row : rows)
        {
            if (row.mItem < 0)
                continue;
            if (index == list.getFocus())
                return row.mItem;
            ++index;
        }
        return -1;
    }

    int& InventoryScreen::sortIndex()
    {
        return mTab == Tab::Equipment ? mAreaSort[mArea] : mTabSort[static_cast<int>(mTab)];
    }

    int InventoryScreen::compareSlot(int item) const
    {
        // The slot whose item the given one would replace.
        if (item < 0 || !isEquipment(mItems[item].mBase) || mItems[item].mType == ItemStack::Type_Equipped)
            return -1;
        const MWWorld::Ptr& ptr = mItems[item].mBase;
        const std::vector<int> slots = ptr.getClass().getEquipmentSlots(ptr).first;
        if (slots.empty())
            return -1;
        if (mPendingRing == item && mFocus == Focus::Slots)
            return sAreas[mArea].mSlots[mSlotFocus];
        if (mSlotFilter >= 0 && std::find(slots.begin(), slots.end(), mSlotFilter) != slots.end())
            return mSlotFilter;
        return slots.front();
    }

    int InventoryScreen::compareItem(int item) const
    {
        // The equipped item the given one would replace.
        const int slot = compareSlot(item);
        if (slot < 0)
            return -1;
        MWWorld::InventoryStore& inventory = playerInventory();
        const auto it = inventory.getSlot(slot);
        if (it == inventory.end())
            return -1;
        for (std::size_t i = 0; i < mItems.size(); ++i)
            if (mItems[i].mBase == *it)
                return static_cast<int>(i);
        return -1;
    }

    std::string InventoryScreen::actionFor(int item) const
    {
        if (item < 0)
            return {};
        const MWWorld::Ptr& ptr = mItems[item].mBase;
        const unsigned int type = ptr.getType();
        if (isEquipment(ptr) || type == ESM::Lockpick::sRecordId || type == ESM::Probe::sRecordId)
            return mItems[item].mType == ItemStack::Type_Equipped ? "#{Interface:InventoryActionUnequip}"
                                                                   : "#{Interface:Equip}";
        if (type == ESM::Potion::sRecordId)
            return "#{Interface:InventoryActionDrink}";
        if (type == ESM::Ingredient::sRecordId)
            return "#{Interface:InventoryActionEat}";
        if (type == ESM::Book::sRecordId)
        {
            const ESM::Book& book = *ptr.get<ESM::Book>()->mBase;
            if (!book.mEnchant.empty())
                return "#{Interface:Equip}";
            // Only the game's own reading teaches the skill, so the action says so.
            if (isSkillBookUnread(ptr))
                if (const ESM::Skill* skill = store().get<ESM::Skill>().search(book.mData.mSkillId))
                    return "#{Interface:InventoryActionRead} (+1 " + escaped(skill->mName) + ")";
            return "#{Interface:InventoryActionRead}";
        }
        if (type == ESM::Repair::sRecordId || type == ESM::Apparatus::sRecordId || ptr.getClass().isSoulGem(ptr))
            return "#{Interface:InventoryActionUse}";
        return {};
    }

    // ---------------------------------------------------------------------------------------------------------------
    // Drawing

    void InventoryScreen::redraw()
    {
        clearContent();
        mBookPaper = nullptr;
        mBookScrollThumb = nullptr;
        const MyGUI::IntSize size = mMainWidget->getSize();
        if (!mModel)
            return;

        drawHeader(size.width);

        const int top = sContentTop;
        const int bottom = size.height - sContentBottomMargin;
        if (mTab == Tab::Equipment)
        {
            if (mFocus == Focus::Areas)
                drawEquipmentOverview(size.width);
            else
                drawArea(size.width);
            return;
        }

        // The Notes tab shows the highlighted book's text, so its details panel gets half the width.
        const int panelW = mTab == Tab::Notes ? (size.width - 2 * sMargin - 16) / 2
                                              : std::max(300, static_cast<int>(size.width * 0.3f));
        const int listW = size.width - 2 * sMargin - panelW - 16;
        const std::vector<Row> rows = buildRows();
        drawList(sMargin, top, listW, bottom - top, rows, mLists[static_cast<int>(mTab)], true);
        const int item = focusedItem();
        if (item >= 0 && mTab == Tab::Notes && !mQuickKeys.isChoosing())
            drawBook(size.width - sMargin - panelW, top, panelW, bottom - top, item);
        else if (item >= 0)
            drawDetails(size.width - sMargin - panelW, top, panelW, bottom - top, item, actionFor(item));
        else
            drawEmptyDetails(size.width - sMargin - panelW, top, panelW, bottom - top,
                mTab == Tab::Recent ? "#{Interface:InventoryNoRecentItems}" : "#{Interface:InventoryNoItems}");
    }

    void InventoryScreen::drawHeader(int width)
    {
        std::vector<std::string> captions;
        // The loot and barter screens share the other tabs' names, but have no Weight tab.
        for (int tab = 0; tab < sTabCount; ++tab)
            captions.push_back(tab == static_cast<int>(Tab::Weight) ? "#{Interface:InventoryTabWeight}"
                                                                     : inventoryTabKey(tab));

        // The armour rating, encumbrance and gold.
        const MWWorld::Ptr actor = player();
        const MWWorld::Class& cls = actor.getClass();
        const std::string status = "#{sArmor}  " + formatNumber(std::floor(cls.getArmorRating(actor, true)))
            + "      #{sEncumbrance}  " + formatNumber(std::floor(cls.getEncumbrance(actor))) + " / "
            + formatNumber(std::floor(cls.getCapacity(actor))) + "      #{sGold}  "
            + std::to_string(playerInventory().count(MWWorld::ContainerStore::sGoldId));
        drawTabRow(mContent, width, captions, static_cast<int>(mTab), status);
    }

    void InventoryScreen::drawEquipmentOverview(int width)
    {
        const int top = sContentTop;
        const int bottom = mMainWidget->getHeight() - sContentBottomMargin;
        const int panelW = std::max(320, static_cast<int>(width * 0.3f));
        const int columnsW = width - 2 * sMargin - panelW - 24;
        const int columnW = (columnsW - 16) / 2;
        const int cardH = (bottom - top - 5 * 6) / sAreasPerColumn;
        MWWorld::InventoryStore& inventory = playerInventory();

        for (int area = 0; area < static_cast<int>(sAreas.size()); ++area)
        {
            const int column = area / sAreasPerColumn;
            const int row = area % sAreasPerColumn;
            const int x = sMargin + column * (columnW + 16);
            const int y = top + row * (cardH + 6);
            const bool focused = area == mArea;
            addRect(mContent, x, y, columnW, cardH, sPanelColour, 0.9f);
            // Pointing at an area highlights it, and clicking it opens it, as A does.
            addPointerTarget(
                mContent, x, y, columnW, cardH,
                [this, area] {
                    if (mArea == area)
                        return;
                    mArea = area;
                    redraw();
                },
                [this, area] {
                    mArea = area;
                    pressButton(SDL_CONTROLLER_BUTTON_A);
                });
            if (focused)
            {
                addRect(mContent, x, y, columnW, cardH, sAccentColour, 0.16f);
                addRect(mContent, x, y, 3, cardH, sAccentColour);
            }
            addText(mContent, x + 84, y + 6, columnW - 100, 22, areaKey(area), sFontSmall, sDimColour);
            if (areaHasNew(area))
                addText(mContent, x + columnW - 70, y + 6, 58, 22, "#{Interface:InventoryNew}", sFontSmall,
                    sNewColour, MyGUI::Align::Right | MyGUI::Align::VCenter);

            const int iconSize = std::min(62, cardH - 28);
            const int topItem = areaTopItem(area);
            if (topItem < 0)
            {
                addRect(mContent, x + 12, y + (cardH - iconSize) / 2, iconSize, iconSize, sIconBackColour, 0.9f);
                addText(mContent, x + 84, y + 28, columnW - 96, 28, "#{Interface:InventoryNothingEquipped}",
                    sFontNormal, sDimColour);
                continue;
            }
            const ItemStack& shown = mItems[topItem];
            addIcon(mContent, x + 12, y + (cardH - iconSize) / 2, iconSize, shown.mBase, false);
            std::string name = escaped(shown.mBase.getClass().getName(shown.mBase));
            if (shown.mCount > 1)
                name += " (" + std::to_string(shown.mCount) + ")";
            addText(mContent, x + 84, y + 28, columnW - 96, 28, name, sFontNormal,
                focused ? sActiveColour : sBrightColour);

            // The second line names the layers under the top item, or the top item's main stat.
            std::string second;
            for (int slot : sAreas[area].mSlots)
            {
                const auto it = inventory.getSlot(slot);
                if (it == inventory.end() || *it == shown.mBase)
                    continue;
                second += (second.empty() ? "+ " : ", ") + escaped(it->getClass().getName(*it));
            }
            if (second.empty())
            {
                second = describe(shown.mBase);
                if (shown.mBase.getType() == ESM::Armor::sRecordId)
                    second += ", #{sArmor} " + statText(Stat::Armor, shown);
                else if (shown.mBase.getType() == ESM::Weapon::sRecordId)
                    second += ", " + statText(Stat::Damage, shown);
            }
            addText(mContent, x + 84, y + 56, columnW - 96, 24, second, sFontSmall, sDimColour);
        }

        // The highlighted area's details: what each slot holds, and what's new for it.
        const int px = width - sMargin - panelW;
        addRect(mContent, px, top, panelW, bottom - top, sPanelColour, 0.9f);
        addText(mContent, px + 16, top + 10, panelW - 32, 32, areaKey(mArea), sFontTitle, sBrightColour);
        int y = top + 52;
        for (int slot : sAreas[mArea].mSlots)
        {
            addText(mContent, px + 16, y, panelW - 32, 22, slotKey(slot), sFontSmall, sDimColour);
            y += 24;
            const auto it = inventory.getSlot(slot);
            if (it != inventory.end())
            {
                addIcon(mContent, px + 16, y, 40, *it, false);
                addText(mContent, px + 66, y, panelW - 82, 22, escaped(it->getClass().getName(*it)), sFontNormal,
                    sBrightColour);
                addText(mContent, px + 66, y + 20, panelW - 82, 22, describe(*it), sFontSmall, sDimColour);
            }
            else
                addText(mContent, px + 16, y, panelW - 32, 40, "#{Interface:InventoryNothingEquipped}", sFontNormal,
                    sDimColour);
            y += 54;
        }
        const MWWorld::Player& worldPlayer = MWBase::Environment::get().getWorld()->getPlayer();
        std::string news;
        for (int item : areaItems(mArea))
            if (mItems[item].mType != ItemStack::Type_Equipped
                && worldPlayer.isRecentItemNew(mItems[item].mBase.getCellRef().getRefId()))
                news += (news.empty() ? "" : ", ") + escaped(mItems[item].mBase.getClass().getName(mItems[item].mBase));
        if (!news.empty())
            addText(mContent, px + 16, y + 6, panelW - 32, 24, "#{Interface:InventoryNew}: " + news, sFontSmall,
                sNewColour);
        addGlyph(mContent, px + 16, bottom - 44, 26, SDL_CONTROLLER_BUTTON_A);
        MyGUI::TextBox* change = addText(mContent, px + 50, bottom - 48, panelW - 66, 34,
            "#{Interface:InventoryActionChange}", sFontNormal, sBrightColour);
        // Clicking the hint presses its button.
        addPointerTarget(mContent, px + 16, bottom - 48, 34 + change->getTextSize().width, 34, nullptr,
            [] { pressButton(SDL_CONTROLLER_BUTTON_A); });
    }

    void InventoryScreen::drawArea(int width)
    {
        const int top = sContentTop;
        const int bottom = mMainWidget->getHeight() - sContentBottomMargin;
        const int slotsW = std::max(200, static_cast<int>(width * 0.18f));
        const int panelW = std::max(300, static_cast<int>(width * 0.27f));
        const int listX = sMargin + slotsW + 16;
        const int listW = width - sMargin - panelW - 16 - listX;
        MWWorld::InventoryStore& inventory = playerInventory();
        const AreaDef& def = sAreas[mArea];

        // The area's slots.
        addRect(mContent, sMargin, top, slotsW, bottom - top, sPanelColour, 0.9f);
        addText(mContent, sMargin + 16, top + 10, slotsW - 32, 32, areaKey(mArea), sFontTitle, sBrightColour);
        int y = top + 52;
        if (mPendingRing >= 0)
        {
            addText(mContent, sMargin + 16, y, slotsW - 32, 22, "#{Interface:InventoryReplaceWhich}", sFontSmall,
                sNewColour);
            y += 28;
        }
        for (int i = 0; i < static_cast<int>(def.mSlots.size()); ++i)
        {
            const int slot = def.mSlots[i];
            const bool focused = mFocus == Focus::Slots && i == mSlotFocus;
            if (focused || slot == mSlotFilter)
            {
                addRect(mContent, sMargin + 4, y - 2, slotsW - 8, 68, sAccentColour, focused ? 0.16f : 0.08f);
                addRect(mContent, sMargin + 4, y - 2, 3, 68, sAccentColour, focused ? 1.f : 0.5f);
            }
            // Pointing at a slot highlights it, moving the selection there from the items as the d-pad does, and
            // clicking it acts on it, as A does. While the player picks a quick key, the d-pad picks it instead.
            if (!mQuickKeys.hasDpad())
            {
                const auto focusSlot = [this, i] {
                    if (mFocus == Focus::Items)
                        setHighlighted(ESM::RefId());
                    mFocus = Focus::Slots;
                    mSlotFocus = i;
                };
                addPointerTarget(
                    mContent, sMargin + 4, y - 2, slotsW - 8, 68,
                    [this, i, focusSlot] {
                        if (mFocus == Focus::Slots && mSlotFocus == i)
                            return;
                        focusSlot();
                        redraw();
                    },
                    [focusSlot] {
                        focusSlot();
                        pressButton(SDL_CONTROLLER_BUTTON_A);
                    });
            }
            addText(mContent, sMargin + 16, y, slotsW - 32, 22, slotKey(slot), sFontSmall, sDimColour);
            y += 24;
            const auto it = inventory.getSlot(slot);
            if (it != inventory.end())
            {
                addIcon(mContent, sMargin + 16, y, 40, *it, false);
                const std::vector<std::string> lines
                    = wrapText(mContent, std::string(it->getClass().getName(*it)), slotsW - 80, sFontSmall);
                const int lineH = 40 / static_cast<int>(lines.size());
                for (std::size_t line = 0; line < lines.size(); ++line)
                    addText(mContent, sMargin + 64, y + static_cast<int>(line) * lineH, slotsW - 80, lineH,
                        escaped(lines[line]), sFontSmall, focused ? sActiveColour : sBrightColour);
            }
            else
            {
                addRect(mContent, sMargin + 16, y, 40, 40, sIconBackColour, 0.9f);
                addText(mContent, sMargin + 64, y, slotsW - 80, 40, "#{Interface:InventoryEmpty}", sFontSmall,
                    sDimColour);
            }
            y += 54;
        }
        if (def.mSlots.size() > 1 && mPendingRing < 0)
            addText(mContent, sMargin + 16, bottom - 34, slotsW - 32, 24,
                mSlotFilter >= 0 ? "#{Interface:InventoryShowAllSlots}" : "#{Interface:InventoryPickSlot}",
                sFontSmall, sDimColour);

        // The items that fit the area.
        const std::vector<Row> rows = buildRows();
        drawList(listX, top, listW, bottom - top, rows, mAreaList, mFocus == Focus::Items);

        // Details of the highlighted item.
        const int px = width - sMargin - panelW;
        const int item = mPendingRing >= 0 ? mPendingRing : focusedItem();
        if (item >= 0)
            drawDetails(px, top, panelW, bottom - top, item,
                mFocus == Focus::Slots && mPendingRing < 0 ? std::string() : actionFor(item));
        else
            drawEmptyDetails(px, top, panelW, bottom - top, "#{Interface:InventoryNothingEquipped}");
    }

    void InventoryScreen::drawList(
        int x, int y, int w, int h, const std::vector<Row>& rows, ControllerList& list, bool focused)
    {
        addRect(mContent, x, y, w, h, sPanelColour, 0.9f);
        const int tab = static_cast<int>(mTab);
        const std::vector<Column> columns = listColumns(tab, mArea);
        int statsW = 0;
        for (const Column& column : columns)
            statsW += column.mWidth;

        // The Recent tab always lists the newest items first.
        std::optional<Stat> sort;
        if (mTab != Tab::Recent)
        {
            const std::vector<Stat> orders = sortOrders(tab, mArea);
            sort = orders[std::min<std::size_t>(sortIndex(), orders.size() - 1)];
            addText(mContent, x + 16, y + 6, 260, 22, "#{Interface:InventorySortedBy} " + statLabel(*sort),
                sFontSmall, sDimColour);
        }
        int cx = x + w - statsW - 12;
        for (const Column& column : columns)
        {
            addText(mContent, cx, y + 6, column.mWidth, 22, statLabel(column.mStat), sFontSmall,
                column.mStat == sort ? sBrightColour : sDimColour, MyGUI::Align::Right | MyGUI::Align::VCenter);
            cx += column.mWidth;
        }

        std::vector<ControllerList::Row> listRows;
        for (const Row& row : rows)
            listRows.push_back({ row.mItem < 0 ? sHeaderRowHeight : sRowHeight, row.mItem >= 0 });
        const int listTop = y + 32;
        const int listBottom = y + h - 4;

        const MWWorld::Player& worldPlayer = MWBase::Environment::get().getWorld()->getPlayer();
        MWWorld::InventoryStore& inventory = playerInventory();
        // Where the list's own focused row is drawn while the selection is elsewhere on the screen, such as on the
        // equipment area's slots.
        std::optional<int> unfocusedRowY;
        const auto drawRow = [&](std::size_t i, int rowY, bool listFocus) {
            const Row& row = rows[i];
            if (row.mItem < 0)
            {
                addText(mContent, x + 12, rowY, w - 24, 24, row.mHeader, sFontSmall, sBrightColour);
                addRect(mContent, x + 12, rowY + 23, w - 24, 1, sAccentColour, 0.25f);
                return;
            }
            if (listFocus && !focused)
                unfocusedRowY = rowY;
            const ItemStack& item = mItems[row.mItem];
            const bool rowFocused = focused && listFocus;
            const bool equipped = item.mType == ItemStack::Type_Equipped;
            const int rx = x + 4;
            const int rw = w - 8;
            if (rowFocused)
            {
                addRect(mContent, rx, rowY, rw, sRowHeight - 2, sAccentColour, 0.16f);
                addRect(mContent, rx, rowY, 3, sRowHeight - 2, sAccentColour);
                setHighlighted(item.mBase.getCellRef().getRefId());
            }
            addIcon(mContent, rx + 8, rowY + 3, sRowHeight - 8, item.mBase, equipped);

            std::string name = escaped(item.mBase.getClass().getName(item.mBase));
            if (item.mCount > 1 && std::none_of(columns.begin(), columns.end(),
                    [](const Column& column) { return column.mStat == Stat::Count; }))
                name += " (" + std::to_string(item.mCount) + ")";
            const MyGUI::Colour& nameColour = rowFocused ? sActiveColour : (equipped ? sBrightColour : sTextColour);
            const int nameX = rx + sRowHeight + 6;
            const int nameW = rw - (sRowHeight + 6) - statsW - 12;

            std::vector<std::pair<std::string, MyGUI::Colour>> markers;
            if (equipped)
                markers.emplace_back("#{Interface:InventoryEquipped}", sDimColour);
            if (worldPlayer.isRecentItemNew(item.mBase.getCellRef().getRefId()))
                markers.emplace_back("#{Interface:InventoryNew}", sNewColour);
            if (isStolen(item.mBase))
                markers.emplace_back("#{Interface:InventoryStolen}", sWorseColour);
            if (isSkillBookUnread(item.mBase))
                markers.emplace_back("#{Interface:InventoryUnread}", sNewColour);
            const int quickKey = dpadSlotOf(item.mBase, {});

            if (markers.empty() && quickKey < 0)
                addText(mContent, nameX, rowY, nameW, sRowHeight - 2, name, sFontNormal, nameColour);
            else
            {
                // Markers sit under the name, so a long name and its markers never collide.
                addText(mContent, nameX, rowY + 1, nameW, 24, name, sFontNormal, nameColour);
                int mx = nameX;
                if (quickKey > 0)
                {
                    drawQuickKeyMarker(mContent, mx, rowY + 21, 4, quickKey);
                    mx += 20 + 10;
                }
                for (const auto& [label, colour] : markers)
                {
                    MyGUI::TextBox* marker = addText(mContent, mx, rowY + 22, 120, 16, label, sFontSmall, colour);
                    mx += marker->getTextSize().width + 10;
                }
            }

            // Armor ratings and damage are green where they beat what the slot holds now, and red where they don't.
            const int slot = compareSlot(row.mItem);
            cx = rx + rw - statsW - 8;
            for (const Column& column : columns)
            {
                MyGUI::Colour colour = sTextColour;
                const bool compared = column.mStat == Stat::Armor || column.mStat == Stat::Damage;
                if (compared && slot >= 0 && sortNumber(column.mStat, item) >= 0)
                {
                    const auto current = inventory.getSlot(slot);
                    const float equippedNumber
                        = current != inventory.end() ? compareNumber(column.mStat, *current) : 0.f;
                    const float number = compareNumber(column.mStat, item.mBase);
                    if (number > equippedNumber)
                        colour = sBetterColour;
                    else if (number < equippedNumber)
                        colour = sWorseColour;
                }
                addText(mContent, cx, rowY, column.mWidth, sRowHeight - 2, escaped(statText(column.mStat, item)),
                    sFontNormal, colour, MyGUI::Align::Right | MyGUI::Align::VCenter);
                cx += column.mWidth;
            }
        };

        // Pointing at an item highlights it, and clicking it uses it, as A does. On the Equipment tab, either one
        // also moves the selection from the area's slots to its items.
        const auto selectItems = [this] {
            if (mTab == Tab::Equipment)
                mFocus = Focus::Items;
        };
        if (!list.draw(
                mContent, x + 4, listTop, w - 8, listBottom - listTop, listRows, drawRow,
                [this, selectItems] {
                    selectItems();
                    redraw();
                },
                [this, selectItems] {
                    selectItems();
                    redraw();
                    pressButton(SDL_CONTROLLER_BUTTON_A);
                }))
        {
            addText(mContent, x + 16, y + 40, w - 32, 30, "#{Interface:InventoryNoItems}", sFontNormal, sDimColour);
            return;
        }

        if (mPendingRing >= 0 || mQuickKeys.hasDpad())
        {
            // While the player picks which ring to replace, or a quick key, the d-pad can't move through the items,
            // so neither can the mouse. This area sits over the list's rows and takes the pointer from them.
            addPointerTarget(mContent, x, y, w, h, nullptr, nullptr);
        }
        else if (unfocusedRowY)
        {
            // The list doesn't react to pointing at the item it already has, so this row moves the selection to the
            // items itself.
            addPointerTarget(
                mContent, x + 4, *unfocusedRowY, w - 8, sRowHeight,
                [this, selectItems] {
                    selectItems();
                    redraw();
                },
                [this, selectItems] {
                    selectItems();
                    redraw();
                    pressButton(SDL_CONTROLLER_BUTTON_A);
                });
        }
    }

    int InventoryScreen::drawDetailsHeader(int x, int y, int w, int h, int item)
    {
        const ItemStack& stack = mItems[item];
        addRect(mContent, x, y, w, h, sPanelColour, 0.9f);
        addRect(mContent, x, y, w, 1, sAccentColour, 0.3f);
        if (mTab != Tab::Equipment || mFocus == Focus::Items)
            setHighlighted(stack.mBase.getCellRef().getRefId());
        return ItemDetails::drawHeader(mContent, x, y, w, stack);
    }

    int InventoryScreen::drawButtonHints(int x, int y, int w, int h, int item, const std::string& action, bool scroll)
    {
        std::vector<Hint> hints;
        if (!action.empty())
            hints.emplace_back(SDL_CONTROLLER_BUTTON_A, action);
        if (mTab != Tab::Equipment || mFocus == Focus::Items)
            hints.emplace_back(SDL_CONTROLLER_BUTTON_X, "#{Interface:Drop}");
        if (canQuickKey(item))
            hints.emplace_back(SDL_CONTROLLER_BUTTON_LEFTSTICK, "#{Interface:QuickKey}");
        if (scroll)
            hints.emplace_back(SDL_CONTROLLER_BUTTON_RIGHTSTICK, "#{Interface:InventoryScroll}");
        return drawHints(mContent, x + 16, w - 32, y + h, hints);
    }

    void InventoryScreen::drawDetails(int x, int y, int w, int h, int item, const std::string& action)
    {
        const MWWorld::Ptr& ptr = mItems[item].mBase;
        const int headerBottom = drawDetailsHeader(x, y, w, h, item);

        // While the player chooses a quick key, the slots show in place of the stats.
        if (mQuickKeys.isChoosing())
        {
            const int hintsTop = drawHints(
                mContent, x + 16, w - 32, y + h, { { SDL_CONTROLLER_BUTTON_DPAD_UP, "#{Interface:QuickKeyChoose}" } });
            mQuickKeys.draw(mContent, x + 16, headerBottom + 4, w - 32, hintsTop - 12 - (headerBottom + 4));
            return;
        }

        const int hintsTop = drawButtonHints(x, y, w, h, item, action);
        std::vector<Compared> compared;
        if (const int compare = compareItem(item); compare >= 0)
            compared.push_back(Compared{ "#{Interface:InventoryEquipped}", mItems[compare].mBase });
        drawStats(mContent, x, headerBottom + 4, w, hintsTop - 16, ptr, compared);
    }

    void InventoryScreen::drawBook(int x, int y, int w, int h, int item)
    {
        const MWWorld::Ptr& ptr = mItems[item].mBase;
        const int headerBottom = drawDetailsHeader(x, y, w, h, item);
        if (ptr.getCellRef().getRefId() != mBookItem)
        {
            mBookItem = ptr.getCellRef().getRefId();
            mBookScroll = 0.f;
        }

        // The book's text, laid out by the same code as the book and scroll windows. A page tall enough for the whole
        // text keeps it in one column, which the right stick scrolls.
        constexpr int padding = 20;
        constexpr int barWidth = 4;
        const int pageX = x + 16;
        const int pageY = headerBottom;
        const int pageW = w - 32;
        const int pageH = y + h - 56 - pageY;
        addRect(mContent, pageX, pageY, pageW, pageH, sPageColour);
        MyGUI::Widget* textArea = mContent->createWidget<MyGUI::Widget>("",
            MyGUI::IntCoord(pageX + padding, pageY + padding, pageW - 2 * padding, pageH - 2 * padding),
            MyGUI::Align::Default);
        textArea->setNeedMouseFocus(false);
        Formatting::BookFormatter formatter;
        constexpr int unlimitedHeight = 1 << 20;
        formatter.markupToWidget(
            textArea, ptr.get<ESM::Book>()->mBase->mText, textArea->getWidth(), unlimitedHeight, true);
        if (textArea->getChildCount() > 0)
        {
            mBookPaper = textArea->getChildAt(0);
            mBookScrollMax = std::max(0, mBookPaper->getHeight() - textArea->getHeight());
            mBookScroll = std::clamp(mBookScroll, 0.f, static_cast<float>(mBookScrollMax));
        }

        // A scroll bar along the page's right edge, when the text is longer than the page.
        const bool scrolls = mBookPaper != nullptr && mBookScrollMax > 0;
        drawButtonHints(x, y, w, h, item, actionFor(item), scrolls);
        if (scrolls)
        {
            mBookTrack = MyGUI::IntCoord(pageX + pageW - padding / 2 - barWidth / 2, pageY + padding, barWidth,
                pageH - 2 * padding);
            addRect(mContent, mBookTrack.left, mBookTrack.top, mBookTrack.width, mBookTrack.height,
                MyGUI::Colour::Black, 0.12f);
            const float visible = static_cast<float>(textArea->getHeight()) / mBookPaper->getHeight();
            mBookScrollThumb = addRect(mContent, mBookTrack.left, mBookTrack.top, mBookTrack.width,
                std::max(16, static_cast<int>(mBookTrack.height * visible)), MyGUI::Colour::Black, 0.45f);
        }
        placeBookText();
    }

    void InventoryScreen::placeBookText()
    {
        if (mBookPaper == nullptr)
            return;
        const int offset = static_cast<int>(mBookScroll);
        mBookPaper->setPosition(0, -offset);
        if (mBookScrollThumb != nullptr && mBookScrollMax > 0)
        {
            const int travel = mBookTrack.height - mBookScrollThumb->getHeight();
            mBookScrollThumb->setPosition(mBookTrack.left, mBookTrack.top + travel * offset / mBookScrollMax);
        }
    }

    void InventoryScreen::drawEmptyDetails(int x, int y, int w, int h, const std::string& text)
    {
        addRect(mContent, x, y, w, h, sPanelColour, 0.9f);
        addText(mContent, x + 16, y + 16, w - 32, 30, text, sFontNormal, sDimColour);
    }

    // ---------------------------------------------------------------------------------------------------------------
    // Input

    void InventoryScreen::moveInList(ControllerList& list, const std::vector<Row>& rows, int delta)
    {
        int itemCount = 0;
        for (const Row& row : rows)
            itemCount += row.mItem >= 0 ? 1 : 0;
        if (itemCount == 0)
            return;
        list.setFocus(std::clamp(list.getFocus() + delta, 0, itemCount - 1));
    }

    void InventoryScreen::move(int dx, int dy)
    {
        if (mTab == Tab::Equipment)
        {
            if (mFocus == Focus::Areas)
            {
                const int column = mArea / sAreasPerColumn;
                const int row = std::clamp(mArea % sAreasPerColumn + dy, 0, sAreasPerColumn - 1);
                const int newColumn = std::clamp(column + dx, 0, 1);
                mArea = newColumn * sAreasPerColumn + row;
            }
            else if (mFocus == Focus::Slots)
            {
                const int slotCount = static_cast<int>(sAreas[mArea].mSlots.size());
                mSlotFocus = std::clamp(mSlotFocus + dy, 0, slotCount - 1);
                if (dx > 0 && mPendingRing < 0)
                    mFocus = Focus::Items;
            }
            else
            {
                if (dx < 0)
                {
                    mFocus = Focus::Slots;
                    setHighlighted(ESM::RefId());
                }
                else
                    moveInList(mAreaList, buildRows(), dy);
            }
        }
        else if (dy != 0)
            moveInList(mLists[static_cast<int>(mTab)], buildRows(), dy);
        redraw();
    }

    void InventoryScreen::activate()
    {
        MWBase::WindowManager* winMgr = MWBase::Environment::get().getWindowManager();
        if (mTab == Tab::Equipment)
        {
            if (mFocus == Focus::Areas)
            {
                mFocus = Focus::Items;
                mSlotFilter = -1;
                mSlotFocus = 0;
                mAreaList.reset();
                winMgr->playSound(ESM::RefId::stringRefId("Menu Click"));
                redraw();
                return;
            }
            if (mFocus == Focus::Slots)
            {
                const int slot = sAreas[mArea].mSlots[mSlotFocus];
                if (mPendingRing >= 0)
                {
                    equipInSlot(mPendingRing, slot);
                    mPendingRing = -1;
                }
                else
                {
                    mSlotFilter = mSlotFilter == slot ? -1 : slot;
                    mAreaList.reset();
                    winMgr->playSound(ESM::RefId::stringRefId("Menu Click"));
                }
                mFocus = Focus::Items;
                redraw();
                return;
            }
        }

        const int item = focusedItem();
        if (item < 0)
            return;
        const ItemStack& stack = mItems[item];
        const MWWorld::Ptr& ptr = stack.mBase;
        if (mTab == Tab::Equipment && stack.mType != ItemStack::Type_Equipped)
        {
            // A ring needs the player to choose which one it replaces, when both hands already wear one.
            MWWorld::InventoryStore& inventory = playerInventory();
            const std::vector<int> slots = ptr.getClass().getEquipmentSlots(ptr).first;
            const bool allFull = slots.size() > 1 && std::all_of(slots.begin(), slots.end(), [&](int slot) {
                return inventory.getSlot(slot) != inventory.end();
            });
            if (allFull && mSlotFilter < 0)
            {
                mPendingRing = item;
                mFocus = Focus::Slots;
                mSlotFocus = 0;
                redraw();
                return;
            }
            if (mSlotFilter >= 0 && std::find(slots.begin(), slots.end(), mSlotFilter) != slots.end()
                && slots.size() > 1)
            {
                equipInSlot(item, mSlotFilter);
                return;
            }
        }
        useItem(item);
    }

    void InventoryScreen::useItem(int item)
    {
        const ItemStack& stack = mItems[item];
        const MWWorld::Ptr ptr = stack.mBase;
        if (stack.mType == ItemStack::Type_Equipped)
        {
            playerInventory().unequipItem(ptr);
            MWBase::Environment::get().getWindowManager()->playSound(ptr.getClass().getDownSoundId(ptr));
            mNeedsUpdate = true;
            return;
        }
        // Going through Lua lets scripts react to the item being used, as in the normal inventory.
        MWBase::Environment::get().getLuaManager()->useItem(ptr, player(), false);
    }

    void InventoryScreen::equipInSlot(int item, int slot)
    {
        // Equipping fills the first empty slot the item fits, so emptying the chosen slot first puts it there.
        MWWorld::InventoryStore& inventory = playerInventory();
        if (inventory.getSlot(slot) != inventory.end())
            inventory.unequipSlot(slot);
        updateItems();
        const MWWorld::Ptr ptr = mItems[std::min<int>(item, static_cast<int>(mItems.size()) - 1)].mBase;
        MWBase::Environment::get().getLuaManager()->useItem(ptr, player(), false);
    }

    void InventoryScreen::back()
    {
        MWBase::WindowManager* winMgr = MWBase::Environment::get().getWindowManager();
        if (mTab == Tab::Equipment && mFocus != Focus::Areas)
        {
            if (mPendingRing >= 0)
            {
                mPendingRing = -1;
                mFocus = Focus::Items;
            }
            else if (mSlotFilter >= 0)
            {
                mSlotFilter = -1;
                mAreaList.reset();
            }
            else
            {
                setHighlighted(ESM::RefId());
                mFocus = Focus::Areas;
            }
            winMgr->playSound(ESM::RefId::stringRefId("Menu Click"));
            redraw();
            return;
        }
        winMgr->exitCurrentGuiMode();
    }

    void InventoryScreen::cycleSort()
    {
        if (mTab == Tab::Recent || (mTab == Tab::Equipment && mFocus == Focus::Areas))
            return;
        int& index = sortIndex();
        index = (index + 1) % static_cast<int>(sortOrders(static_cast<int>(mTab), mArea).size());
        MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
        redraw();
    }

    bool InventoryScreen::canQuickKey(int item) const
    {
        // Anything the player can use from the inventory can go on a quick key, as in the quick keys menu, except
        // books and notes without an enchantment. Reading one from a quick key is of little use.
        if (item < 0 || (mTab == Tab::Equipment && mFocus != Focus::Items) || actionFor(item).empty())
            return false;
        const MWWorld::Ptr& ptr = mItems[item].mBase;
        return ptr.getType() != ESM::Book::sRecordId || !ptr.get<ESM::Book>()->mBase->mEnchant.empty();
    }

    void InventoryScreen::startQuickKey()
    {
        const int item = focusedItem();
        if (!canQuickKey(item))
            return;
        mQuickKeys.start(QuickKeyAssigner::Target{ ESM::QuickKeys::Type::Item, mItems[item].mBase, {} });
        MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
        redraw();
    }

    void InventoryScreen::dropFocused()
    {
        if (mTab == Tab::Equipment && mFocus == Focus::Slots)
        {
            // Unequips the item in the focused slot.
            MWWorld::InventoryStore& inventory = playerInventory();
            const int slot = sAreas[mArea].mSlots[mSlotFocus];
            const auto it = inventory.getSlot(slot);
            if (it != inventory.end())
            {
                const MWWorld::Ptr ptr = *it;
                inventory.unequipSlot(slot);
                MWBase::Environment::get().getWindowManager()->playSound(ptr.getClass().getDownSoundId(ptr));
                updateItems();
                redraw();
            }
            return;
        }
        if (mTab == Tab::Equipment && mFocus == Focus::Areas)
            return;

        const int item = focusedItem();
        if (item < 0)
            return;
        const ItemStack& stack = mItems[item];
        if (stack.mCount > 1)
        {
            mPendingDropItem = item;
            CountDialog* dialog = MWBase::Environment::get().getWindowManager()->getCountDialog();
            dialog->openCountDialog(
                std::string(stack.mBase.getClass().getName(stack.mBase)), "#{sDrop}", static_cast<int>(stack.mCount));
            dialog->eventOkClicked.clear();
            dialog->eventOkClicked += MyGUI::newDelegate(this, &InventoryScreen::onDropCountChosen);
            return;
        }
        dropItem(item, 1);
    }

    void InventoryScreen::onDropCountChosen(MyGUI::Widget* /*sender*/, std::size_t count)
    {
        if (mPendingDropItem >= 0 && mPendingDropItem < static_cast<int>(mItems.size()))
            dropItem(mPendingDropItem, static_cast<int>(count));
        mPendingDropItem = -1;
    }

    void InventoryScreen::dropItem(int item, int count)
    {
        const ItemStack stack = mItems[item];
        const MWWorld::Ptr ptr = stack.mBase;
        const ESM::RefId sound = ptr.getClass().getDownSoundId(ptr);
        // Drops the item in front of the player, where a drop from the normal inventory lands with a gamepad.
        WorldItemModel drop(0.5f, 0.5f);
        mModel->moveItem(stack, count, &drop);
        MWBase::Environment::get().getWindowManager()->playSound(sound);
        updateItems();
        redraw();
    }
}
