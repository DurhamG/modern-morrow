#include "levelupdialog.hpp"

#include <MyGUI_Button.h>
#include <MyGUI_EditBox.h>
#include <MyGUI_Gui.h>
#include <MyGUI_ILayer.h>
#include <MyGUI_ImageBox.h>
#include <MyGUI_LayerManager.h>
#include <MyGUI_ScrollView.h>
#include <MyGUI_TextBox.h>
#include <MyGUI_UString.h>

#include <components/esm3/loadskil.hpp>
#include <components/fallback/fallback.hpp>
#include <components/settings/values.hpp>
#include <components/widgets/box.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"

#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/creaturestats.hpp"
#include "../mwmechanics/npcstats.hpp"

#include "../mwsound/constants.hpp"

#include "class.hpp"
#include "controllerscreen.hpp"

namespace
{
    constexpr unsigned int sMaxCoins = 3;
    constexpr int sColumnOffsets[] = { 32, 218 };
}
namespace MWGui
{
    LevelupDialog::LevelupDialog()
        : WindowBase("openmw_levelup_dialog.layout")
        , mCoinCount(sMaxCoins)
    {
        getWidget(mOkButton, "OkButton");
        getWidget(mClassImage, "ClassImage");
        getWidget(mLevelText, "LevelText");
        getWidget(mLevelDescription, "LevelDescription");
        getWidget(mCoinBox, "Coins");
        getWidget(mAssignWidget, "AssignWidget");

        mOkButton->eventMouseButtonClick += MyGUI::newDelegate(this, &LevelupDialog::onOkButtonClicked);

        {
            const auto& store = MWBase::Environment::get().getESMStore()->get<ESM::Attribute>();
            mPerCol = static_cast<size_t>(std::ceil(store.getSize() / static_cast<float>(std::size(sColumnOffsets))));
            size_t i = 0;
            for (const ESM::Attribute& attribute : store)
            {
                const int offset = sColumnOffsets[i / mPerCol];
                const int row = static_cast<int>(i % mPerCol);
                Widgets widgets;
                widgets.mMultiplier = mAssignWidget->createWidget<MyGUI::TextBox>(
                    "SandTextVCenter", { offset, 20 * row, 100, 20 }, MyGUI::Align::Default);
                auto* hbox = mAssignWidget->createWidget<Gui::HBox>(
                    {}, { offset + 20, 20 * row, 200, 20 }, MyGUI::Align::Default);
                widgets.mButton = hbox->createWidget<Gui::AutoSizedButton>("SandTextButton", {}, MyGUI::Align::Default);
                widgets.mButton->setUserData(attribute.mId);
                widgets.mButton->eventMouseButtonClick += MyGUI::newDelegate(this, &LevelupDialog::onAttributeClicked);
                widgets.mButton->setUserString("TextPadding", "0 0");
                widgets.mButton->setUserString("ToolTipType", "Layout");
                widgets.mButton->setUserString("ToolTipLayout", "AttributeToolTip");
                widgets.mButton->setUserString("Caption_AttributeName", attribute.mName);
                widgets.mButton->setUserString("Caption_AttributeDescription", attribute.mDescription);
                widgets.mButton->setUserString("ImageTexture_AttributeImage", attribute.mIcon.getNormalized());
                widgets.mButton->setCaption(attribute.mName);
                widgets.mValue = hbox->createWidget<Gui::AutoSizedTextBox>("SandText", {}, MyGUI::Align::Default);
                mAttributeWidgets.emplace(attribute.mId, widgets);
                mAttributeButtons.emplace_back(widgets.mButton);
                ++i;
            }

            mAssignWidget->setVisibleVScroll(false);
            mAssignWidget->setCanvasSize(MyGUI::IntSize(
                mAssignWidget->getWidth(), std::max(mAssignWidget->getHeight(), static_cast<int>(20 * mPerCol))));
            mAssignWidget->setVisibleVScroll(true);
            mAssignWidget->setViewOffset(MyGUI::IntPoint());
        }

        for (unsigned int i = 0; i < sMaxCoins; ++i)
        {
            MyGUI::ImageBox* image = mCoinBox->createWidget<MyGUI::ImageBox>(
                "ImageBox", MyGUI::IntCoord(0, 0, 16, 16), MyGUI::Align::Default);
            image->setImageTexture("icons\\tx_goldicon.dds");
            mCoins.push_back(image);
        }

        if (Settings::gui().mControllerMenus)
        {
            mDisableGamepadCursor = true;
            mControllerButtons.mA = "#{Interface:Select}";
            mControllerButtons.mX = "#{Interface:Done}";
            mOkButton->setCaption(
                MyGUI::UString(MWBase::Environment::get().getWindowManager()->getGameSettingString("sDone", {})));

            if (MyGUI::ILayer* layer = MyGUI::LayerManager::getInstance().getByName("ControllerMenus", false))
            {
                mPanel = MyGUI::Gui::getInstance().createWidget<MyGUI::Widget>(
                    "", MyGUI::IntCoord(MyGUI::IntPoint(), layer->getSize()), MyGUI::Align::Default, "ControllerMenus");
                mPanel->setNeedMouseFocus(false);
                mPanel->setVisible(false);
            }
        }

        center();
    }

