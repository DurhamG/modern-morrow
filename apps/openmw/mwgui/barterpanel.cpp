#include "barterpanel.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include <MyGUI_Button.h>
#include <MyGUI_Gui.h>
#include <MyGUI_ILayer.h>
#include <MyGUI_LayerManager.h>

#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwmechanics/actorutil.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/containerstore.hpp"
#include "../mwworld/inventorystore.hpp"

#include "inventorywindow.hpp"
#include "itemdetails.hpp"
#include "tradeitemmodel.hpp"
#include "tradewindow.hpp"

namespace MWGui
{
    namespace
    {
        using namespace ControllerUi;
        using namespace ItemDetails;
        using Inventory = MWWorld::InventoryStore;

        // The panel's place on the canvas, which is 720 units tall, the same as the loot panel's.
        constexpr float sPanelLeft = 0.40f;
        constexpr int sPanelTop = 40;
        constexpr int sPanelBottom = 680;
        constexpr int sPanelMargin = 40;
        constexpr int sEntryHeight = 52;
        constexpr int sHeaderRowHeight = 28;

        // How often the offer changes while the right stick is held, in seconds.
        constexpr float sHaggleInterval = 0.06f;

        const MyGUI::Colour sPanelBackColour = MyGUI::Colour::parse("#0d0b08");

        MWWorld::Ptr player()
        {
            return MWMechanics::getPlayer();
        }

        int goldOf(const MWWorld::Ptr& actor)
        {
            return actor.getClass().getContainerStore(actor).count(MWWorld::ContainerStore::sGoldId);
        }

        // How many of an item a list of borrowed items holds.
        int borrowedCount(const std::vector<ItemStack>& borrowed, const MWWorld::Ptr& item)
        {
            for (const ItemStack& stack : borrowed)
                if (stack.mBase == item)
                    return static_cast<int>(stack.mCount);
            return 0;
        }
    }

    BarterPanel::BarterPanel(TradeWindow& window)
        : mWindow(window)
    {
    }

    BarterPanel::~BarterPanel()
    {
        if (mRoot != nullptr)
            MyGUI::Gui::getInstance().destroyWidget(mRoot);
    }

    TradeItemModel* BarterPanel::playerModel() const
    {
        return MWBase::Environment::get().getWindowManager()->getInventoryWindow()->getTradeModel();
    }

