#include "lootpanel.hpp"

#include <algorithm>
#include <array>

#include <MyGUI_Button.h>
#include <MyGUI_Gui.h>
#include <MyGUI_ILayer.h>
#include <MyGUI_LayerManager.h>

#include <components/esm3/loadfact.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/luamanager.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwmechanics/actorutil.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/containerstore.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/inventorystore.hpp"
#include "../mwworld/player.hpp"

#include "confirmationdialog.hpp"
#include "container.hpp"
#include "inventoryitemmodel.hpp"
#include "itemdetails.hpp"
#include "pickpocketitemmodel.hpp"

namespace MWGui
{
    namespace
    {
        using namespace ControllerUi;
        using namespace ItemDetails;
        using Inventory = MWWorld::InventoryStore;

        // The panel's place on the canvas, which is 720 units tall. It covers the right side of the screen, so the
        // body or chest stays in view on the left.
        constexpr float sPanelLeft = 0.40f;
        constexpr int sPanelTop = 40;
        constexpr int sPanelBottom = 680;
        constexpr int sPanelMargin = 40;
        constexpr int sEntryHeight = 52;
        constexpr int sHeaderRowHeight = 28;

        const MyGUI::Colour sPanelBackColour = MyGUI::Colour::parse("#0d0b08");

        MWWorld::Ptr player()
        {
            return MWMechanics::getPlayer();
        }

        Inventory& playerInventory()
        {
            const MWWorld::Ptr actor = player();
            return actor.getClass().getInventoryStore(actor);
        }

        // Who owns a container, for the warning that taking from it is stealing.
        std::string ownerName(const MWWorld::Ptr& container)
        {
            MWWorld::Ptr victim;
            MWBase::Environment::get().getMechanicsManager()->isAllowedToUse(player(), container, victim);
            if (!victim.isEmpty())
                return escaped(victim.getClass().getName(victim));
            const ESM::RefId& faction = container.getCellRef().getFaction();
            if (!faction.empty())
                if (const ESM::Faction* record
                    = MWBase::Environment::get().getESMStore()->get<ESM::Faction>().search(faction))
                    return escaped(record->mName);
            return {};
        }
    }

    LootPanel::LootPanel(ContainerWindow& window)
        : mWindow(window)
    {
    }

    LootPanel::~LootPanel()
    {
        if (mRoot != nullptr)
            MyGUI::Gui::getInstance().destroyWidget(mRoot);
    }

    void LootPanel::show()
    {
        if (mRoot == nullptr)
        {
            MyGUI::ILayer* layer = MyGUI::LayerManager::getInstance().getByName("ControllerMenus", false);
            if (layer == nullptr)
                return;
            mRoot = MyGUI::Gui::getInstance().createWidget<MyGUI::Widget>(
                "", MyGUI::IntCoord(MyGUI::IntPoint(), layer->getSize()), MyGUI::Align::Default, "ControllerMenus");
            mRoot->setNeedMouseFocus(false);
        }
        mRoot->setSize(mRoot->getLayer()->getSize());
        mRoot->setVisible(true);
        mList.reset();
        mList.resetHeld();
        mYourItems = false;
        mYourTab = 0;
        mContainerFocus = 0;
        update();
    }

    void LootPanel::hide()
    {
        if (mRoot != nullptr)
            mRoot->setVisible(false);
        mItems.clear();
    }

    bool LootPanel::isStealing() const
    {
        return mWindow.mModel != nullptr && !mWindow.mModel->allowedToUseItems();
    }

    bool LootPanel::isPickpocketing() const
    {
        return dynamic_cast<const PickpocketItemModel*>(mWindow.mModel) != nullptr;
    }

    bool LootPanel::canPutIn() const
    {
        // The game doesn't let the player put items into an NPC's pockets.
        return mWindow.mModel != nullptr && !isPickpocketing();
    }

