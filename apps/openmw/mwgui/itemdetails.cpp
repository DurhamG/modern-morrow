#include "itemdetails.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

#include <MyGUI_TextBox.h>

#include <components/esm3/loadalch.hpp>
#include <components/esm3/loadappa.hpp>
#include <components/esm3/loadarmo.hpp>
#include <components/esm3/loadbook.hpp>
#include <components/esm3/loadclot.hpp>
#include <components/esm3/loadcrea.hpp>
#include <components/esm3/loadench.hpp>
#include <components/esm3/loadingr.hpp>
#include <components/esm3/loadligh.hpp>
#include <components/esm3/loadlock.hpp>
#include <components/esm3/loadprob.hpp>
#include <components/esm3/loadrepa.hpp>
#include <components/esm3/loadskil.hpp>
#include <components/esm3/loadweap.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/magiceffects.hpp"
#include "../mwmechanics/npcstats.hpp"
#include "../mwmechanics/weapontype.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/inventorystore.hpp"
#include "../mwworld/player.hpp"

#include "controllerscreen.hpp"
#include "itemmodel.hpp"

namespace MWGui::ItemDetails
{
    namespace
    {
        using namespace ControllerUi;

        MWWorld::Ptr player()
        {
            return MWMechanics::getPlayer();
        }

        const MWWorld::ESMStore& store()
        {
            return *MWBase::Environment::get().getESMStore();
        }

        std::string effectName(const ESM::RefId& effect, const ESM::RefId& skill, const ESM::RefId& attribute)
        {
            return MWMechanics::EffectKey(effect, skill.empty() ? attribute : skill).toString();
        }

        std::vector<std::string> enchantmentEffects(const ESM::RefId& enchantment)
        {
            std::vector<std::string> names;
            if (enchantment.empty())
                return names;
            const ESM::Enchantment* record = store().get<ESM::Enchantment>().search(enchantment);
            if (record == nullptr)
                return names;
            for (const ESM::IndexedENAMstruct& effect : record->mEffects.mList)
                names.push_back(MWMechanics::EffectKey(effect.mData).toString());
            return names;
        }

        // A row in the details panel. Numbers are compared with the equipped item's, where there is one.
        struct DetailRow
        {
            std::string mLabel;
            std::string mValue;
            float mNumber = 0;
            bool mComparable = false;
            bool mHigherIsBetter = true;
        };

        DetailRow numberRow(std::string label, float value, bool higherIsBetter = true)
        {
            return DetailRow{ std::move(label), formatNumber(value), value, true, higherIsBetter };
        }

        DetailRow rangeRow(std::string label, int low, int high)
        {
            return DetailRow{ std::move(label), rangeText(low, high), static_cast<float>(high), true, true };
        }

        std::vector<DetailRow> detailRows(const MWWorld::Ptr& ptr)
        {
            std::vector<DetailRow> rows;
            const MWWorld::Class& cls = ptr.getClass();
            const unsigned int type = ptr.getType();
            if (type == ESM::Weapon::sRecordId)
            {
                const ESM::Weapon& weapon = *ptr.get<ESM::Weapon>()->mBase;
                const ESM::WeaponType* weaponType = MWMechanics::getWeaponType(weapon.mData.mType);
                const bool melee = weaponType == nullptr || weaponType->mWeaponClass == ESM::WeaponType::Melee;
                if (melee)
                {
                    rows.push_back(rangeRow("#{sChop}", weapon.mData.mChop[0], weapon.mData.mChop[1]));
                    rows.push_back(rangeRow("#{sSlash}", weapon.mData.mSlash[0], weapon.mData.mSlash[1]));
                    rows.push_back(rangeRow("#{sThrust}", weapon.mData.mThrust[0], weapon.mData.mThrust[1]));
                }
                else
                    rows.push_back(rangeRow("#{sAttack}", weapon.mData.mChop[0], weapon.mData.mChop[1]));
                if (!isAmmunition(weapon))
                {
                    rows.push_back(numberRow("#{Interface:InventoryColumnSpeed}", weapon.mData.mSpeed));
                    if (melee)
                        rows.push_back(numberRow("#{sRange}", weapon.mData.mReach));
                }
            }
            if (type == ESM::Armor::sRecordId)
                rows.push_back(numberRow(
                    "#{sArmorRating}", armorRating(ptr)));
            if (type == ESM::Lockpick::sRecordId || type == ESM::Probe::sRecordId || type == ESM::Repair::sRecordId
                || type == ESM::Apparatus::sRecordId)
                rows.push_back(numberRow("#{sQuality}", toolQuality(ptr)));
            if (cls.hasItemHealth(ptr))
            {
                const bool tool = type == ESM::Lockpick::sRecordId || type == ESM::Probe::sRecordId
                    || type == ESM::Repair::sRecordId;
                rows.push_back(
                    numberRow(tool ? "#{sUses}" : "#{sCondition}", static_cast<float>(cls.getItemHealth(ptr))));
            }
            if (cls.isSoulGem(ptr))
            {
                const ESM::RefId& soul = ptr.getCellRef().getSoul();
                const ESM::Creature* creature = soul.empty() ? nullptr : store().get<ESM::Creature>().search(soul);
                rows.push_back(DetailRow{ "#{Interface:Soul}",
                    creature != nullptr ? escaped(creature->mName) : std::string("#{Interface:None}") });
            }
            rows.push_back(numberRow("#{sWeight}", cls.getWeight(ptr), false));
            rows.push_back(numberRow("#{sValue}", static_cast<float>(cls.getValue(ptr))));
            return rows;
        }

    }