    LevelupDialog::~LevelupDialog()
    {
        if (mPanel != nullptr)
            MyGUI::Gui::getInstance().destroyWidget(mPanel);
    }

    void LevelupDialog::onClose()
    {
        if (mPanel != nullptr)
            mPanel->setVisible(false);
    }

    void LevelupDialog::drawPanel()
    {
        using namespace ControllerUi;
        mPanel->setSize(mPanel->getLayer()->getSize());
        while (mPanel->getChildCount() > 0)
            MyGUI::Gui::getInstance().destroyWidget(mPanel->getChildAt(0));
        const MyGUI::IntSize size = mPanel->getSize();
        addRect(mPanel, 0, 0, size.width, size.height, MyGUI::Colour::Black, 0.97f);

        const MWWorld::Ptr player = MWMechanics::getPlayer();
        const MWMechanics::NpcStats& pcStats = player.getClass().getNpcStats(player);
        const MWWorld::ESMStore& store = *MWBase::Environment::get().getESMStore();

        // The class image, the new level and its description on the left.
        const int leftW = std::max(320, static_cast<int>(size.width * 0.34f));
        const int x = sMargin;
        int ty = sMargin;
        const int imageH = leftW / 2;
        MyGUI::ImageBox* image = mPanel->createWidget<MyGUI::ImageBox>(
            "ImageBox", MyGUI::IntCoord(x, ty, leftW, imageH), MyGUI::Align::Default);
        setClassImage(image, mClassImageId);
        image->setNeedMouseFocus(false);
        addRect(mPanel, x, ty, leftW, 1, sAccentColour, 0.6f);
        ty += imageH + 18;
        for (const std::string& line :
            wrapText(mPanel, localised(mLevelText->getCaption().asUTF8()), leftW, sFontTitle, 2))
        {
            addText(mPanel, x, ty, leftW, 34, escaped(line), sFontTitle, sBrightColour);
            ty += 32;
        }
        ty += 8;
        for (const std::string& line :
            wrapText(mPanel, mLevelDescription->getCaption().asUTF8(), leftW, sFontNormal, 12))
        {
            addText(mPanel, x, ty, leftW, sFontNormal + 6, escaped(line), sFontNormal, sTextColour);
            ty += sFontNormal + 6;
        }

        // The attributes as tiles on the right, in the dialog's two columns.
        const int rx = x + leftW + 40;
        const int rw = size.width - sMargin - rx;
        const int left = static_cast<int>(mCoinCount - std::min<std::size_t>(mCoinCount, mSpentAttributes.size()));
        addText(mPanel, rx, sMargin, rw, 34, "#{Interface:LevelUpChoose}", sFontLarge, sBrightColour);
        addText(mPanel, rx, sMargin, rw, 34, std::to_string(left) + " #{Interface:LevelUpLeft}", sFontNormal,
            left > 0 ? sNewColour : sBetterColour, MyGUI::Align::Right | MyGUI::Align::VCenter);
        constexpr int gap = 12;
        constexpr int tileH = 74;
        const int tileW = (rw - gap) / 2;
        const int tilesTop = sMargin + 50;
        std::size_t i = 0;
        const ESM::Attribute* focusedAttribute = nullptr;
        for (const ESM::Attribute& attribute : store.get<ESM::Attribute>())
        {
            const int tx = rx + static_cast<int>(i / mPerCol) * (tileW + gap);
            const int tileY = tilesTop + static_cast<int>(i % mPerCol) * (tileH + gap);
            const bool focused = i == mControllerFocus;
            const bool chosen
                = std::find(mSpentAttributes.begin(), mSpentAttributes.end(), attribute.mId) != mSpentAttributes.end();
            const int base = static_cast<int>(pcStats.getAttribute(attribute.mId).getBase());
            const bool maxed = base >= 100;
            const int bonus = std::min(pcStats.getLevelupAttributeMultiplier(attribute.mId), 100 - base);
            if (focused)
                focusedAttribute = &attribute;

            addRect(mPanel, tx, tileY, tileW, tileH, sPanelColour, 0.9f);
            if (chosen)
                addRect(mPanel, tx, tileY, tileW, tileH, sBetterColour, 0.12f);
            if (focused)
            {
                addRect(mPanel, tx, tileY, tileW, tileH, sAccentColour, 0.16f);
                addRect(mPanel, tx, tileY, 3, tileH, sAccentColour);
            }
            addImage(mPanel, tx + 12, tileY + 12, tileH - 24, std::string(attribute.mIcon.getNormalized().value()));
            const int textX = tx + 12 + tileH - 24 + 12;
            addText(mPanel, textX, tileY + 8, tileW - (textX - tx) - 110, 30, escaped(attribute.mName), sFontLarge,
                maxed ? sDimColour : (focused ? sActiveColour : sBrightColour));
            std::string bonusText;
            if (maxed)
                bonusText = "#{Interface:LevelUpMaxed}";
            else
                bonusText = "+" + std::to_string(std::max(1, bonus));
            addText(mPanel, textX, tileY + 40, tileW - (textX - tx) - 110, 24, bonusText, sFontNormal,
                maxed ? sDimColour : (bonus > 1 ? sBetterColour : sTextColour));
            // The value now, and after the level up when the attribute is chosen.
            const std::string value = chosen
                ? std::to_string(base) + " -> " + std::to_string(std::min(100, base + std::max(1, bonus)))
                : std::to_string(base);
            addText(mPanel, tx + tileW - 120, tileY, 108, tileH, value, sFontLarge,
                chosen ? sBetterColour : sBrightColour, MyGUI::Align::Right | MyGUI::Align::VCenter);
            // Pointing at a tile highlights it, and clicking it picks it, as A does.
            addPointerTarget(
                mPanel, tx, tileY, tileW, tileH,
                [this, i] {
                    if (mControllerFocus != i)
                        focusWithPointer(i);
                },
                [this, i] {
                    focusWithPointer(i);
                    pressButton(SDL_CONTROLLER_BUTTON_A);
                });
            ++i;
        }

        // The highlighted attribute's description, and the skills that raised its bonus.
        int dy = tilesTop + static_cast<int>(mPerCol) * (tileH + gap) + 6;
        const int bottom = size.height - sMargin - 40;
        if (focusedAttribute != nullptr)
        {
            for (const std::string& line : wrapText(mPanel, focusedAttribute->mDescription, rw, sFontNormal, 4))
            {
                if (dy + sFontNormal + 6 > bottom)
                    break;
                addText(mPanel, rx, dy, rw, sFontNormal + 6, escaped(line), sFontNormal, sTextColour);
                dy += sFontNormal + 6;
            }
            std::string skills;
            for (const ESM::Skill& skill : store.get<ESM::Skill>())
                if (skill.mData.mAttribute == focusedAttribute->mId)
                    skills += (skills.empty() ? "" : ", ") + skill.mName;
            if (!skills.empty() && dy + 26 <= bottom)
                addText(mPanel, rx, dy + 6, rw, 24, "#{Interface:LevelUpSkills} " + escaped(skills), sFontSmall,
                    sDimColour);
        }

        // The button hints along the bottom.
        int hx = rx;
        const int hintY = size.height - sMargin - 30;
        const auto hint = [&](SDL_GameControllerButton button, const std::string& label, const MyGUI::Colour& colour) {
            addGlyph(mPanel, hx, hintY + 4, 22, button);
            MyGUI::TextBox* text = addText(mPanel, hx + 30, hintY, 220, 30, label, sFontNormal, colour);
            // Clicking a hint presses its button.
            addPointerTarget(
                mPanel, hx, hintY, 30 + text->getTextSize().width, 30, nullptr, [button] { pressButton(button); });
            hx += 30 + text->getTextSize().width + 22;
        };
        hint(SDL_CONTROLLER_BUTTON_A, "#{Interface:LevelUpPick}", sBrightColour);
        hint(SDL_CONTROLLER_BUTTON_X, "#{sDone}", left == 0 ? sBrightColour : sDimColour);
    }

