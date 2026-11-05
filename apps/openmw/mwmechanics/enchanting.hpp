#ifndef GAME_MWMECHANICS_ENCHANTING_H
#define GAME_MWMECHANICS_ENCHANTING_H

#include <string>
#include <vector>

#include <components/esm3/effectlist.hpp>
#include <components/esm3/loadench.hpp>

#include "../mwworld/ptr.hpp"

namespace MWMechanics
{
    class Enchanting
    {
        MWWorld::Ptr mOldItemPtr;
        MWWorld::Ptr mSoulGemPtr;
        MWWorld::Ptr mEnchanter;

        int mCastStyle;

        bool mSelfEnchanting;

        ESM::EffectList mEffectList;

        std::string mNewItemName;
        unsigned int mObjectType;
        ESM::RefId mWeaponType;

        const ESM::Enchantment* getRecord(const ESM::Enchantment& newEnchantment) const;
        int getBaseCastCost() const; // To be saved in the enchantment's record
        int getEnchantItemsCount() const;
        float getTypeMultiplier() const;
        void payForEnchantment(int count) const;
        int getEnchantPrice(int count) const;
        std::vector<float> getEffectCosts() const;

    public:
        /// The first problem that stops the player from enchanting the item.
        enum class Problem
        {
            None,
            NoEffects,
            NoName,
            NoSoulGem,
            NoItem,
            TooManyPoints,
            NotEnoughGold,
        };

        /// Whether @a item can be enchanted: a weapon, a piece of armor or clothing, or a scroll, that isn't
        /// enchanted already.
        static bool canEnchant(const MWWorld::ConstPtr& item);

        Enchanting();
        void setEnchanter(const MWWorld::Ptr& enchanter);
        void setSelfEnchanting(bool selfEnchanting);
        void setOldItem(const MWWorld::Ptr& oldItem);
        MWWorld::Ptr getOldItem() { return mOldItemPtr; }
        MWWorld::Ptr getGem() { return mSoulGemPtr; }
        void setNewItemName(const std::string& s);
        void setEffect(const ESM::EffectList& effectList);
        void setSoulGem(const MWWorld::Ptr& soulGem);
        /// What stops the enchanting, checked in the order that the enchanting window checks.
        Problem getProblem() const;
        /// The item or the soul gem, if any, that the player stole from the enchanter. The enchanter takes it back
        /// rather than enchant with it. This is always empty when the player enchants an item themself.
        MWWorld::Ptr getStolenItem() const;
        bool create(); // Return true if created, false if failed.
        void nextCastStyle(); // Set enchant type to next possible type (for mOldItemPtr object)
        int getCastStyle() const;
        float getEnchantPoints(bool precise = true) const;
        int getEffectiveCastCost()
            const; // Effective cost taking player Enchant skill into account, used for preview purposes in the UI
        int getEnchantPrice() const { return getEnchantPrice(getEnchantItemsCount()); }
        int getMaxEnchantValue() const;
        int getGemCharge() const;
        int getEnchantChance() const;
        bool soulEmpty() const; // Return true if empty
        bool itemEmpty() const; // Return true if empty
    };
}
#endif
