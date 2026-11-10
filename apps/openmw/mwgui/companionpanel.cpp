#include "companionpanel.hpp"

#include <algorithm>
#include <cmath>

#include <MyGUI_Gui.h>
#include <MyGUI_ILayer.h>
#include <MyGUI_LayerManager.h>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwmechanics/actorutil.hpp"

#include "../mwworld/class.hpp"

#include "companionitemmodel.hpp"
#include "companionwindow.hpp"
#include "inventoryitemmodel.hpp"
#include "itemdetails.hpp"

namespace MWGui
{
    namespace
    {
        using namespace ControllerUi;
        using namespace ItemDetails;

        // The panel's place on the canvas, which is 720 units tall, the same as the loot panel's.
        constexpr float sPanelLeft = 0.40f;
        constexpr int sPanelTop = 40;
        constexpr int sPanelBottom = 680;
        constexpr int sPanelMargin = 40;
        constexpr int sEntryHeight = 52;
        constexpr int sHeaderRowHeight = 28;

        const MyGUI::Colour sPanelBackColour = MyGUI::Colour::parse("#0d0b08");

        int getProfit(const MWWorld::Ptr& actor)
        {
            const ESM::RefId& script = actor.getClass().getScript(actor);
            if (script.empty())
                return 0;
            return actor.getRefData().getLocals().getIntVar(script, "minimumprofit");
        }

        std::string carrying(const MWWorld::Ptr& actor)
        {
            return formatNumber(std::floor(actor.getClass().getEncumbrance(actor))) + " / "
                + formatNumber(std::floor(actor.getClass().getCapacity(actor)));
        }
    }

    CompanionPanel::CompanionPanel(CompanionWindow& window)
        : mWindow(window)
    {
    }

    CompanionPanel::~CompanionPanel()
    {
        if (mRoot != nullptr)
            MyGUI::Gui::getInstance().destroyWidget(mRoot);
    }

