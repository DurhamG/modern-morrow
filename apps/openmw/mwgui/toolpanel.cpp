#include "toolpanel.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include <MyGUI_Gui.h>
#include <MyGUI_ImageBox.h>

#include <components/esm3/loadcrea.hpp>
#include <components/esm3/loadench.hpp>
#include <components/esm3/loadrepa.hpp>
#include <components/esm3/loadskil.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/spellutil.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"

#include "inventoryitemmodel.hpp"
#include "itemwidget.hpp"
#include "recharge.hpp"
#include "repair.hpp"
#include "sortfilteritemmodel.hpp"

namespace MWGui
{
    namespace
    {
        using namespace ControllerUi;

        constexpr int sRowHeight = 58;

        const MWWorld::ESMStore& store()
        {
            return *MWBase::Environment::get().getESMStore();
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

        void drawBar(MyGUI::Widget* parent, int x, int y, int w, int h, int value, int most)
        {
            const float fraction = most > 0 ? std::clamp(static_cast<float>(value) / most, 0.f, 1.f) : 0.f;
            addRect(parent, x, y, w, h, MyGUI::Colour::Black, 0.6f);
            addRect(parent, x, y, std::max(2, static_cast<int>(w * fraction)), h, sAccentColour, 0.8f);
        }
    }

    ToolPanel::ToolPanel(Repair& window)
        : mRepair(&window)
    {
    }

    ToolPanel::ToolPanel(Recharge& window)
        : mRecharge(&window)
    {
    }

    ToolPanel::~ToolPanel()
    {
        if (mRoot != nullptr)
            MyGUI::Gui::getInstance().destroyWidget(mRoot);
    }

    void ToolPanel::show()
    {
        if (mRoot == nullptr)
            mRoot = createPanelRoot("ControllerMenus");
        if (mRoot == nullptr)
            return;
        mRoot->setVisible(true);
        mList.reset();
        mList.resetHeld();
        update();
    }

    void ToolPanel::hide()
    {
        if (mRoot != nullptr)
            mRoot->setVisible(false);
        mItems.clear();
    }

    MWWorld::Ptr ToolPanel::tool() const
    {
        if (mRepair != nullptr)
            return mRepair->mRepair.getTool();
        const MWWorld::Ptr* gem = mRecharge->mGemIcon->getUserData<MWWorld::Ptr>(false);
        return gem != nullptr ? *gem : MWWorld::Ptr();
    }

    std::vector<MWWorld::Ptr> ToolPanel::tools() const
    {
        return inventoryItems(mRepair != nullptr ? SortFilterItemModel::Filter_OnlyRepairTools
                                                 : SortFilterItemModel::Filter_OnlyChargedSoulstones);
    }

    void ToolPanel::setTool(const MWWorld::Ptr& item)
    {
        // The same steps as the window takes when the player picks a tool.
        ItemWidget* icon = mRepair != nullptr ? mRepair->mToolIcon : mRecharge->mGemIcon;
        icon->setItem(item);
        icon->setUserString("ToolTipType", "ItemPtr");
        icon->setUserData(MWWorld::Ptr(item));
        if (mRepair != nullptr)
        {
            mRepair->mRepair.setTool(item);
            mRepair->updateRepairView();
        }
        else
            mRecharge->updateView();
    }

    std::pair<int, int> ToolPanel::itemValues(const MWWorld::Ptr& item) const
    {
        if (mRepair != nullptr)
            return { item.getClass().getItemHealth(item), item.getClass().getItemMaxHealth(item) };
        const ESM::Enchantment* enchantment
            = store().get<ESM::Enchantment>().search(item.getClass().getEnchantment(item));
        const int most = enchantment != nullptr ? MWMechanics::getEnchantmentCharge(*enchantment) : 0;
        const float charge = item.getCellRef().getEnchantmentCharge();
        // A charge of -1 means the item has never been used, so it is full.
        return { charge < 0 ? most : static_cast<int>(charge), most };
    }

