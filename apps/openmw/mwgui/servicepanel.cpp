#include "servicepanel.hpp"

#include <algorithm>

#include <MyGUI_Gui.h>
#include <MyGUI_ILayer.h>
#include <MyGUI_LayerManager.h>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwmechanics/actorutil.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/containerstore.hpp"

namespace MWGui
{
    namespace
    {
        using namespace ControllerUi;

        // The panel's place on the canvas, which is 720 units tall, the same as the barter panel's.
        constexpr float sPanelLeft = 0.40f;
        constexpr int sPanelTop = 40;
        constexpr int sPanelBottom = 680;
        constexpr int sPanelMargin = 40;
        constexpr int sEntryHeight = 52;

        const MyGUI::Colour sPanelBackColour = MyGUI::Colour::parse("#0d0b08");

        int playerGold()
        {
            const MWWorld::Ptr player = MWMechanics::getPlayer();
            return player.getClass().getContainerStore(player).count(MWWorld::ContainerStore::sGoldId);
        }
    }

    ServicePanel::ServicePanel(Service service)
        : mService(std::move(service))
    {
    }

    ServicePanel::~ServicePanel()
    {
        if (mRoot != nullptr)
            MyGUI::Gui::getInstance().destroyWidget(mRoot);
    }

    void ServicePanel::show()
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
        mFocus = 0;
        mScroll = 0;
        mHeldDirection.reset();
        update();
    }

    void ServicePanel::hide()
    {
        if (mRoot != nullptr)
            mRoot->setVisible(false);
        mEntries.clear();
    }

    void ServicePanel::update()
    {
        if (mRoot == nullptr || !mRoot->getVisible())
            return;
        // The window opens before it knows the merchant, and the panel draws again once it does.
        if (mService.mMerchant().isEmpty())
            return;
        mEntries = mService.mEntries();
        draw();
    }

    // ---------------------------------------------------------------------------------------------------------------
    // Drawing

    void ServicePanel::draw()
    {
        while (mRoot->getChildCount() > 0)
            MyGUI::Gui::getInstance().destroyWidget(mRoot->getChildAt(0));

        const MyGUI::IntSize size = mRoot->getSize();
        const int x = static_cast<int>(size.width * sPanelLeft);
        const int w = size.width - sPanelMargin - x;
        addRect(mRoot, x, sPanelTop, w, sPanelBottom - sPanelTop, sPanelBackColour);
        addRect(mRoot, x, sPanelTop, w, 1, sAccentColour, 0.6f);

        // The merchant's name, the service, and the player's gold.
        const MWWorld::Ptr merchant = mService.mMerchant();
        addText(mRoot, x + 20, sPanelTop + 10, w - 40, 34, escaped(merchant.getClass().getName(merchant)), sFontTitle,
            sBrightColour);
        addText(mRoot, x + 20, sPanelTop + 14, w - 40, 30,
            "#{Interface:BarterYourGold}  " + std::to_string(playerGold()), sFontSmall, sDimColour,
            MyGUI::Align::Right | MyGUI::Align::VCenter);
        addText(mRoot, x + 20, sPanelTop + 44, w - 40, 26, mService.mTitle, sFontNormal, sTextColour);
        const int contentTop = sPanelTop + 82;
        addRect(mRoot, x + 20, contentTop - 6, w - 40, 1, sAccentColour, 0.2f);

        const int listW = static_cast<int>(w * 0.54f);
        drawList(x + 8, contentTop, listW - 8, sPanelBottom - 8 - contentTop);
        addRect(mRoot, x + listW + 4, contentTop, 1, sPanelBottom - 16 - contentTop, sAccentColour, 0.15f);
        drawDetails(x + listW + 8, contentTop - 10, w - listW - 8, sPanelBottom - contentTop + 10);
    }

    void ServicePanel::drawList(int x, int y, int w, int h)
    {
        if (mEntries.empty())
        {
            addText(mRoot, x + 12, y + 4, w - 24, 30, mService.mEmpty, sFontNormal, sDimColour);
            return;
        }
        const int count = static_cast<int>(mEntries.size());
        mFocus = std::clamp(mFocus, 0, count - 1);

        // Scroll so the focused entry is in view.
        const int visible = std::max(1, h / sEntryHeight);
        mScroll = std::clamp(mScroll, std::max(0, mFocus - visible + 1), mFocus);
        mScroll = std::min(mScroll, std::max(0, count - visible));

        for (int i = mScroll; i < count && i < mScroll + visible; ++i)
        {
            const Entry& entry = mEntries[i];
            const int rowY = y + (i - mScroll) * sEntryHeight;
            const int rh = sEntryHeight - 2;
            const bool focused = i == mFocus;
            if (focused)
            {
                addRect(mRoot, x, rowY, w, rh, sAccentColour, 0.16f);
                addRect(mRoot, x, rowY, 3, rh, sAccentColour);
            }
            int textX = x + 14;
            if (!entry.mItem.isEmpty())
            {
                addIcon(mRoot, x + 8, rowY + 6, rh - 12, entry.mItem, false);
                textX = x + 8 + rh - 12 + 10;
            }
            else if (!entry.mIcon.empty())
            {
                addImage(mRoot, x + 8, rowY + 6, rh - 12, entry.mIcon);
                textX = x + 8 + rh - 12 + 10;
            }
            constexpr int priceW = 76;
            const bool available = entry.mUnavailable.empty();
            addText(mRoot, textX, rowY + 3, x + w - textX - priceW - 4, 26, escaped(entry.mName), sFontNormal,
                focused ? sActiveColour : (available ? sBrightColour : sDimColour));
            addText(mRoot, textX, rowY + 27, x + w - textX - priceW - 4, 20, entry.mSubtitle, sFontSmall, sDimColour);
            addText(mRoot, x + w - priceW - 8, rowY + 3, priceW, 26,
                std::to_string(entry.mPrice) + " #{Interface:BarterGoldShort}", sFontNormal,
                available ? sTextColour : sWorseColour, MyGUI::Align::Right | MyGUI::Align::VCenter);
        }

        // A thin bar along the right edge shows where the list is scrolled to, when it doesn't fit.
        if (count > visible)
        {
            const int trackH = visible * sEntryHeight - 2;
            addRect(mRoot, x + w + 1, y, 2, trackH, sAccentColour, 0.12f);
            const int thumbH = std::max(16, trackH * visible / count);
            addRect(
                mRoot, x + w + 1, y + (trackH - thumbH) * mScroll / (count - visible), 2, thumbH, sAccentColour, 0.6f);
        }
    }

    void ServicePanel::drawDetails(int x, int y, int w, int h)
    {
        // The button hints along the bottom.
        const int hintY = y + h - 42;
        int hx = x + 16;
        const auto hint = [&](SDL_GameControllerButton button, const std::string& label) {
            addGlyph(mRoot, hx, hintY + 1, 22, button);
            MyGUI::TextBox* text = addText(mRoot, hx + 30, hintY - 3, 200, 30, label, sFontNormal, sBrightColour);
            hx += 30 + text->getTextSize().width + 22;
        };
        const Entry* entry = mEntries.empty() ? nullptr : &mEntries[mFocus];
        if (entry != nullptr && entry->mUnavailable.empty())
            hint(SDL_CONTROLLER_BUTTON_A, mService.mAction);
        if (entry == nullptr)
            return;

        const int innerX = x + 16;
        const int innerW = w - 32;
        int ty = y + 14;
        int titleX = innerX;
        if (!entry->mItem.isEmpty())
        {
            addIcon(mRoot, innerX, ty + 2, 56, entry->mItem, false);
            titleX = innerX + 68;
        }
        else if (!entry->mIcon.empty())
        {
            addImage(mRoot, innerX, ty + 2, 56, entry->mIcon);
            titleX = innerX + 68;
        }
        for (const std::string& line : wrapText(mRoot, entry->mName, x + w - 16 - titleX, sFontLarge, 2))
        {
            addText(mRoot, titleX, ty, x + w - 16 - titleX, 30, escaped(line), sFontLarge, sBrightColour);
            ty += 28;
        }
        addText(mRoot, titleX, ty, x + w - 16 - titleX, 24, entry->mSubtitle, sFontSmall, sDimColour);
        ty = std::max(ty + 34, y + 84);

        // The price, then the entry's own facts and paragraphs.
        const auto fact = [&](const std::string& label, const std::string& value, const MyGUI::Colour& colour) {
            addText(mRoot, innerX, ty, innerW, 28, label, sFontNormal, sTextColour);
            addText(
                mRoot, innerX, ty, innerW, 28, value, sFontNormal, colour, MyGUI::Align::Right | MyGUI::Align::VCenter);
            ty += 30;
        };
        fact("#{Interface:BarterBuyPrice}", std::to_string(entry->mPrice),
            entry->mPrice <= playerGold() ? sBrightColour : sWorseColour);
        for (const auto& [label, value] : entry->mFacts)
            fact(label, value, sBrightColour);
        ty += 6;
        const int bottom = hintY - 40;
        for (const std::string& paragraph : entry->mParagraphs)
        {
            for (const std::string& line : wrapText(mRoot, localised(paragraph), innerW, sFontNormal, 12))
            {
                if (ty + sFontNormal + 6 > bottom)
                    break;
                addText(mRoot, innerX, ty, innerW, sFontNormal + 6, escaped(line), sFontNormal, sTextColour);
                ty += sFontNormal + 6;
            }
            ty += 8;
        }

        // Why the entry can't be picked, just above the hints.
        if (!entry->mUnavailable.empty())
            addText(mRoot, innerX, hintY - 36, innerW, 28, entry->mUnavailable, sFontNormal, sWorseColour);
    }

    // ---------------------------------------------------------------------------------------------------------------
    // Input

    void ServicePanel::onFrame(float duration)
    {
        if (mRoot == nullptr || !mRoot->getVisible())
            return;
        int dx = 0;
        int dy = 0;
        if (mHeldDirection.update(duration, dx, dy) && dy != 0 && !mEntries.empty())
        {
            mFocus = std::clamp(mFocus + dy, 0, static_cast<int>(mEntries.size()) - 1);
            draw();
        }
    }

    bool ServicePanel::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
    {
        switch (arg.button)
        {
            case SDL_CONTROLLER_BUTTON_A:
                if (!mEntries.empty() && mEntries[mFocus].mUnavailable.empty())
                {
                    mService.mChoose(mEntries[mFocus].mButton);
                    update();
                }
                break;
            case SDL_CONTROLLER_BUTTON_B:
                mService.mBack();
                break;
            default:
                // The d-pad is read every frame in onFrame, so it can repeat while held.
                break;
        }
        return true;
    }
}