    void LevelupDialog::focusWithPointer(std::size_t index)
    {
        if (index >= mAttributeButtons.size())
            return;
        setControllerFocus(mAttributeButtons, mControllerFocus, false);
        mControllerFocus = index;
        setControllerFocus(mAttributeButtons, mControllerFocus, true);
        if (mPanel != nullptr && mPanel->getVisible())
            drawPanel();
    }

    void LevelupDialog::setAttributeValues()
    {
        MWWorld::Ptr player = MWBase::Environment::get().getWorld()->getPlayerPtr();
        MWMechanics::CreatureStats& creatureStats = player.getClass().getCreatureStats(player);
        MWMechanics::NpcStats& pcStats = player.getClass().getNpcStats(player);

        for (const ESM::Attribute& attribute : MWBase::Environment::get().getESMStore()->get<ESM::Attribute>())
        {
            int val = static_cast<int>(creatureStats.getAttribute(attribute.mId).getBase());
            if (std::find(mSpentAttributes.begin(), mSpentAttributes.end(), attribute.mId) != mSpentAttributes.end())
            {
                val += pcStats.getLevelupAttributeMultiplier(attribute.mId);
            }

            if (val >= 100)
                val = 100;

            mAttributeWidgets[attribute.mId].mValue->setCaption(MyGUI::utility::toString(val));
        }
    }