    void LootPanel::readItems()
    {
        mItems.clear();
        if (mWindow.mModel == nullptr)
            return;
        if (!mYourItems)
        {
            mWindow.mModel->update();
            for (std::size_t i = 0; i < mWindow.mModel->getItemCount(); ++i)
                mItems.push_back(mWindow.mModel->getItem(static_cast<ItemModel::ModelIndex>(i)));
            return;
        }

        InventoryItemModel model(player());
        model.update();
        std::vector<ItemStack> all;
        for (std::size_t i = 0; i < model.getItemCount(); ++i)
            all.push_back(model.getItem(static_cast<ItemModel::ModelIndex>(i)));
        if (mYourTab == 0)
        {
            // The Recent tab lists what the player picked up last, which is often what a quest asks them to leave.
            for (const auto& recent : MWBase::Environment::get().getWorld()->getPlayer().getRecentItems())
                for (const ItemStack& item : all)
                    if (item.mBase.getCellRef().getRefId() == recent.mItem
                        && mItems.size() < MWWorld::Player::sRecentItemsShown)
                        mItems.push_back(item);
            return;
        }
        for (const ItemStack& item : all)
            if (inventoryTab(item.mBase) == mYourTab)
                mItems.push_back(item);
    }

    std::string LootPanel::currentState() const
    {
        std::string state;
        for (const ItemStack& item : mItems)
            state += item.mBase.getCellRef().getRefId().toDebugString() + std::to_string(item.mCount) + ";";
        return state;
    }