    bool isAmmunition(const ESM::Weapon& weapon)
    {
        const ESM::WeaponType* type = MWMechanics::getWeaponType(weapon.mData.mType);
        return type != nullptr && type->mWeaponClass == ESM::WeaponType::Ammo;
    }

    std::pair<int, int> bestAttack(const ESM::Weapon& weapon)
    {
        std::pair<int, int> best(weapon.mData.mChop[0], weapon.mData.mChop[1]);
        for (const auto& attack : { weapon.mData.mSlash, weapon.mData.mThrust })
            if (attack[1] > best.second)
                best = { attack[0], attack[1] };
        return best;
    }

    std::string rangeText(int low, int high)
    {
        return std::to_string(low) + "-" + std::to_string(high);
    }

    std::vector<std::string> itemEffects(const MWWorld::Ptr& ptr)
    {
        std::vector<std::string> names;
        const unsigned int type = ptr.getType();
        if (type == ESM::Potion::sRecordId)
        {
            for (const ESM::IndexedENAMstruct& effect : ptr.get<ESM::Potion>()->mBase->mEffects.mList)
                names.push_back(MWMechanics::EffectKey(effect.mData).toString());
        }
        else if (type == ESM::Ingredient::sRecordId)
        {
            const ESM::Ingredient& ingredient = *ptr.get<ESM::Ingredient>()->mBase;
            const MWWorld::Ptr actor = player();
            const float alchemy = actor.getClass().getSkill(actor, ESM::Skill::Alchemy);
            const float wortChance
                = store().get<ESM::GameSetting>().find("fWortChanceValue")->mValue.getFloat();
            const int known = wortChance > 0 ? static_cast<int>(alchemy / wortChance) : 4;
            for (int i = 0; i < 4; ++i)
            {
                if (ingredient.mData.mEffectID[i].empty())
                    continue;
                if (i < known)
                    names.push_back(effectName(ingredient.mData.mEffectID[i], ingredient.mData.mSkills[i],
                        ingredient.mData.mAttributes[i]));
                else
                    names.emplace_back("?");
            }
        }
        else
            names = enchantmentEffects(ptr.getClass().getEnchantment(ptr));
        return names;
    }

    bool isSkillBookUnread(const MWWorld::Ptr& ptr)
    {
        if (ptr.getType() != ESM::Book::sRecordId)
            return false;
        const ESM::Book& book = *ptr.get<ESM::Book>()->mBase;
        if (book.mData.mSkillId.empty())
            return false;
        const MWWorld::Ptr actor = player();
        return !actor.getClass().getNpcStats(actor).hasBeenUsed(book.mId);
    }

    bool isStolen(const MWWorld::Ptr& ptr)
    {
        return !MWBase::Environment::get()
                    .getMechanicsManager()
                    ->getStolenItemOwners(ptr.getCellRef().getRefId())
                    .empty();
    }