    void BarterPanel::show()
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
        mSelling = false;
        mFocus = 0;
        mScroll = 0;
        mHeldDirection.reset();
        update();
    }

    void BarterPanel::hide()
    {
        if (mRoot != nullptr)
            mRoot->setVisible(false);
        mEntries.clear();
    }

    int BarterPanel::unitPrice(const MWWorld::Ptr& item) const
    {
        return TradeWindow::getTradePrice(mWindow.mPtr, item, 1, !mSelling);
    }

    void BarterPanel::readEntries()
    {
        mEntries.clear();
        TradeItemModel* merchantModel = mWindow.mTradeModel;
        TradeItemModel* ownModel = playerModel();
        // The window opens before it knows the merchant, and the panel draws again once it does.
        if (merchantModel == nullptr || ownModel == nullptr || mWindow.mPtr.isEmpty())
            return;
        merchantModel->update();
        ownModel->update();

        // The list shows one side's items: what's left of them, and how many are in the deal. Items wholly in the
        // deal are only in the other side's model, so they're added from there.
        TradeItemModel* source = mSelling ? ownModel : merchantModel;
        TradeItemModel* other = mSelling ? merchantModel : ownModel;
        const std::vector<ItemStack> inDeal = other->getItemsBorrowedToUs();
        const int services = mWindow.getMerchantServices();
        for (std::size_t i = 0; i < source->getItemCount(); ++i)
        {
            const ItemStack stack = source->getItem(static_cast<ItemModel::ModelIndex>(i));
            if (stack.mType == ItemStack::Type_Barter)
                continue;
            if (mSelling)
            {
                // Only what this merchant buys can be sold, and never gold or summoned items.
                const MWWorld::Ptr& ptr = stack.mBase;
                if (ptr.getClass().isGold(ptr) || (stack.mFlags & ItemStack::Flag_Bound)
                    || !ptr.getClass().canSell(ptr, services))
                    continue;
            }
            Entry entry;
            entry.mStack = stack;
            entry.mAvailable = static_cast<int>(stack.mCount);
            entry.mInDeal = borrowedCount(inDeal, stack.mBase);
            mEntries.push_back(std::move(entry));
        }
        for (const ItemStack& stack : inDeal)
        {
            const bool listed = std::any_of(
                mEntries.begin(), mEntries.end(), [&](const Entry& entry) { return entry.mStack.mBase == stack.mBase; });
            if (listed)
                continue;
            Entry entry;
            entry.mStack = stack;
            entry.mStack.mType = ItemStack::Type_Normal;
            entry.mInDeal = static_cast<int>(stack.mCount);
            mEntries.push_back(std::move(entry));
        }
    }

    std::vector<BarterPanel::Row> BarterPanel::buildRows() const
    {
        // Items are grouped by kind, with the most valuable first in each group.
        std::vector<int> order(mEntries.size());
        for (std::size_t i = 0; i < order.size(); ++i)
            order[i] = static_cast<int>(i);
        std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
            const MWWorld::Ptr& left = mEntries[a].mStack.mBase;
            const MWWorld::Ptr& right = mEntries[b].mStack.mBase;
            if (itemGroup(left) != itemGroup(right))
                return itemGroup(left) < itemGroup(right);
            return left.getClass().getValue(left) > right.getClass().getValue(right);
        });
        int groups = 0;
        for (std::size_t i = 0; i < order.size(); ++i)
        {
            const MWWorld::Ptr& item = mEntries[order[i]].mStack.mBase;
            if (i == 0 || itemGroup(item) != itemGroup(mEntries[order[i - 1]].mStack.mBase))
                ++groups;
        }
        std::vector<Row> rows;
        for (std::size_t i = 0; i < order.size(); ++i)
        {
            const ItemGroup group = itemGroup(mEntries[order[i]].mStack.mBase);
            if (groups > 1 && (i == 0 || group != itemGroup(mEntries[order[i - 1]].mStack.mBase)))
                rows.push_back(Row{ itemGroupKey(group), -1 });
            rows.push_back(Row{ {}, order[i] });
        }
        return rows;
    }

    int BarterPanel::focusedEntry() const
    {
        int index = 0;
        for (const Row& row : buildRows())
        {
            if (row.mEntry < 0)
                continue;
            if (index++ == mFocus)
                return row.mEntry;
        }
        return -1;
    }

    void BarterPanel::update()
    {
        if (mRoot == nullptr || !mRoot->getVisible() || mWindow.mPtr.isEmpty())
            return;
        readEntries();
        while (mRoot->getChildCount() > 0)
            MyGUI::Gui::getInstance().destroyWidget(mRoot->getChildAt(0));

        const MyGUI::IntSize size = mRoot->getSize();
        const int x = static_cast<int>(size.width * sPanelLeft);
        const int w = size.width - sPanelMargin - x;
        addRect(mRoot, x, sPanelTop, w, sPanelBottom - sPanelTop, sPanelBackColour);
        addRect(mRoot, x, sPanelTop, w, 1, sAccentColour, 0.6f);

        // The merchant's name, both sides' gold, and the Buy and Sell tabs.
        const MWWorld::Ptr& merchant = mWindow.mPtr;
        const std::string name = merchant.isEmpty() ? std::string() : escaped(merchant.getClass().getName(merchant));
        addText(mRoot, x + 20, sPanelTop + 10, w - 40, 34, name, sFontTitle, sBrightColour);
        const std::string gold = "#{Interface:BarterYourGold}  " + std::to_string(goldOf(player()))
            + "      #{Interface:BarterMerchantGold}  " + std::to_string(mWindow.getMerchantGold());
        addText(mRoot, x + 20, sPanelTop + 14, w - 40, 30, gold, sFontSmall, sDimColour,
            MyGUI::Align::Right | MyGUI::Align::VCenter);
        int tx = x + 20;
        addGlyph(mRoot, tx, sPanelTop + 48, 20, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
        tx += 28;
        for (const bool selling : { false, true })
        {
            const bool selected = selling == mSelling;
            MyGUI::TextBox* label = addText(mRoot, tx, sPanelTop + 42, 200, 30,
                selling ? "#{Interface:BarterSell}" : "#{Interface:BarterBuy}", sFontNormal,
                selected ? sBrightColour : sDimColour);
            const int labelW = label->getTextSize().width + 20;
            label->setCoord(tx, sPanelTop + 42, labelW, 30);
            label->setTextAlign(MyGUI::Align::Center);
            if (selected)
                addRect(mRoot, tx + 6, sPanelTop + 68, labelW - 12, 2, sAccentColour);
            tx += labelW + 4;
        }
        addGlyph(mRoot, tx + 2, sPanelTop + 48, 20, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
        const int contentTop = sPanelTop + 82;
        addRect(mRoot, x + 20, contentTop - 6, w - 40, 1, sAccentColour, 0.2f);

        const int listW = static_cast<int>(w * 0.54f);
        drawList(x + 8, contentTop, listW - 8, sPanelBottom - 8 - contentTop, buildRows());
        addRect(mRoot, x + listW + 4, contentTop, 1, sPanelBottom - 16 - contentTop, sAccentColour, 0.15f);
        drawDetails(x + listW + 8, contentTop - 10, w - listW - 8, sPanelBottom - contentTop + 10, focusedEntry());
    }

    void BarterPanel::drawList(int x, int y, int w, int h, const std::vector<Row>& rows)
    {
        int entryCount = 0;
        for (const Row& row : rows)
            entryCount += row.mEntry >= 0 ? 1 : 0;
        if (entryCount == 0)
        {
            addText(mRoot, x + 12, y + 4, w - 24, 30,
                mSelling ? "#{Interface:BarterNothingToSell}" : "#{Interface:BarterNothingForSale}", sFontNormal,
                sDimColour);
            return;
        }
        mFocus = std::clamp(mFocus, 0, entryCount - 1);

        // Scroll so the focused row is in view.
        const int listBottom = y + h;
        int focusRow = 0;
        for (int i = 0, index = 0; i < static_cast<int>(rows.size()); ++i)
        {
            if (rows[i].mEntry < 0)
                continue;
            if (index++ == mFocus)
                focusRow = i;
        }
        const auto rowHeight = [&](int i) { return rows[i].mEntry < 0 ? sHeaderRowHeight : sEntryHeight; };
        if (focusRow < mScroll)
            mScroll = focusRow > 0 && rows[focusRow - 1].mEntry < 0 ? focusRow - 1 : focusRow;
        while (true)
        {
            int bottomOfFocus = y;
            for (int i = mScroll; i <= focusRow; ++i)
                bottomOfFocus += rowHeight(i);
            if (bottomOfFocus <= listBottom || mScroll >= focusRow)
                break;
            ++mScroll;
        }

        int rowY = y;
        int index = 0;
        for (int i = 0; i < mScroll; ++i)
            index += rows[i].mEntry >= 0 ? 1 : 0;
        for (int i = mScroll; i < static_cast<int>(rows.size()); ++i)
        {
            const Row& row = rows[i];
            if (rowY + rowHeight(i) > listBottom)
                break;
            if (row.mEntry < 0)
            {
                addText(mRoot, x + 8, rowY, w - 16, 24, row.mHeader, sFontSmall, sBrightColour);
                addRect(mRoot, x + 8, rowY + 23, w - 16, 1, sAccentColour, 0.25f);
                rowY += sHeaderRowHeight;
                continue;
            }
            const Entry& entry = mEntries[row.mEntry];
            const MWWorld::Ptr& ptr = entry.mStack.mBase;
            const bool focused = index == mFocus;
            const int rh = sEntryHeight - 2;
            if (entry.mInDeal > 0)
                addRect(mRoot, x, rowY, w, rh, sNewColour, 0.08f);
            if (focused)
            {
                addRect(mRoot, x, rowY, w, rh, sAccentColour, 0.16f);
                addRect(mRoot, x, rowY, 3, rh, sAccentColour);
            }
            addIcon(mRoot, x + 8, rowY + 6, rh - 12, ptr, entry.mStack.mType == ItemStack::Type_Equipped);
            const int textX = x + 8 + rh - 12 + 10;
            const int rightW = 120;
            std::string label = escaped(ptr.getClass().getName(ptr));
            const int total = entry.mAvailable + entry.mInDeal;
            if (total > 1)
                label += " (" + std::to_string(total) + ")";
            addText(mRoot, textX, rowY + 3, x + w - textX - rightW - 4, 26, label, sFontNormal,
                focused ? sActiveColour : sBrightColour);
            addText(mRoot, textX, rowY + 27, x + w - textX - rightW - 4, 20, summary(ptr), sFontSmall, sDimColour);

            // The price of one, and how many are in the deal, or whether the item is an upgrade.
            addText(mRoot, x + w - rightW - 8, rowY + 3, rightW, 26,
                std::to_string(unitPrice(ptr)) + " #{Interface:BarterGoldShort}", sFontNormal, sTextColour,
                MyGUI::Align::Right | MyGUI::Align::VCenter);
            std::string marker;
            MyGUI::Colour markerColour = sNewColour;
            if (entry.mInDeal > 0)
                marker = (mSelling ? "#{Interface:BarterSelling} " : "#{Interface:BarterBuying} ")
                    + (total > 1 ? std::to_string(entry.mInDeal) : std::string());
            else if (mSelling && entry.mStack.mType == ItemStack::Type_Equipped)
            {
                marker = "#{Interface:InventoryEquipped}";
                markerColour = sDimColour;
            }
            else if (!mSelling && isUpgrade(ptr))
            {
                marker = "#{Interface:LootBetter}";
                markerColour = sBetterColour;
            }
            addText(mRoot, x + w - rightW - 8, rowY + 27, rightW, 20, marker, sFontSmall, markerColour,
                MyGUI::Align::Right | MyGUI::Align::VCenter);
            rowY += sEntryHeight;
            ++index;
        }
    }

    void BarterPanel::drawDetails(int x, int y, int w, int h, int entryIndex)
    {
        // The deal and the offer, along the bottom of the details, above the button hints.
        TradeItemModel* ownModel = playerModel();
        int buying = 0;
        int selling = 0;
        if (ownModel != nullptr && mWindow.mTradeModel != nullptr)
        {
            for (const ItemStack& stack : ownModel->getItemsBorrowedToUs())
                buying += static_cast<int>(stack.mCount);
            for (const ItemStack& stack : mWindow.mTradeModel->getItemsBorrowedToUs())
                selling += static_cast<int>(stack.mCount);
        }
        const bool hasDeal = buying + selling > 0;
        const int balance = mWindow.mCurrentBalance;
        const int asked = mWindow.mCurrentMerchantOffer;

        std::vector<std::pair<SDL_GameControllerButton, std::string>> hints;
        if (entryIndex >= 0)
        {
            const Entry& entry = mEntries[entryIndex];
            const bool inDeal = entry.mInDeal > 0;
            hints.emplace_back(SDL_CONTROLLER_BUTTON_A,
                inDeal ? "#{Interface:BarterRemove}"
                       : (mSelling ? "#{Interface:BarterSellAction}" : "#{Interface:BarterBuyAction}"));
            if (entry.mAvailable + entry.mInDeal > 1)
                hints.emplace_back(SDL_CONTROLLER_BUTTON_DPAD_LEFT, "#{Interface:BarterOneMoreLess}");
        }
        const std::vector<Row> rows = buildRows();
        if (std::count_if(rows.begin(), rows.end(), [](const Row& row) { return row.mEntry < 0; }) > 1)
            hints.emplace_back(SDL_CONTROLLER_BUTTON_Y, "#{Interface:MagicNextGroup}");
        if (hasDeal)
        {
            hints.emplace_back(SDL_CONTROLLER_BUTTON_X, "#{Interface:BarterOffer}");
            hints.emplace_back(SDL_CONTROLLER_BUTTON_RIGHTSTICK, "#{Interface:BarterHaggle}");
        }

        // The hints wrap onto as many rows as they need, ending at the bottom of the panel.
        constexpr int rowHeight = 34;
        std::vector<std::pair<MyGUI::TextBox*, int>> placed;
        int hx = x + 16;
        int row = 0;
        for (const auto& [button, label] : hints)
        {
            MyGUI::TextBox* text = addText(mRoot, 0, 0, 240, 30, label, sFontNormal, sBrightColour);
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

        int dealTop = firstRowY - 12;
        if (hasDeal)
        {
            dealTop -= 110;
            addRect(mRoot, x + 12, dealTop, w - 24, 100, sAccentColour, 0.08f);
            addRect(mRoot, x + 12, dealTop, w - 24, 1, sAccentColour, 0.3f);
            std::string items;
            if (buying > 0)
                items = "#{Interface:BarterBuying} " + std::to_string(buying);
            if (selling > 0)
                items += (items.empty() ? "" : ",  ") + std::string("#{Interface:BarterSelling} ")
                    + std::to_string(selling);
            addText(mRoot, x + 24, dealTop + 6, w - 48, 24, items, sFontSmall, sDimColour);
            // A negative balance is what the player pays, and a positive one what they receive.
            const std::string payOrGet = balance < 0 ? "#{Interface:BarterYouPay}" : "#{Interface:BarterYouGet}";
            addText(mRoot, x + 24, dealTop + 32, w - 48, 30, payOrGet, sFontNormal, sTextColour);
            const bool haggled = balance != asked;
            addText(mRoot, x + 24, dealTop + 32, w - 48, 30, std::to_string(std::abs(balance)), sFontLarge,
                haggled ? sNewColour : sBrightColour, MyGUI::Align::Right | MyGUI::Align::VCenter);
            const std::string askedText = (asked < 0 ? "#{Interface:BarterMerchantAsks} "
                                                     : "#{Interface:BarterMerchantOffers} ")
                + std::to_string(std::abs(asked));
            addText(mRoot, x + 24, dealTop + 66, w - 48, 24, askedText, sFontSmall, sDimColour);
            // Paying more gold than the player has, or asking more than the merchant has, can't work.
            const bool short_ = (balance < 0 && goldOf(player()) < -balance)
                || (balance > 0 && mWindow.getMerchantGold() < balance);
            if (short_)
                addText(mRoot, x + 24, dealTop + 66, w - 48, 24, "#{Interface:BarterNotEnoughGold}", sFontSmall,
                    sWorseColour, MyGUI::Align::Right | MyGUI::Align::VCenter);
        }

        if (entryIndex < 0)
            return;
        const Entry& entry = mEntries[entryIndex];
        const MWWorld::Ptr& ptr = entry.mStack.mBase;
        ItemStack shown = entry.mStack;
        shown.mCount = entry.mAvailable + entry.mInDeal;
        drawHeader(mRoot, x, y, w, shown, mSelling);

        // What a bought item would replace, as on the loot panel.
        std::vector<Compared> compared;
        if (!mSelling && isEquipment(ptr))
        {
            const MWWorld::Ptr actor = player();
            Inventory& inventory = actor.getClass().getInventoryStore(actor);
            for (int slot : ptr.getClass().getEquipmentSlots(ptr).first)
            {
                const auto equipped = inventory.getSlot(slot);
                if (equipped == inventory.end())
                {
                    compared.clear();
                    break;
                }
                std::string label = "#{Interface:InventoryEquipped}";
                if (slot == Inventory::Slot_LeftRing || slot == Inventory::Slot_LeftGauntlet)
                    label = "#{Interface:LootLeft}";
                else if (slot == Inventory::Slot_RightRing || slot == Inventory::Slot_RightGauntlet)
                    label = "#{Interface:LootRight}";
                compared.push_back(Compared{ label, *equipped });
            }
        }
        const int priceY = y + 104;
        addText(mRoot, x + 16, priceY, w - 32, 28,
            mSelling ? "#{Interface:BarterSellPrice}" : "#{Interface:BarterBuyPrice}", sFontNormal, sTextColour);
        addText(mRoot, x + 16, priceY, w - 32, 28, std::to_string(unitPrice(ptr)), sFontNormal, sBrightColour,
            MyGUI::Align::Right | MyGUI::Align::VCenter);
        drawStats(mRoot, x, priceY + 34, w, dealTop - 8, ptr, compared);
    }

    // ---------------------------------------------------------------------------------------------------------------
    // Input

    void BarterPanel::onFrame(float duration)
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
        if (mHeldDirection.update(duration, dx, dy))
        {
            if (dy != 0)
            {
                int entryCount = 0;
                for (const Row& row : buildRows())
                    entryCount += row.mEntry >= 0 ? 1 : 0;
                if (entryCount > 0)
                    mFocus = std::clamp(mFocus + dy, 0, entryCount - 1);
                update();
            }
            else if (const int entry = focusedEntry(); entry >= 0)
                changeDeal(entry, dx);
        }

        // The right stick changes the offer while it's held, faster the further it's pushed.
        const float stick = MWBase::Environment::get().getInputManager()->getControllerAxisValue(
            SDL_CONTROLLER_AXIS_RIGHTX);
        if (std::abs(stick) < 0.3f)
        {
            mHaggleTimer = 0.f;
            return;
        }
        mHaggleTimer -= duration;
        if (mHaggleTimer > 0.f)
            return;
        mHaggleTimer = std::abs(stick) > 0.85f ? sHaggleInterval / 3.f : sHaggleInterval;
        haggle(stick > 0 ? 1 : -1);
    }

    bool BarterPanel::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
    {
        MWBase::WindowManager* winMgr = MWBase::Environment::get().getWindowManager();
        switch (arg.button)
        {
            case SDL_CONTROLLER_BUTTON_A:
                if (const int entry = focusedEntry(); entry >= 0)
                {
                    // A puts the whole stack into the deal, or takes it all back out.
                    const Entry& item = mEntries[entry];
                    changeDeal(entry, item.mInDeal > 0 ? -item.mInDeal : item.mAvailable);
                }
                break;
            case SDL_CONTROLLER_BUTTON_X:
                mWindow.updateLabels();
                mWindow.onOfferButtonClicked(mWindow.mOfferButton);
                update();
                break;
            case SDL_CONTROLLER_BUTTON_Y:
                nextGroup();
                break;
            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                switchTab();
                winMgr->playSound(ESM::RefId::stringRefId("Menu Click"));
                break;
            case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
                // Pressing the stick goes back to the merchant's price.
                mWindow.mCurrentBalance = mWindow.mCurrentMerchantOffer;
                mWindow.updateLabels();
                update();
                break;
            case SDL_CONTROLLER_BUTTON_B:
                mWindow.onCancelButtonClicked(mWindow.mCancelButton);
                break;
            default:
                // The d-pad is read every frame in onFrame, so it can repeat while held.
                break;
        }
        return true;
    }

    bool BarterPanel::onControllerThumbstickEvent(const SDL_ControllerAxisEvent& arg)
    {
        return arg.axis == SDL_CONTROLLER_AXIS_LEFTX || arg.axis == SDL_CONTROLLER_AXIS_LEFTY
            || arg.axis == SDL_CONTROLLER_AXIS_RIGHTX || arg.axis == SDL_CONTROLLER_AXIS_RIGHTY;
    }

    void BarterPanel::switchTab()
    {
        mSelling = !mSelling;
        mFocus = 0;
        mScroll = 0;
        update();
    }

    void BarterPanel::changeDeal(int entryIndex, int count)
    {
        TradeItemModel* merchantModel = mWindow.mTradeModel;
        TradeItemModel* ownModel = playerModel();
        if (merchantModel == nullptr || ownModel == nullptr || count == 0)
            return;
        const Entry entry = mEntries[entryIndex];
        count = std::clamp(count, -entry.mInDeal, entry.mAvailable);
        if (count == 0)
            return;
        const MWWorld::Ptr item = entry.mStack.mBase;
        // The side the item comes from, and the side that takes it in the deal.
        TradeItemModel* source = mSelling ? ownModel : merchantModel;
        TradeItemModel* other = mSelling ? merchantModel : ownModel;

        const auto indexIn = [&](TradeItemModel* model, bool borrowed) -> int {
            for (std::size_t i = 0; i < model->getItemCount(); ++i)
            {
                const ItemStack stack = model->getItem(static_cast<ItemModel::ModelIndex>(i));
                if (stack.mBase == item && (stack.mType == ItemStack::Type_Barter) == borrowed)
                    return static_cast<int>(i);
            }
            return -1;
        };
        if (count > 0)
        {
            const int index = indexIn(source, false);
            if (index < 0)
                return;
            other->borrowItemToUs(index, source, count);
            source->borrowItemFromUs(index, count);
        }
        else
        {
            const int index = indexIn(other, true);
            if (index < 0)
                return;
            source->returnItemBorrowedFromUs(index, other, -count);
            other->returnItemBorrowedToUs(index, -count);
        }
        MWBase::Environment::get().getWindowManager()->playSound(
            count > 0 ? item.getClass().getUpSoundId(item) : item.getClass().getDownSoundId(item));
        merchantModel->update();
        ownModel->update();
        mWindow.updateOffer();
        update();
    }

    void BarterPanel::haggle(int direction)
    {
        // Right asks for more gold, or offers to pay less. The step grows with the deal, so haggling a big deal
        // doesn't take long.
        if (mWindow.mCurrentMerchantOffer == 0)
            return;
        const int step = std::max(1, std::abs(mWindow.mCurrentMerchantOffer) / 50);
        mWindow.mCurrentBalance += direction * step;
        // The offer can't cross zero: the player who buys pays something, and the one who sells gets something.
        if (mWindow.mCurrentMerchantOffer < 0)
            mWindow.mCurrentBalance = std::min(mWindow.mCurrentBalance, -1);
        else
            mWindow.mCurrentBalance = std::max(mWindow.mCurrentBalance, 1);
        mWindow.updateLabels();
        update();
    }

    void BarterPanel::nextGroup()
    {
        // Moves the selection to the first item of the next group, with its header at the top of the list.
        const std::vector<Row> rows = buildRows();
        std::vector<std::pair<int, int>> groupStarts;
        for (int i = 0, index = 0; i < static_cast<int>(rows.size()); ++i)
        {
            if (rows[i].mEntry < 0)
            {
                groupStarts.emplace_back(index, i);
                continue;
            }
            ++index;
        }
        if (groupStarts.size() < 2)
            return;
        auto next = std::find_if(
            groupStarts.begin(), groupStarts.end(), [&](const auto& start) { return start.first > mFocus; });
        if (next == groupStarts.end())
            next = groupStarts.begin();
        mFocus = next->first;
        mScroll = next->second;
        MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
        update();
    }
}