    void LevelupDialog::resetCoins()
    {
        constexpr int coinSpacing = 33;
        int curX = mCoinBox->getWidth() / 2 - (coinSpacing * (mCoinCount - 1) + 16 * mCoinCount) / 2;
        for (unsigned int i = 0; i < sMaxCoins; ++i)
        {
            MyGUI::ImageBox* image = mCoins[i];
            image->detachFromWidget();
            image->attachToWidget(mCoinBox);
            if (i < mCoinCount)
            {
                mCoins[i]->setVisible(true);
                image->setCoord(MyGUI::IntCoord(curX, 0, 16, 16));
                curX += 16 + coinSpacing;
            }
            else
                mCoins[i]->setVisible(false);
        }
    }

    void LevelupDialog::assignCoins()
    {
        resetCoins();
        for (size_t i = 0; i < mSpentAttributes.size(); ++i)
        {
            MyGUI::ImageBox* image = mCoins[i];
            image->detachFromWidget();
            image->attachToWidget(mAssignWidget);

            const auto& attribute = mSpentAttributes[i];
            const auto& widgets = mAttributeWidgets[attribute];

            const int xdiff = widgets.mMultiplier->getCaption().empty() ? 0 : 20;
            const auto* hbox = widgets.mButton->getParent();

            MyGUI::IntPoint pos = hbox->getPosition();
            pos.left -= 22 + xdiff;
            pos.top += (hbox->getHeight() - image->getHeight()) / 2;
            image->setPosition(pos);
        }

        setAttributeValues();
    }