    std::string describe(const MWWorld::Ptr& ptr)
    {
        const unsigned int type = ptr.getType();
        if (type == ESM::Weapon::sRecordId)
        {
            const ESM::WeaponType* weaponType
                = MWMechanics::getWeaponType(ptr.get<ESM::Weapon>()->mBase->mData.mType);
            if (weaponType != nullptr)
                if (const ESM::Skill* skill = store().get<ESM::Skill>().search(weaponType->mSkill))
                    return skill->mName;
            return "#{sWeapon}";
        }
        if (type == ESM::Armor::sRecordId)
        {
            const ESM::RefId skill = ptr.getClass().getEquipmentSkill(ptr);
            if (skill == ESM::Skill::LightArmor)
                return "#{sLight}";
            if (skill == ESM::Skill::MediumArmor)
                return "#{sMedium}";
            return "#{sHeavy}";
        }
        if (type == ESM::Clothing::sRecordId)
            return "#{Interface:InventoryKindClothing}";
        if (type == ESM::Potion::sRecordId)
            return "#{Interface:InventoryKindPotion}";
        if (type == ESM::Ingredient::sRecordId)
            return "#{sIngredients}";
        if (type == ESM::Book::sRecordId)
        {
            const ESM::Book& book = *ptr.get<ESM::Book>()->mBase;
            if (!book.mEnchant.empty())
                return "#{Interface:InventoryKindScroll}";
            if (const ESM::Skill* skill = store().get<ESM::Skill>().search(book.mData.mSkillId))
                return "#{Interface:InventoryKindSkillBook}: " + skill->mName;
            return "#{Interface:InventoryKindBook}";
        }
        if (type == ESM::Lockpick::sRecordId)
            return "#{Interface:InventoryKindLockpick}";
        if (type == ESM::Probe::sRecordId)
            return "#{Interface:InventoryKindProbe}";
        if (type == ESM::Repair::sRecordId)
            return "#{Interface:InventoryKindRepair}";
        if (type == ESM::Apparatus::sRecordId)
            return "#{Interface:InventoryKindApparatus}";
        if (type == ESM::Light::sRecordId)
            return "#{Interface:InventoryKindLight}";
        if (ptr.getClass().isSoulGem(ptr))
            return "#{Interface:InventoryKindSoulGem}";
        if (ptr.getClass().isKey(ptr))
            return "#{Interface:InventoryKindKey}";
        return "#{Interface:InventoryKindMisc}";
    }

    float toolQuality(const MWWorld::Ptr& ptr)
    {
        const unsigned int type = ptr.getType();
        if (type == ESM::Lockpick::sRecordId)
            return ptr.get<ESM::Lockpick>()->mBase->mData.mQuality;
        if (type == ESM::Probe::sRecordId)
            return ptr.get<ESM::Probe>()->mBase->mData.mQuality;
        if (type == ESM::Repair::sRecordId)
            return ptr.get<ESM::Repair>()->mBase->mData.mQuality;
        if (type == ESM::Apparatus::sRecordId)
            return ptr.get<ESM::Apparatus>()->mBase->mData.mQuality;
        return -1.f;
    }

    bool isEquipment(const MWWorld::Ptr& ptr)
    {
        const unsigned int type = ptr.getType();
        return type == ESM::Weapon::sRecordId || type == ESM::Armor::sRecordId || type == ESM::Clothing::sRecordId
            || type == ESM::Light::sRecordId;
    }

    std::string inventoryTabKey(int tab)
    {
        static const std::array<std::string_view, sInventoryTabCount> names
            = { "Recent", "Equipment", "Consumables", "Notes", "Tools", "Misc" };
        return "#{Interface:InventoryTab" + std::string(names[tab]) + "}";
    }

    int inventoryTab(const MWWorld::Ptr& ptr)
    {
        const unsigned int type = ptr.getType();
        if (isEquipment(ptr))
            return 1;
        if (type == ESM::Potion::sRecordId || type == ESM::Ingredient::sRecordId)
            return 2;
        if (type == ESM::Book::sRecordId)
            return ptr.get<ESM::Book>()->mBase->mEnchant.empty() ? 3 : 2;
        if (type == ESM::Lockpick::sRecordId || type == ESM::Probe::sRecordId || type == ESM::Repair::sRecordId
            || type == ESM::Apparatus::sRecordId)
            return 4;
        return 5;
    }

    float armorRating(const MWWorld::Ptr& ptr)
    {
        return std::floor(ptr.getClass().getSkillAdjustedArmorRating(ptr, player(), true));
    }

    enum class Group
    {
        Gold,
        Weapons,
        Armor,
        Clothing,
        Consumables,
        Books,
        Tools,
        Misc,
    };