    void ToolPanel::update()
    {
        if (mRoot == nullptr || !mRoot->getVisible())
            return;

        // When the tool runs out, the next one of its kind takes its place.
        const MWWorld::Ptr current = tool();
        if (current.isEmpty() || current.getCellRef().getCount() == 0)
        {
            const std::vector<MWWorld::Ptr> available = tools();
            if (!available.empty())
                setTool(available.front());
        }

        mItems = inventoryItems(mRepair != nullptr ? SortFilterItemModel::Filter_OnlyRepairable
                                                   : SortFilterItemModel::Filter_OnlyRechargable);
        // The ones in the worst shape come first.
        std::stable_sort(mItems.begin(), mItems.end(), [this](const MWWorld::Ptr& a, const MWWorld::Ptr& b) {
            const auto [valueA, mostA] = itemValues(a);
            const auto [valueB, mostB] = itemValues(b);
            return static_cast<float>(valueA) / std::max(1, mostA) < static_cast<float>(valueB) / std::max(1, mostB);
        });
        mList.setFocus(std::clamp(mList.getFocus(), 0, std::max(0, static_cast<int>(mItems.size()) - 1)));

        clearPanel(mRoot);
        const MyGUI::IntSize size = mRoot->getSize();
        addRect(mRoot, 0, 0, size.width, size.height, MyGUI::Colour::Black)->setNeedMouseFocus(true);
        addText(mRoot, sMargin, 20, size.width - 2 * sMargin, 40,
            mRepair != nullptr ? "#{sRepair}" : "#{sRechargeEnchantment}", sFontTitle, sBrightColour);
        const MWWorld::Ptr player = MWMechanics::getPlayer();
        const auto skill = mRepair != nullptr ? ESM::Skill::Armorer : ESM::Skill::Enchant;
        const ESM::Skill* skillRecord = store().get<ESM::Skill>().find(skill);
        addText(mRoot, sMargin, 20, size.width - 2 * sMargin, 40,
            escaped(skillRecord->mName) + "  "
                + std::to_string(static_cast<int>(player.getClass().getSkill(player, skill))),
            sFontSmall, sDimColour, MyGUI::Align::Right | MyGUI::Align::VCenter);
        addRect(mRoot, sMargin, 66, size.width - 2 * sMargin, 1, sAccentColour, 0.2f);

        const int top = 82;
        const int bottom = size.height - sContentBottomMargin;
        const int listW = (size.width - 2 * sMargin - 16) * 55 / 100;
        drawList(sMargin, top, listW, bottom - top);
        const int px = sMargin + listW + 16;
        drawTool(px, top, size.width - sMargin - px, bottom - top);
    }

    void ToolPanel::drawList(int x, int y, int w, int h)
    {
        addRect(mRoot, x, y, w, h, sPanelColour, 0.9f);
        addText(mRoot, x + 16, y + 8, w - 32, 30,
            mRepair != nullptr ? "#{Interface:ToolDamaged}" : "#{Interface:ToolDrained}", sFontLarge, sBrightColour);
        const int listTop = y + 44;
        const int listBottom = y + h - 6;
        if (mItems.empty())
        {
            addText(mRoot, x + 16, listTop, w - 32, 30,
                mRepair != nullptr ? "#{Interface:ToolNothingToRepair}" : "#{Interface:ToolNothingToRecharge}",
                sFontNormal, sDimColour);
            return;
        }

        const std::vector<ControllerList::Row> rows(mItems.size(), ControllerList::Row{ sRowHeight, true });
        const auto drawRow = [&](std::size_t i, int rowY, bool focused) {
            const MWWorld::Ptr& item = mItems[i];
            const int rx = x + 4;
            const int rw = w - 8;
            if (focused)
            {
                addRect(mRoot, rx, rowY, rw, sRowHeight - 2, sAccentColour, 0.16f);
                addRect(mRoot, rx, rowY, 3, sRowHeight - 2, sAccentColour);
            }
            addIcon(mRoot, rx + 10, rowY + 8, 40, item, false);
            const int textX = rx + 62;
            const int textW = rx + rw - 16 - textX;
            const auto [value, most] = itemValues(item);
            addText(mRoot, textX, rowY + 4, textW, 26, escaped(item.getClass().getName(item)), sFontNormal,
                focused ? sActiveColour : sBrightColour);
            addText(mRoot, textX, rowY + 4, textW, 26, std::to_string(value) + " / " + std::to_string(most),
                sFontNormal, sTextColour, MyGUI::Align::Right | MyGUI::Align::VCenter);
            drawBar(mRoot, textX, rowY + 38, textW, 6, value, most);
        };
        // Pointing at an item focuses it, and clicking it uses the tool on it, as A does.
        mList.draw(
            mRoot, x, listTop, w, listBottom - listTop, rows, drawRow, [this] { update(); },
            [] { pressButton(SDL_CONTROLLER_BUTTON_A); });
    }

