#include "pickpocketitemmodel.hpp"

#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/creaturestats.hpp"
#include "../mwmechanics/pickpocket.hpp"

#include "../mwworld/class.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"

namespace MWGui
{

    PickpocketItemModel::PickpocketItemModel(
        const MWWorld::Ptr& actor, std::unique_ptr<ItemModel> sourceModel, bool hideItems)
        : mActor(actor)
        , mPickpocketDetected(false)
    {
        mSourceModel = std::move(sourceModel);

        mSourceModel->update();
        // build list of items that player is unable to find when attempts to pickpocket.
        if (hideItems)
        {
            const MWMechanics::Pickpocket pickpocket(MWMechanics::getPlayer(), mActor);
            for (size_t i = 0; i < mSourceModel->getItemCount(); ++i)
            {
                if (!pickpocket.noticesItem())
                    mHiddenItems.push_back(mSourceModel->getItem(static_cast<ModelIndex>(i)));
            }
        }
    }

    bool PickpocketItemModel::allowedToUseItems() const
    {
        return false;
    }

    ItemStack PickpocketItemModel::getItem(ModelIndex index)
    {
        if (index < 0)
            throw std::runtime_error("Invalid index supplied");
        if (mItems.size() <= static_cast<size_t>(index))
            throw std::runtime_error("Item index out of range");
        return mItems[index];
    }

    size_t PickpocketItemModel::getItemCount()
    {
        return mItems.size();
    }

    void PickpocketItemModel::update()
    {
        mSourceModel->update();
        mItems.clear();
        const MWMechanics::Pickpocket pickpocket(MWMechanics::getPlayer(), mActor);
        for (size_t i = 0; i < mSourceModel->getItemCount(); ++i)
        {
            const ItemStack& item = mSourceModel->getItem(static_cast<ModelIndex>(i));
            if (std::find(mHiddenItems.begin(), mHiddenItems.end(), item) == mHiddenItems.end()
                && pickpocket.canSteal(item.mBase))
                mItems.push_back(item);
        }
    }

    bool PickpocketItemModel::onDropItem(const MWWorld::Ptr& item, int count)
    {
        // don't allow "reverse pickpocket" (it will be handled by scripts after 1.0)
        return false;
    }

    void PickpocketItemModel::onClose()
    {
        // Make sure we were actually closed, rather than just temporarily hidden (e.g. console or main menu opened)
        if (MWBase::Environment::get().getWindowManager()->containsMode(GM_Container)
            // If it was already detected while taking an item, no need to check now
            || mPickpocketDetected)
            return;

        MWMechanics::Pickpocket pickpocket(MWMechanics::getPlayer(), mActor);
        if (pickpocket.finish())
        {
            pickpocket.commitCrime();
            mPickpocketDetected = true;
            MWBase::Environment::get().getWindowManager()->removeGuiMode(MWGui::GM_Container);
        }
    }

    bool PickpocketItemModel::onTakeItem(const MWWorld::Ptr& item, int count)
    {
        if (mActor.getClass().getCreatureStats(mActor).getKnockedDown())
            return mSourceModel->onTakeItem(item, count);

        return stealItem(item, count);
    }

    bool PickpocketItemModel::stealItem(const MWWorld::Ptr& item, int count)
    {
        MWMechanics::Pickpocket pickpocket(MWMechanics::getPlayer(), mActor);
        if (pickpocket.pick(item, count))
        {
            pickpocket.commitCrime();
            mPickpocketDetected = true;
            MWBase::Environment::get().getWindowManager()->removeGuiMode(MWGui::GM_Container);
            return false;
        }

        pickpocket.recordTheft(item, count);
        return true;
    }
}