    void LevelupDialog::onOpen()
    {
        MWBase::World* world = MWBase::Environment::get().getWorld();
        MWWorld::Ptr player = world->getPlayerPtr();
        const MWMechanics::CreatureStats& creatureStats = player.getClass().getCreatureStats(player);
        const MWMechanics::NpcStats& pcStats = player.getClass().getNpcStats(player);

        mClassImageId = ESM::RefId::stringRefId(
            getLevelupClassImage(pcStats.getSkillIncreasesForSpecialization(ESM::Class::Specialization::Combat),
                pcStats.getSkillIncreasesForSpecialization(ESM::Class::Specialization::Magic),
                pcStats.getSkillIncreasesForSpecialization(ESM::Class::Specialization::Stealth)));
        setClassImage(mClassImage, mClassImageId);

        int level = creatureStats.getLevel() + 1;
        mLevelText->setCaptionWithReplacing("#{sLevelUpMenu1} " + MyGUI::utility::toString(level));

        std::string_view levelupdescription;
        levelupdescription = Fallback::Map::getString("Level_Up_Level" + MyGUI::utility::toString(level));

        if (levelupdescription.empty())
            levelupdescription = Fallback::Map::getString("Level_Up_Default");

        mLevelDescription->setCaption(MyGUI::UString(levelupdescription));

        unsigned int availableAttributes = 0;
        for (const ESM::Attribute& attribute : MWBase::Environment::get().getESMStore()->get<ESM::Attribute>())
        {
            const auto& widgets = mAttributeWidgets[attribute.mId];
            if (pcStats.getAttribute(attribute.mId).getBase() < 100)
            {
                widgets.mButton->setEnabled(true);
                widgets.mValue->setEnabled(true);
                availableAttributes++;

                int mult = pcStats.getLevelupAttributeMultiplier(attribute.mId);
                mult = std::min(mult, static_cast<int>(100 - pcStats.getAttribute(attribute.mId).getBase()));
                if (mult <= 1)
                    widgets.mMultiplier->setCaption({});
                else
                    widgets.mMultiplier->setCaption("x" + MyGUI::utility::toString(mult));
            }
            else
            {
                widgets.mButton->setEnabled(false);
                widgets.mValue->setEnabled(false);

                widgets.mMultiplier->setCaption({});
            }
        }

        mCoinCount = std::min(sMaxCoins, availableAttributes);

        mSpentAttributes.clear();
        resetCoins();

        setAttributeValues();

        center();

        if (Settings::gui().mControllerMenus)
        {
            mControllerFocus = 0;
            for (size_t i = 0; i < mAttributeButtons.size(); i++)
                mAttributeButtons[i]->setStateSelected(i == 0);
        }

        if (mPanel != nullptr)
        {
            ControllerUi::hideWindow(mMainWidget);
            mPanel->setVisible(true);
            drawPanel();
        }

        // Play LevelUp Music
        MWBase::Environment::get().getSoundManager()->streamMusic(MWSound::triumphMusic, MWSound::MusicType::Normal);
    }

    void LevelupDialog::onOkButtonClicked(MyGUI::Widget* /*sender*/)
    {
        MWWorld::Ptr player = MWMechanics::getPlayer();
        MWMechanics::NpcStats& pcStats = player.getClass().getNpcStats(player);

        if (mSpentAttributes.size() < mCoinCount)
            MWBase::Environment::get().getWindowManager()->messageBox("#{sNotifyMessage36}");
        else
        {
            // increase attributes
            for (unsigned int i = 0; i < mCoinCount; ++i)
            {
                MWMechanics::AttributeValue attribute = pcStats.getAttribute(mSpentAttributes[i]);
                attribute.setBase(attribute.getBase() + pcStats.getLevelupAttributeMultiplier(mSpentAttributes[i]));

                if (attribute.getBase() >= 100)
                    attribute.setBase(100);
                pcStats.setAttribute(mSpentAttributes[i], attribute);
            }

            pcStats.levelUp();

            MWBase::Environment::get().getWindowManager()->removeGuiMode(GM_Levelup);
        }
    }

