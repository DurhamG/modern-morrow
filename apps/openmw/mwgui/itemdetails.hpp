#ifndef MWGUI_ITEMDETAILS_H
#define MWGUI_ITEMDETAILS_H

#include <string>
#include <utility>
#include <vector>

#include <MyGUI_Colour.h>

#include "../mwworld/ptr.hpp"

namespace ESM
{
    struct Weapon;
}

namespace MyGUI
{
    class Widget;
}

namespace MWGui
{
    struct ItemStack;

    /// What the controller screens say about an item, such as the inventory and loot screens: its description, its
    /// stats, and how it compares with what the player wears.
    namespace ItemDetails
    {
        bool isAmmunition(const ESM::Weapon& weapon);

        /// The weapon's strongest attack, as the lowest and highest damage.
        std::pair<int, int> bestAttack(const ESM::Weapon& weapon);

        /// A damage range, such as "2-14".
        std::string rangeText(int low, int high);

        /// The effects the player can see, as the alchemy rules allow for ingredients. Unknown effects are "?".
        std::vector<std::string> itemEffects(const MWWorld::Ptr& ptr);

        bool isSkillBookUnread(const MWWorld::Ptr& ptr);
        bool isStolen(const MWWorld::Ptr& ptr);

        /// Whether the item is worn or held: a weapon, armour, clothing or a light.
        bool isEquipment(const MWWorld::Ptr& ptr);

        /// The inventory screen tabs, by name: Recent, Equipment, Consumables, Notes, Tools and Misc.
        inline constexpr int sInventoryTabCount = 6;
        std::string inventoryTabKey(int tab);

        /// The inventory screen tab an item belongs in, from Equipment to Misc. The Recent tab is by when items
        /// arrived, not what they are, so no item belongs in it.
        int inventoryTab(const MWWorld::Ptr& ptr);

        /// The groups the loot and barter lists sort items into, in the order they show.
        enum class ItemGroup
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
        ItemGroup itemGroup(const MWWorld::Ptr& ptr);
        std::string itemGroupKey(ItemGroup group);

        /// The armour rating or highest damage, which decides whether an item is an upgrade. -1 for other items.
        float upgradeNumber(const MWWorld::Ptr& ptr);

        /// Whether the item beats what the player wears in a slot it fits, or fits a free slot.
        bool isUpgrade(const MWWorld::Ptr& ptr);

        /// The second line of a list entry: what the item is, and its main stat, such as "Long Blade, 1-20".
        std::string summary(const MWWorld::Ptr& ptr);

        /// A short description of the item, such as "Long Blade" or "Medium armor".
        std::string describe(const MWWorld::Ptr& ptr);

        /// The quality of a lockpick, probe, repair tool or alchemy apparatus, or -1 for other items.
        float toolQuality(const MWWorld::Ptr& ptr);

        /// The armour rating as the player would get it, adjusted for their armour skill.
        float armorRating(const MWWorld::Ptr& ptr);

        /// Markers for an item, such as "New" or "Stolen", with their colours. "New" only means something for the
        /// player's own items, so it can be left out.
        std::vector<std::pair<std::string, MyGUI::Colour>> markers(const ItemStack& stack, bool showNew = true);

        /// An equipped item to compare with, and what to call it, such as "Equipped" or "Left Ring".
        struct Compared
        {
            std::string mLabel;
            MWWorld::Ptr mItem;
        };

        /// Draws the item's icon, name, description and markers, in the top 100 units of a panel at x and y.
        void drawHeader(MyGUI::Widget* parent, int x, int y, int w, const ItemStack& stack, bool showNew = true);

        /// Draws the item's stats from y down, beside the stats of the equipped items it would replace, then its
        /// effects. A stat is green where it's better than what's equipped, and red where it's worse. Nothing is drawn
        /// below the bottom. Returns where the drawing ended.
        int drawStats(MyGUI::Widget* parent, int x, int y, int w, int bottom, const MWWorld::Ptr& ptr,
            const std::vector<Compared>& compared);
    }
}

#endif