    void CompanionPanel::show()
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
        mYourItems = false;
        mList.reset();
        mList.resetHeld();
        update();
    }

    void CompanionPanel::hide()
    {
        if (mRoot != nullptr)
            mRoot->setVisible(false);
        mItems.clear();
    }

    void CompanionPanel::readItems()
    {
        mItems.clear();
        if (mYourItems)
        {
            InventoryItemModel model(MWMechanics::getPlayer());
            model.update();
            for (std::size_t i = 0; i < model.getItemCount(); ++i)
                mItems.push_back(model.getItem(static_cast<ItemModel::ModelIndex>(i)));
            return;
        }
        if (mWindow.mModel == nullptr)
            return;
        mWindow.mModel->update();
        for (std::size_t i = 0; i < mWindow.mModel->getItemCount(); ++i)
            mItems.push_back(mWindow.mModel->getItem(static_cast<ItemModel::ModelIndex>(i)));
    }

    std::vector<CompanionPanel::Row> CompanionPanel::buildRows() const
    {
        // The items by kind, with the most valuable first in each group.
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
        std::vector<Row> rows;
        for (std::size_t i = 0; i < order.size(); ++i)
        {
            const ItemGroup group = itemGroup(mItems[order[i]].mBase);
            if (i == 0 || group != itemGroup(mItems[order[i - 1]].mBase))
                rows.push_back(Row{ itemGroupKey(group), -1 });
            rows.push_back(Row{ {}, order[i] });
        }
        return rows;
    }

    int CompanionPanel::focusedItem() const
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

    // ---------------------------------------------------------------------------------------------------------------
    // Drawing

    void CompanionPanel::update()
    {
        if (mRoot == nullptr || !mRoot->getVisible() || mWindow.mPtr.isEmpty())
            return;
        readItems();
        while (mRoot->getChildCount() > 0)
            MyGUI::Gui::getInstance().destroyWidget(mRoot->getChildAt(0));

        const MyGUI::IntSize size = mRoot->getSize();
        const int x = static_cast<int>(size.width * sPanelLeft);
        const int w = size.width - sPanelMargin - x;
        addRect(mRoot, x, sPanelTop, w, sPanelBottom - sPanelTop, sPanelBackColour);
        addRect(mRoot, x, sPanelTop, w, 1, sAccentColour, 0.6f);

        // The companion's name and how much they carry, the profit when they keep count, and the two tabs.
        const MWWorld::Ptr& companion = mWindow.mPtr;
        addText(mRoot, x + 20, sPanelTop + 10, w - 40, 34, escaped(companion.getClass().getName(companion)), sFontTitle,
            sBrightColour);
        std::string status = "#{sEncumbrance}  " + carrying(companion);
        if (mWindow.mModel != nullptr && mWindow.mModel->hasProfit(companion))
            status = "#{sProfitValue} " + std::to_string(getProfit(companion)) + "      " + status;
        addText(mRoot, x + 20, sPanelTop + 14, w - 40, 30, status, sFontSmall, sDimColour,
            MyGUI::Align::Right | MyGUI::Align::VCenter);
        int tx = x + 20;
        addGlyph(mRoot, tx, sPanelTop + 48, 20, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
        addPointerTarget(
            mRoot, tx, sPanelTop + 44, 24, 28, nullptr, [] { pressButton(SDL_CONTROLLER_BUTTON_LEFTSHOULDER); });
        tx += 28;
        for (const bool yours : { false, true })
        {
            const bool selected = yours == mYourItems;
            MyGUI::TextBox* label = addText(mRoot, tx, sPanelTop + 42, 200, 30,
                yours ? "#{Interface:CompanionYourItems}" : "#{Interface:CompanionTheirItems}", sFontNormal,
                selected ? sBrightColour : sDimColour);
            const int labelW = label->getTextSize().width + 20;
            label->setCoord(tx, sPanelTop + 42, labelW, 30);
            label->setTextAlign(MyGUI::Align::Center);
            if (selected)
                addRect(mRoot, tx + 6, sPanelTop + 68, labelW - 12, 2, sAccentColour);
            else
            {
                // Either shoulder button switches to the other tab, so clicking that tab presses one.
                addPointerTarget(mRoot, tx, sPanelTop + 42, labelW, 30, nullptr,
                    [] { pressButton(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER); });
            }
            tx += labelW + 4;
        }
        addGlyph(mRoot, tx + 2, sPanelTop + 48, 20, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
        addPointerTarget(
            mRoot, tx + 2, sPanelTop + 44, 24, 28, nullptr, [] { pressButton(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER); });
        const int contentTop = sPanelTop + 82;
        addRect(mRoot, x + 20, contentTop - 6, w - 40, 1, sAccentColour, 0.2f);

        const int listW = static_cast<int>(w * 0.54f);
        drawList(x + 8, contentTop, listW - 8, sPanelBottom - 8 - contentTop, buildRows());
        addRect(mRoot, x + listW + 4, contentTop, 1, sPanelBottom - 16 - contentTop, sAccentColour, 0.15f);

        // The highlighted item's details, and the button hints below them.
        const int dx = x + listW + 8;
        const int dw = w - listW - 8;
        const int hintY = sPanelBottom - 42;
        const int item = focusedItem();
        if (item >= 0)
        {
            drawHeader(mRoot, dx, contentTop - 10, dw, mItems[item], mYourItems);
            drawStats(mRoot, dx, contentTop + 94, dw, hintY - 12, mItems[item].mBase, {});
        }
        int hx = dx + 16;
        const auto hint = [&](SDL_GameControllerButton button, const std::string& label) {
            addGlyph(mRoot, hx, hintY + 1, 22, button);
            MyGUI::TextBox* text = addText(mRoot, hx + 30, hintY - 3, 200, 30, label, sFontNormal, sBrightColour);
            // Clicking the hint presses its button.
            addPointerTarget(mRoot, hx, hintY - 4, 30 + text->getTextSize().width + 8, 32, nullptr,
                [button] { pressButton(button); });
            hx += 30 + text->getTextSize().width + 22;
        };
        if (item >= 0)
            hint(SDL_CONTROLLER_BUTTON_A, mYourItems ? "#{Interface:CompanionGive}" : "#{Interface:Take}");
    }

    void CompanionPanel::drawList(int x, int y, int w, int h, const std::vector<Row>& rows)
    {
        std::vector<ControllerList::Row> listRows;
        for (const Row& row : rows)
            listRows.push_back({ row.mItem < 0 ? sHeaderRowHeight : sEntryHeight, row.mItem >= 0 });

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
            addIcon(mRoot, x + 8, rowY + 6, rh - 12, ptr, item.mType == ItemStack::Type_Equipped);
            const int textX = x + 8 + rh - 12 + 10;
            std::string name = escaped(ptr.getClass().getName(ptr));
            if (item.mCount > 1)
                name += " (" + std::to_string(item.mCount) + ")";
            addText(mRoot, textX, rowY + 3, x + w - textX - 4, 26, name, sFontNormal,
                focused ? sActiveColour : sBrightColour);
            if (!ptr.getClass().isGold(ptr))
                addText(mRoot, textX, rowY + 27, x + w - textX - 4, 20, summary(ptr), sFontSmall, sDimColour);
        };
        // Pointing at an item shows its details, and clicking it takes it, or gives it, as A does.
        if (!mList.draw(
                mRoot, x, y, w, h, listRows, drawRow, [this] { update(); },
                [] { pressButton(SDL_CONTROLLER_BUTTON_A); }))
            addText(mRoot, x + 12, y + 4, w - 24, 30, "#{Interface:InventoryNoItems}", sFontNormal, sDimColour);
    }

    // ---------------------------------------------------------------------------------------------------------------
    // Input

    void CompanionPanel::move()
    {
        const int index = focusedItem();
        CompanionItemModel* companionModel = mWindow.mModel;
        if (index < 0 || companionModel == nullptr)
            return;
        const ItemStack item = mItems[index];
        // Summoned items can't change hands.
        if (item.mFlags & ItemStack::Flag_Bound)
        {
            MWBase::Environment::get().getWindowManager()->messageBox("#{sBarterDialog12}");
            return;
        }
        InventoryItemModel playerModel(MWMechanics::getPlayer());
        playerModel.update();
        MWBase::WindowManager* winMgr = MWBase::Environment::get().getWindowManager();
        if (mYourItems)
        {
            // The companion can refuse, such as when they can't carry more, and says why.
            if (!companionModel->onDropItem(item.mBase, static_cast<int>(item.mCount)))
                return;
            winMgr->playSound(item.mBase.getClass().getDownSoundId(item.mBase));
            playerModel.moveItem(item, item.mCount, companionModel);
        }
        else
        {
            winMgr->playSound(item.mBase.getClass().getUpSoundId(item.mBase));
            companionModel->moveItem(item, item.mCount, &playerModel);
        }
        update();
    }

    void CompanionPanel::onFrame(float duration)
    {
        if (mRoot == nullptr || !mRoot->getVisible())
            return;
        if (MWBase::Environment::get().getWindowManager()->getActiveControllerWindow() != &mWindow)
        {
            mList.resetHeld();
            return;
        }
        if (mList.updateHeld(duration, static_cast<int>(mItems.size())))
            update();
    }

    bool CompanionPanel::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
    {
        switch (arg.button)
        {
            case SDL_CONTROLLER_BUTTON_A:
                move();
                break;
            case SDL_CONTROLLER_BUTTON_B:
                mWindow.onCloseButtonClicked(nullptr);
                break;
            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                mYourItems = !mYourItems;
                mList.reset();
                MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
                update();
                break;
            default:
                // The d-pad is read every frame in onFrame, so it can repeat while held.
                break;
        }
        return true;
    }
}