    void LevelupDialog::onAttributeClicked(MyGUI::Widget* sender)
    {
        auto attribute = *sender->getUserData<ESM::RefId>();

        auto found = std::find(mSpentAttributes.begin(), mSpentAttributes.end(), attribute);
        if (found != mSpentAttributes.end())
            mSpentAttributes.erase(found);
        else
        {
            if (mSpentAttributes.size() == mCoinCount)
                mSpentAttributes[mCoinCount - 1] = attribute;
            else
                mSpentAttributes.push_back(attribute);
        }
        assignCoins();
    }

    std::string_view LevelupDialog::getLevelupClassImage(
        const int combatIncreases, const int magicIncreases, const int stealthIncreases)
    {
        std::string_view ret = "acrobat";

        int total = combatIncreases + magicIncreases + stealthIncreases;
        if (total == 0)
            return ret;

        int combatFraction = static_cast<int>(static_cast<float>(combatIncreases) / total * 10.f);
        int magicFraction = static_cast<int>(static_cast<float>(magicIncreases) / total * 10.f);
        int stealthFraction = static_cast<int>(static_cast<float>(stealthIncreases) / total * 10.f);

        if (combatFraction > 7)
            ret = "warrior";
        else if (magicFraction > 7)
            ret = "mage";
        else if (stealthFraction > 7)
            ret = "thief";

        switch (combatFraction)
        {
            case 7:
                ret = "warrior";
                break;
            case 6:
                if (stealthFraction == 1)
                    ret = "barbarian";
                else if (stealthFraction == 3)
                    ret = "crusader";
                else
                    ret = "knight";
                break;
            case 5:
                if (stealthFraction == 3)
                    ret = "scout";
                else
                    ret = "archer";
                break;
            case 4:
                ret = "rogue";
                break;
            default:
                break;
        }

        switch (magicFraction)
        {
            case 7:
                ret = "mage";
                break;
            case 6:
                if (combatFraction == 2)
                    ret = "sorcerer";
                else if (combatIncreases == 3)
                    ret = "healer";
                else
                    ret = "battlemage";
                break;
            case 5:
                ret = "witchhunter";
                break;
            case 4:
                ret = "spellsword";
                // In vanilla there's also code for "nightblade", however it seems to be unreachable.
                break;
            default:
                break;
        }

        switch (stealthFraction)
        {
            case 7:
                ret = "thief";
                break;
            case 6:
                if (magicFraction == 1)
                    ret = "agent";
                else if (magicIncreases == 3)
                    ret = "assassin";
                else
                    ret = "acrobat";
                break;
            case 5:
                if (magicIncreases == 3)
                    ret = "monk";
                else
                    ret = "pilgrim";
                break;
            case 3:
                if (magicFraction == 3)
                    ret = "bard";
                break;
            default:
                break;
        }

        return ret;
    }

    bool LevelupDialog::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
    {
        if (arg.button == SDL_CONTROLLER_BUTTON_A)
        {
            if (mControllerFocus < mAttributeButtons.size())
                onAttributeClicked(mAttributeButtons[mControllerFocus]);
            MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Item Gold Up"));
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_X)
        {
            onOkButtonClicked(mOkButton);
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_DPAD_UP)
        {
            setControllerFocus(mAttributeButtons, mControllerFocus, false);
            if (mControllerFocus % mPerCol == 0)
                mControllerFocus += mPerCol - 1;
            else
                mControllerFocus--;
            setControllerFocus(mAttributeButtons, mControllerFocus, true);
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN)
        {
            setControllerFocus(mAttributeButtons, mControllerFocus, false);
            if (mControllerFocus % mPerCol == mPerCol - 1)
                mControllerFocus -= mPerCol - 1;
            else
                mControllerFocus++;
            setControllerFocus(mAttributeButtons, mControllerFocus, true);
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT || arg.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT)
        {
            setControllerFocus(mAttributeButtons, mControllerFocus, false);
            mControllerFocus = (mControllerFocus + mPerCol) % mAttributeButtons.size();
            setControllerFocus(mAttributeButtons, mControllerFocus, true);
        }

        if (mPanel != nullptr && mPanel->getVisible())
            drawPanel();

        return true;
    }
}