    ItemGroup itemGroup(const MWWorld::Ptr& ptr)
    {
        const unsigned int type = ptr.getType();
        if (ptr.getClass().isGold(ptr))
            return ItemGroup::Gold;
        if (type == ESM::Weapon::sRecordId)
            return ItemGroup::Weapons;
        if (type == ESM::Armor::sRecordId)
            return ItemGroup::Armor;
        if (type == ESM::Clothing::sRecordId)
            return ItemGroup::Clothing;
        if (type == ESM::Potion::sRecordId || type == ESM::Ingredient::sRecordId)
            return ItemGroup::Consumables;
        if (type == ESM::Book::sRecordId)
            return ptr.get<ESM::Book>()->mBase->mEnchant.empty() ? ItemGroup::Books : ItemGroup::Consumables;
        if (type == ESM::Lockpick::sRecordId || type == ESM::Probe::sRecordId || type == ESM::Repair::sRecordId
            || type == ESM::Apparatus::sRecordId)
            return ItemGroup::Tools;
        return ItemGroup::Misc;
    }

    std::string itemGroupKey(ItemGroup group)
    {
        static const std::array<std::string_view, 8> names
            = { "", "Weapons", "Armor", "Clothing", "Consumables", "Books", "Tools", "Misc" };
        return "#{Interface:LootGroup" + std::string(names[static_cast<int>(group)]) + "}";
    }

    float upgradeNumber(const MWWorld::Ptr& ptr)
    {
        if (ptr.getType() == ESM::Armor::sRecordId)
            return armorRating(ptr);
        if (ptr.getType() == ESM::Weapon::sRecordId)
            return static_cast<float>(bestAttack(*ptr.get<ESM::Weapon>()->mBase).second);
        return -1.f;
    }

    std::string summary(const MWWorld::Ptr& ptr)
    {
        std::string text = describe(ptr);
        if (ptr.getType() == ESM::Armor::sRecordId)
            text += ",  #{sArmor} " + formatNumber(armorRating(ptr));
        else if (ptr.getType() == ESM::Weapon::sRecordId)
        {
            const auto [low, high] = bestAttack(*ptr.get<ESM::Weapon>()->mBase);
            text += ",  " + rangeText(low, high);
        }
        else
        {
            const std::vector<std::string> effects = itemEffects(ptr);
            if (!effects.empty())
                text += ",  " + escaped(effects.front());
        }
        return text;
    }

    bool isUpgrade(const MWWorld::Ptr& ptr)
    {
        const float number = upgradeNumber(ptr);
        if (number < 0)
            return false;
        const MWWorld::Ptr actor = player();
        MWWorld::InventoryStore& inventory = actor.getClass().getInventoryStore(actor);
        for (int slot : ptr.getClass().getEquipmentSlots(ptr).first)
        {
            const auto equipped = inventory.getSlot(slot);
            if (equipped == inventory.end() || upgradeNumber(*equipped) < number)
                return true;
        }
        return false;
    }

    std::vector<std::pair<std::string, MyGUI::Colour>> markers(const ItemStack& stack, bool showNew)
    {
        std::vector<std::pair<std::string, MyGUI::Colour>> result;
        const MWWorld::Ptr& ptr = stack.mBase;
        if (stack.mType == ItemStack::Type_Equipped)
            result.emplace_back("#{Interface:InventoryEquipped}", sDimColour);
        if (showNew && MWBase::Environment::get().getWorld()->getPlayer().isRecentItemNew(ptr.getCellRef().getRefId()))
            result.emplace_back("#{Interface:InventoryNew}", sNewColour);
        if (isStolen(ptr))
            result.emplace_back("#{Interface:InventoryStolen}", sWorseColour);
        if (isSkillBookUnread(ptr))
            result.emplace_back("#{Interface:InventoryUnread}", sNewColour);
        return result;
    }

    int drawHeader(MyGUI::Widget* parent, int x, int y, int w, const ItemStack& stack, bool showNew)
    {
        const MWWorld::Ptr& ptr = stack.mBase;
        addIcon(parent, x + 16, y + 16, 64, ptr, stack.mType == ItemStack::Type_Equipped);
        std::string name(ptr.getClass().getName(ptr));
        if (stack.mCount > 1)
            name += " (" + std::to_string(stack.mCount) + ")";

        // A long name wraps onto a second line. The name then starts higher, and the lines below it move down by
        // what's left of the extra line.
        const int textW = w - 108;
        const std::vector<std::string> lines = wrapText(parent, name, textW, sFontLarge, 2);
        constexpr int lineHeight = 26;
        const bool twoLines = lines.size() > 1;
        const int nameY = twoLines ? y + 6 : y + 14;
        const int shift = twoLines ? lineHeight - 8 : 0;
        for (std::size_t i = 0; i < lines.size(); ++i)
            addText(parent, x + 92, nameY + static_cast<int>(i) * lineHeight, textW, 34, escaped(lines[i]), sFontLarge,
                sBrightColour);
        addText(parent, x + 92, y + 46 + shift, textW, 26, describe(ptr), sFontSmall, sDimColour);
        int tx = x + 92;
        for (const auto& [label, colour] : markers(stack, showNew))
        {
            MyGUI::TextBox* tag = addText(parent, tx, y + 70 + shift, 120, 20, label, sFontSmall, colour);
            tx += tag->getTextSize().width + 12;
        }
        return y + 100 + shift;
    }