    std::vector<LootPanel::Row> LootPanel::buildRows() const
    {
        // The player's own items keep the order readItems gave them: newest first on the Recent tab, and the most
        // valuable first on the others.
        if (mYourItems)
        {
            std::vector<int> order(mItems.size());
            for (std::size_t i = 0; i < order.size(); ++i)
                order[i] = static_cast<int>(i);
            if (mYourTab != 0)
                std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
                    return mItems[a].mBase.getClass().getValue(mItems[a].mBase)
                        > mItems[b].mBase.getClass().getValue(mItems[b].mBase);
                });
            std::vector<Row> rows;
            for (int item : order)
                rows.push_back(Row{ {}, item });
            return rows;
        }

        // Gold comes first, then the items by kind, with the most valuable first in each group.
        std::vector<int> order(mItems.size());
        for (std::size_t i = 0; i < order.size(); ++i)
            order[i] = static_cast<int>(i);
        std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
            const MWWorld::Ptr& left = mItems[a].mBase;
            const MWWorld::Ptr& right = mItems[b].mBase;
            if (itemGroup(left) != itemGroup(right))
                return itemGroup(left) < itemGroup(right);
            return left.getClass().getValue(left) > right.getClass().getValue(right);
        });

        int groups = 0;
        for (std::size_t i = 0; i < order.size(); ++i)
            if (itemGroup(mItems[order[i]].mBase) != ItemGroup::Gold
                && (i == 0 || itemGroup(mItems[order[i]].mBase) != itemGroup(mItems[order[i - 1]].mBase)))
                ++groups;

        // Group headers only help when there is more than one group.
        std::vector<Row> rows;
        for (std::size_t i = 0; i < order.size(); ++i)
        {
            const ItemGroup group = itemGroup(mItems[order[i]].mBase);
            if (groups > 1 && group != ItemGroup::Gold
                && (i == 0 || group != itemGroup(mItems[order[i - 1]].mBase)))
                rows.push_back(Row{ itemGroupKey(group), -1 });
            rows.push_back(Row{ {}, order[i] });
        }
        return rows;
    }

    int LootPanel::focusedItem() const
    {
        int index = 0;
        for (const Row& row : buildRows())
        {
            if (row.mItem < 0)
                continue;
            if (index++ == mList.getFocus())
                return row.mItem;
        }
        return -1;
    }

    bool LootPanel::canTakeAndEquip(const ItemStack& item) const
    {
        if (isPickpocketing() || !isEquipment(item.mBase))
            return false;
        const std::vector<int> slots = item.mBase.getClass().getEquipmentSlots(item.mBase).first;
        if (slots.empty())
            return false;
        if (slots.size() == 1)
            return true;
        // With more than one slot, such as for rings, it's only clear where the item goes when a slot is free.
        Inventory& inventory = playerInventory();
        return std::any_of(
            slots.begin(), slots.end(), [&](int slot) { return inventory.getSlot(slot) == inventory.end(); });
    }

    void LootPanel::update()
    {
        if (mRoot == nullptr || !mRoot->getVisible())
            return;
        readItems();
        mShownState = currentState();
        while (mRoot->getChildCount() > 0)
            MyGUI::Gui::getInstance().destroyWidget(mRoot->getChildAt(0));

        const MyGUI::IntSize size = mRoot->getSize();
        const int x = static_cast<int>(size.width * sPanelLeft);
        const int w = size.width - sPanelMargin - x;
        addRect(mRoot, x, sPanelTop, w, sPanelBottom - sPanelTop, sPanelBackColour);
        addRect(mRoot, x, sPanelTop, w, 1, sAccentColour, 0.6f);

        // The container's name, and a warning when taking from it is a crime.
        const MWWorld::Ptr& container = mWindow.mPtr;
        std::string name = container.isEmpty() ? std::string() : escaped(container.getClass().getName(container));
        if (mYourItems)
            name = "#{Interface:LootPutIn} " + name;
        addText(mRoot, x + 20, sPanelTop + 10, w - 40, 34, name, sFontTitle, sBrightColour);
        std::string warning;
        if (mYourItems)
        {
            // The inventory screen's tabs, switched with the shoulder buttons.
            int tx = x + 20;
            addGlyph(mRoot, tx, sPanelTop + 48, 20, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
            addPointerTarget(
                mRoot, tx, sPanelTop + 44, 24, 28, nullptr, [] { pressButton(SDL_CONTROLLER_BUTTON_LEFTSHOULDER); });
            tx += 28;
            for (int tab = 0; tab < sInventoryTabCount; ++tab)
            {
                const bool selected = tab == mYourTab;
                MyGUI::TextBox* label = addText(mRoot, tx, sPanelTop + 42, 200, 30, inventoryTabKey(tab), sFontSmall,
                    selected ? sBrightColour : sDimColour);
                const int labelW = label->getTextSize().width + 16;
                label->setCoord(tx, sPanelTop + 42, labelW, 30);
                label->setTextAlign(MyGUI::Align::Center);
                if (selected)
                    addRect(mRoot, tx + 6, sPanelTop + 68, labelW - 12, 2, sAccentColour);
                else
                    addPointerTarget(mRoot, tx, sPanelTop + 42, labelW, 30, nullptr, [this, tab] {
                        mYourTab = tab;
                        mList.reset();
                        MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
                        update();
                    });
                tx += labelW + 2;
            }
            addGlyph(mRoot, tx + 4, sPanelTop + 48, 20, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
            addPointerTarget(mRoot, tx + 4, sPanelTop + 44, 24, 28, nullptr,
                [] { pressButton(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER); });
        }
        else if (isPickpocketing())
            warning = "#{Interface:LootPickpocketing}";
        else if (isStealing())
        {
            const std::string owner = ownerName(container);
            warning = owner.empty() ? std::string("#{Interface:LootOwned}")
                                    : "#{Interface:LootOwnedBy} " + owner + ".  #{Interface:LootStealingWarning}";
        }
        if (!warning.empty())
            addText(mRoot, x + 20, sPanelTop + 44, w - 40, 24, warning, sFontSmall, sWorseColour);
        // How much the player carries, since taking too much stops them moving.
        const MWWorld::Ptr actor = player();
        const std::string weight = "#{sEncumbrance}  "
            + formatNumber(std::floor(actor.getClass().getEncumbrance(actor))) + " / "
            + formatNumber(std::floor(actor.getClass().getCapacity(actor)));
        addText(mRoot, x + 20, sPanelTop + 14, w - 40, 30, weight, sFontSmall, sDimColour,
            MyGUI::Align::Right | MyGUI::Align::VCenter);
        const int contentTop = sPanelTop + 76;
        addRect(mRoot, x + 20, contentTop - 6, w - 40, 1, sAccentColour, 0.2f);

        const int listW = static_cast<int>(w * 0.54f);
        const std::vector<Row> rows = buildRows();
        drawList(x + 8, contentTop, listW - 8, sPanelBottom - 8 - contentTop, rows);
        // A faint line between the list and the details.
        addRect(mRoot, x + listW + 4, contentTop, 1, sPanelBottom - 16 - contentTop, sAccentColour, 0.15f);
        drawDetails(x + listW + 8, contentTop - 10, w - listW - 8, sPanelBottom - contentTop + 10, focusedItem());
    }

    void LootPanel::drawList(int x, int y, int w, int h, const std::vector<Row>& rows)
    {
        std::vector<ControllerList::Row> listRows;
        for (const Row& row : rows)
            listRows.push_back({ row.mItem < 0 ? sHeaderRowHeight : sEntryHeight, row.mItem >= 0 });

        const MWWorld::Ptr actor = player();
        const float spare = actor.getClass().getCapacity(actor) - actor.getClass().getEncumbrance(actor);
        const bool stealing = isStealing();
        const auto drawRow = [&](std::size_t i, int rowY, bool focused) {
            const Row& row = rows[i];
            if (row.mItem < 0)
            {
                addText(mRoot, x + 8, rowY, w - 16, 24, row.mHeader, sFontSmall, sBrightColour);
                addRect(mRoot, x + 8, rowY + 23, w - 16, 1, sAccentColour, 0.25f);
                return;
            }
            const ItemStack& item = mItems[row.mItem];
            const MWWorld::Ptr& ptr = item.mBase;
            const int rh = sEntryHeight - 2;
            if (focused)
            {
                addRect(mRoot, x, rowY, w, rh, sAccentColour, 0.16f);
                addRect(mRoot, x, rowY, 3, rh, sAccentColour);
            }
            addIcon(mRoot, x + 8, rowY + 6, rh - 12, ptr, false);
            const int textX = x + 8 + rh - 12 + 10;
            const int rightW = 110;

            std::string name = escaped(ptr.getClass().getName(ptr));
            if (item.mCount > 1)
                name += " (" + std::to_string(item.mCount) + ")";
            addText(mRoot, textX, rowY + 3, x + w - textX - rightW - 4, 26, name, sFontNormal,
                focused ? sActiveColour : sBrightColour);
            if (!ptr.getClass().isGold(ptr))
                addText(mRoot, textX, rowY + 27, x + w - textX - rightW - 4, 20, summary(ptr), sFontSmall, sDimColour);

            // Markers: an upgrade over what's equipped, and items that would be stolen. The player's own items show
            // which are equipped.
            std::string marker;
            MyGUI::Colour markerColour = sBetterColour;
            if (mYourItems)
            {
                if (item.mType == ItemStack::Type_Equipped)
                {
                    marker = "#{Interface:InventoryEquipped}";
                    markerColour = sDimColour;
                }
            }
            else if (stealing && !isPickpocketing())
            {
                marker = "#{Interface:LootStealing}";
                markerColour = sWorseColour;
            }
            else if (isUpgrade(item.mBase))
                marker = "#{Interface:LootBetter}";
            addText(mRoot, x + w - rightW - 8, rowY + 3, rightW, 26, marker, sFontSmall, markerColour,
                MyGUI::Align::Right | MyGUI::Align::VCenter);
            // The weight is red when taking the item would go over what the player can carry.
            const float itemWeight = ptr.getClass().getWeight(ptr) * static_cast<float>(item.mCount);
            if (itemWeight > 0.f)
                addText(mRoot, x + w - rightW - 8, rowY + 27, rightW, 20, "#{sWeight} " + formatNumber(itemWeight),
                    sFontSmall, !mYourItems && itemWeight > spare ? sWorseColour : sDimColour,
                    MyGUI::Align::Right | MyGUI::Align::VCenter);
        };
        // Pointing at an item shows its details, and clicking it takes it, or puts it in, as A does.
        if (!mList.draw(
                mRoot, x, y, w, h, listRows, drawRow, [this] { update(); },
                [] { pressButton(SDL_CONTROLLER_BUTTON_A); }))
            addText(mRoot, x + 12, y + 4, w - 24, 30,
                mYourItems ? "#{Interface:InventoryNoItems}" : "#{Interface:LootEmpty}", sFontNormal, sDimColour);
    }

    void LootPanel::drawDetails(int x, int y, int w, int h, int item)
    {
        if (item < 0)
            return;
        const ItemStack& stack = mItems[item];
        const MWWorld::Ptr& ptr = stack.mBase;
        const int headerBottom = drawHeader(mRoot, x, y, w, stack, mYourItems);

        // What the item would replace: the item in each slot it fits that holds one. The player's own items aren't
        // compared, since they're already theirs.
        std::vector<Compared> compared;
        if (isEquipment(ptr) && !mYourItems)
        {
            const std::vector<int> slots = ptr.getClass().getEquipmentSlots(ptr).first;
            Inventory& inventory = playerInventory();
            for (int slot : slots)
            {
                const auto equipped = inventory.getSlot(slot);
                if (equipped == inventory.end())
                {
                    // A free slot means the item replaces nothing.
                    compared.clear();
                    break;
                }
                // Items that fit two slots, such as rings, are compared with both, labelled by hand.
                std::string label = "#{Interface:InventoryEquipped}";
                if (slot == Inventory::Slot_LeftRing || slot == Inventory::Slot_LeftGauntlet)
                    label = "#{Interface:LootLeft}";
                else if (slot == Inventory::Slot_RightRing || slot == Inventory::Slot_RightGauntlet)
                    label = "#{Interface:LootRight}";
                compared.push_back(Compared{ label, *equipped });
            }
        }
        // What the buttons do.
        // Taking from an owned container, or from an NPC's pockets, is stealing, and the hints say so.
        const bool stealing = isStealing() || isPickpocketing();
        std::vector<std::pair<SDL_GameControllerButton, std::string>> hints;
        if (mYourItems)
            hints.emplace_back(SDL_CONTROLLER_BUTTON_A, "#{Interface:LootPutInAction}");
        else
            hints.emplace_back(
                SDL_CONTROLLER_BUTTON_A, stealing ? "#{Interface:LootSteal}" : "#{Interface:LootTake}");
        if (!mYourItems && !isPickpocketing())
            hints.emplace_back(
                SDL_CONTROLLER_BUTTON_Y, stealing ? "#{Interface:LootStealAll}" : "#{Interface:LootTakeAll}");
        if (!mYourItems && canTakeAndEquip(stack))
            hints.emplace_back(SDL_CONTROLLER_BUTTON_X,
                stealing ? "#{Interface:LootStealAndEquip}" : "#{Interface:LootTakeAndEquip}");
        if (!mYourItems && mWindow.mDisposeCorpseButton->getVisible())
            hints.emplace_back(SDL_CONTROLLER_BUTTON_BACK, "#{Interface:DisposeOfCorpse}");
        if (!mYourItems && canPutIn())
            hints.emplace_back(SDL_CONTROLLER_BUTTON_LEFTSTICK, "#{Interface:LootYourItems}");

        // The hints wrap onto as many rows as they need, and the rows end at the bottom of the panel.
        constexpr int rowHeight = 34;
        std::vector<std::pair<MyGUI::TextBox*, int>> placed;
        int hx = x + 16;
        int row = 0;
        for (const auto& [button, label] : hints)
        {
            MyGUI::TextBox* text = addText(mRoot, 0, 0, 220, 30, label, sFontNormal, sBrightColour);
            const int hintW = 30 + text->getTextSize().width + 22;
            if (hx + hintW > x + w && hx > x + 16)
            {
                hx = x + 16;
                ++row;
            }
            text->setPosition(hx + 30, row);
            placed.emplace_back(text, hx);
            hx += hintW;
        }
        const int firstRowY = y + h - 42 - row * rowHeight;
        for (std::size_t i = 0; i < placed.size(); ++i)
        {
            auto [text, textX] = placed[i];
            const int hy = firstRowY + text->getTop() * rowHeight;
            text->setPosition(textX + 30, hy - 3);
            addGlyph(mRoot, textX, hy + 1, 22, hints[i].first);
        }

        drawStats(mRoot, x, headerBottom + 4, w, firstRowY - 12, ptr, compared);
    }

    // ---------------------------------------------------------------------------------------------------------------
    // Input

    void LootPanel::onFrame(float duration)
    {
        if (mRoot == nullptr || !mRoot->getVisible())
            return;

        // The container can change while the panel is open, such as when a script adds items.
        mRefreshTimer -= duration;
        if (mRefreshTimer <= 0.f)
        {
            mRefreshTimer = 0.25f;
            readItems();
            if (currentState() != mShownState)
                update();
        }

        if (MWBase::Environment::get().getWindowManager()->getActiveControllerWindow() != &mWindow)
        {
            mList.resetHeld();
            return;
        }
        int itemCount = 0;
        for (const Row& row : buildRows())
            itemCount += row.mItem >= 0 ? 1 : 0;
        if (mList.updateHeld(duration, itemCount))
            update();
    }

    bool LootPanel::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
    {
        MWBase::WindowManager* winMgr = MWBase::Environment::get().getWindowManager();
        if (mYourItems)
        {
            switch (arg.button)
            {
                case SDL_CONTROLLER_BUTTON_A:
                    putIn();
                    break;
                case SDL_CONTROLLER_BUTTON_B:
                case SDL_CONTROLLER_BUTTON_LEFTSTICK:
                    switchView();
                    break;
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                {
                    const int delta = arg.button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER ? -1 : 1;
                    mYourTab = (mYourTab + delta + sInventoryTabCount) % sInventoryTabCount;
                    mList.reset();
                    winMgr->playSound(ESM::RefId::stringRefId("Menu Click"));
                    update();
                    break;
                }
                default:
                    break;
            }
            return true;
        }
        switch (arg.button)
        {
            case SDL_CONTROLLER_BUTTON_A:
                take(false);
                break;
            case SDL_CONTROLLER_BUTTON_LEFTSTICK:
                if (canPutIn())
                    switchView();
                break;
            case SDL_CONTROLLER_BUTTON_X:
                if (const int item = focusedItem(); item >= 0 && canTakeAndEquip(mItems[item]))
                    take(true);
                break;
            case SDL_CONTROLLER_BUTTON_Y:
                if (!isPickpocketing())
                    takeAll();
                break;
            case SDL_CONTROLLER_BUTTON_BACK:
                if (mWindow.mDisposeCorpseButton->getVisible())
                    mWindow.onDisposeCorpseButtonClicked(mWindow.mDisposeCorpseButton);
                break;
            case SDL_CONTROLLER_BUTTON_B:
                winMgr->removeGuiMode(GM_Container);
                break;
            default:
                // The d-pad is read every frame in onFrame, so it can repeat while held.
                break;
        }
        return true;
    }

    void LootPanel::take(bool equip)
    {
        const int index = focusedItem();
        ItemModel* model = mWindow.mModel;
        if (index < 0 || model == nullptr)
            return;
        const ItemStack item = mItems[index];
        // Pickpocketing can fail here, which ends it and closes the window.
        if (!model->onTakeItem(item.mBase, static_cast<int>(item.mCount)))
            return;
        const MWWorld::Ptr actor = player();
        InventoryItemModel playerModel(actor);
        MWBase::Environment::get().getWindowManager()->playSound(item.mBase.getClass().getUpSoundId(item.mBase));
        const MWWorld::Ptr taken = model->moveItem(item, item.mCount, &playerModel);
        if (equip && !taken.isEmpty())
            MWBase::Environment::get().getLuaManager()->useItem(taken, actor, false);

        // The window closes once the last item is taken.
        model->update();
        if (model->getItemCount() == 0)
        {
            MWBase::Environment::get().getWindowManager()->removeGuiMode(GM_Container);
            return;
        }
        update();
    }

    void LootPanel::takeAll()
    {
        // Taking everything from an owned container is an easy way to a bounty, so it asks first.
        if (isStealing())
        {
            MWBase::WindowManager* winMgr = MWBase::Environment::get().getWindowManager();
            ConfirmationDialog* dialog = winMgr->getConfirmationDialog();
            dialog->askForConfirmation("#{Interface:LootStealAllConfirmation}");
            dialog->eventOkClicked.clear();
            dialog->eventOkClicked += MyGUI::newDelegate(this, &LootPanel::onTakeAllConfirmed);
            dialog->eventCancelClicked.clear();
            return;
        }
        onTakeAllConfirmed();
    }

    void LootPanel::onTakeAllConfirmed()
    {
        mWindow.onTakeAllButtonClicked(mWindow.mTakeButton);
    }

    void LootPanel::switchView()
    {
        if (mYourItems)
        {
            mYourItems = false;
            mList.reset();
            mList.setFocus(mContainerFocus);
        }
        else
        {
            mYourItems = true;
            mContainerFocus = mList.getFocus();
            mList.reset();
        }
        MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
        update();
    }

    void LootPanel::putIn()
    {
        const int index = focusedItem();
        ItemModel* model = mWindow.mModel;
        if (index < 0 || model == nullptr)
            return;
        const ItemStack item = mItems[index];
        // The container can refuse, such as when it's full, and shows why.
        if (!model->onDropItem(item.mBase, static_cast<int>(item.mCount)))
            return;
        InventoryItemModel playerModel(player());
        playerModel.update();
        MWBase::Environment::get().getWindowManager()->playSound(item.mBase.getClass().getDownSoundId(item.mBase));
        // Moving an equipped item out of the inventory takes it off first.
        playerModel.moveItem(item, item.mCount, model);
        update();
    }
}