    void ToolPanel::drawTool(int x, int y, int w, int h)
    {
        addRect(mRoot, x, y, w, h, sPanelColour, 0.9f);
        addRect(mRoot, x, y, w, 1, sAccentColour, 0.3f);
        addText(mRoot, x + 16, y + 8, w - 32, 30, mRepair != nullptr ? "#{Interface:ToolRepairTool}" : "#{sSoulGem}",
            sFontLarge, sBrightColour);

        const MWWorld::Ptr current = tool();
        const bool hasTool = !current.isEmpty() && current.getCellRef().getCount() > 0;
        int ry = y + 52;
        if (!hasTool)
        {
            addText(mRoot, x + 16, ry, w - 32, 30,
                mRepair != nullptr ? "#{Interface:ToolNoRepairTools}" : "#{Interface:ToolNoSoulGems}", sFontNormal,
                sWorseColour);
        }
        else
        {
            addIcon(mRoot, x + 16, ry, 56, current, false);
            std::string name = escaped(current.getClass().getName(current));
            if (current.getCellRef().getCount() > 1)
                name += " (" + std::to_string(current.getCellRef().getCount()) + ")";
            addText(mRoot, x + 84, ry, w - 100, 28, name, sFontNormal, sBrightColour);
            std::vector<std::pair<std::string, std::string>> facts;
            if (mRepair != nullptr)
            {
                std::ostringstream quality;
                quality << std::setprecision(3) << current.get<ESM::Repair>()->mBase->mData.mQuality;
                facts = { { "#{sUses}", std::to_string(current.getClass().getItemHealth(current)) },
                    { "#{sQuality}", quality.str() } };
            }
            else if (const ESM::Creature* soul = store().get<ESM::Creature>().search(current.getCellRef().getSoul()))
                facts = { { "#{Interface:ToolSoul}", escaped(soul->mName) },
                    { "#{Interface:ToolSoulStrength}", std::to_string(soul->mData.mSoul) } };
            int fy = ry + 30;
            for (const auto& [label, value] : facts)
            {
                addText(mRoot, x + 84, fy, w - 100, 24, label, sFontSmall, sDimColour);
                addText(mRoot, x + 84, fy, w - 100, 24, value, sFontSmall, sTextColour,
                    MyGUI::Align::Right | MyGUI::Align::VCenter);
                fy += 22;
            }
        }
        ry += 110;

        // What each use does.
        const std::string help = mRepair != nullptr ? "#{Interface:ToolRepairHelp}" : "#{Interface:ToolRechargeHelp}";
        for (const std::string& line : wrapText(mRoot, localised(help), w - 32, sFontSmall, 4))
        {
            addText(mRoot, x + 16, ry, w - 32, 22, escaped(line), sFontSmall, sDimColour);
            ry += 22;
        }

        // The highlighted item, larger.
        if (!mItems.empty())
        {
            const MWWorld::Ptr& item = mItems[mList.getFocus()];
            const auto [value, most] = itemValues(item);
            ry += 20;
            addRect(mRoot, x + 16, ry, w - 32, 1, sAccentColour, 0.2f);
            ry += 14;
            addIcon(mRoot, x + 16, ry, 56, item, false);
            addText(mRoot, x + 84, ry, w - 100, 30, escaped(item.getClass().getName(item)), sFontLarge, sBrightColour);
            addText(mRoot, x + 84, ry + 30, w - 100, 24,
                (mRepair != nullptr ? "#{sCondition}  " : "#{Interface:MagicCharge}  ") + std::to_string(value) + " / "
                    + std::to_string(most),
                sFontNormal, sTextColour);
            drawBar(mRoot, x + 16, ry + 70, w - 32, 10, value, most);
        }

        std::vector<Hint> hints;
        if (hasTool && !mItems.empty())
            hints.emplace_back(
                SDL_CONTROLLER_BUTTON_A, mRepair != nullptr ? "#{Interface:Repair}" : "#{Interface:ToolRecharge}");
        if (tools().size() > 1)
            hints.emplace_back(SDL_CONTROLLER_BUTTON_Y, "#{Interface:ToolNext}");
        drawHints(mRoot, x + 16, w - 32, y + h, hints);
    }

    void ToolPanel::onFrame(float duration)
    {
        if (mRoot == nullptr || !mRoot->getVisible())
            return;
        const WindowBase* window = mRepair != nullptr ? static_cast<WindowBase*>(mRepair) : mRecharge;
        if (MWBase::Environment::get().getWindowManager()->getActiveControllerWindow() != window)
        {
            mList.resetHeld();
            return;
        }
        if (mList.updateHeld(duration, static_cast<int>(mItems.size())))
            update();
    }

    bool ToolPanel::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
    {
        switch (arg.button)
        {
            case SDL_CONTROLLER_BUTTON_A:
                useTool();
                break;
            case SDL_CONTROLLER_BUTTON_B:
                if (mRepair != nullptr)
                    mRepair->onCancel(nullptr);
                else
                    mRecharge->onCancel(nullptr);
                return true;
            case SDL_CONTROLLER_BUTTON_Y:
                nextTool();
                break;
            default:
                // The d-pad and the left stick are read every frame in onFrame, so they repeat while held.
                return true;
        }
        update();
        return true;
    }

    void ToolPanel::useTool()
    {
        const int focus = mList.getFocus();
        if (mItems.empty() || focus >= static_cast<int>(mItems.size()))
            return;
        const MWWorld::Ptr item = mItems[focus];
        if (mRepair != nullptr)
            mRepair->onRepairItem(nullptr, item);
        else
            mRecharge->onItemClicked(nullptr, item);
    }

    void ToolPanel::nextTool()
    {
        const std::vector<MWWorld::Ptr> available = tools();
        if (available.size() < 2)
            return;
        const auto it = std::find(available.begin(), available.end(), tool());
        const MWWorld::Ptr next = it == available.end() || it + 1 == available.end() ? available.front() : *(it + 1);
        setTool(next);
        MWBase::Environment::get().getWindowManager()->playSound(next.getClass().getDownSoundId(next));
    }
}