    int drawStats(MyGUI::Widget* parent, int x, int y, int w, int bottom, const MWWorld::Ptr& ptr,
        const std::vector<Compared>& compared)
    {
        const std::vector<DetailRow> rows = detailRows(ptr);
        std::vector<std::vector<DetailRow>> otherRows;
        for (const Compared& other : compared)
            otherRows.push_back(detailRows(other.mItem));

        int ry = y;
        const int valueW = 80;
        const int columns = 1 + static_cast<int>(compared.size());
        const int valuesX = x + w - 16 - columns * valueW;
        if (!compared.empty())
        {
            addText(parent, valuesX, ry, valueW, 24, "#{Interface:InventoryColumnThis}", sFontSmall, sDimColour,
                MyGUI::Align::Right | MyGUI::Align::VCenter);
            for (std::size_t i = 0; i < compared.size(); ++i)
                addText(parent, valuesX + static_cast<int>(i + 1) * valueW, ry, valueW, 24, compared[i].mLabel,
                    sFontSmall, sDimColour, MyGUI::Align::Right | MyGUI::Align::VCenter);
            ry += 26;
        }
        for (const DetailRow& row : rows)
        {
            if (ry + 30 > bottom)
                break;
            addText(parent, x + 16, ry, w - 32, 28, row.mLabel, sFontNormal, sTextColour);
            // The value is compared with the weakest of the equipped items, which is the one worth replacing.
            MyGUI::Colour colour = sBrightColour;
            std::vector<const DetailRow*> others;
            for (const std::vector<DetailRow>& other : otherRows)
            {
                const auto found = std::find_if(other.begin(), other.end(),
                    [&](const DetailRow& candidate) { return candidate.mLabel == row.mLabel; });
                others.push_back(found != other.end() ? &*found : nullptr);
            }
            if (row.mComparable)
            {
                std::optional<float> weakest;
                for (const DetailRow* other : others)
                {
                    if (other == nullptr || !other->mComparable)
                        continue;
                    if (!weakest || (row.mHigherIsBetter ? other->mNumber < *weakest : other->mNumber > *weakest))
                        weakest = other->mNumber;
                }
                if (weakest && row.mNumber != *weakest)
                    colour = (row.mNumber > *weakest) == row.mHigherIsBetter ? sBetterColour : sWorseColour;
            }
            addText(parent, valuesX, ry, valueW, 28, row.mValue, sFontNormal, colour,
                MyGUI::Align::Right | MyGUI::Align::VCenter);
            for (std::size_t i = 0; i < others.size(); ++i)
                addText(parent, valuesX + static_cast<int>(i + 1) * valueW, ry, valueW, 28,
                    others[i] != nullptr ? others[i]->mValue : "-", sFontNormal, sDimColour,
                    MyGUI::Align::Right | MyGUI::Align::VCenter);
            ry += 30;
        }

        const std::vector<std::string> effects = itemEffects(ptr);
        if (!effects.empty() && ry + 60 <= bottom)
        {
            ry += 8;
            const unsigned int type = ptr.getType();
            const bool consumable = type == ESM::Potion::sRecordId || type == ESM::Ingredient::sRecordId
                || (type == ESM::Book::sRecordId && !ptr.get<ESM::Book>()->mBase->mEnchant.empty());
            addText(parent, x + 16, ry, w - 32, 24,
                consumable ? "#{Interface:MagicEffects}" : "#{Interface:InventoryEnchantment}", sFontSmall,
                sDimColour);
            ry += 24;
            for (const std::string& effect : effects)
            {
                if (ry + 26 > bottom)
                    break;
                addText(parent, x + 16, ry, w - 32, 26, escaped(effect), sFontNormal, sNewColour);
                ry += 26;
            }
        }
        if (!compared.empty() && ry + 36 <= bottom)
        {
            std::string names;
            for (const Compared& other : compared)
                names += (names.empty() ? "" : ", ") + escaped(other.mItem.getClass().getName(other.mItem));
            addText(parent, x + 16, ry + 12, w - 32, 24, "#{Interface:InventoryReplaces} " + names, sFontSmall,
                sDimColour);
            ry += 36;
        }
        return ry;
    }
}
