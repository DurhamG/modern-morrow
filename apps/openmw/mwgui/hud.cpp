#include "hud.hpp"

#include <stdexcept>
#include <string>
#include <utility>

#include <MyGUI_Button.h>
#include <MyGUI_ImageBox.h>
#include <MyGUI_InputManager.h>
#include <MyGUI_ProgressBar.h>
#include <MyGUI_RenderManager.h>
#include <MyGUI_ScrollView.h>

#include <components/esm3/loadgmst.hpp>
#include <components/esm3/loadmgef.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/worldmodel.hpp"

#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/npcstats.hpp"

#include "draganddrop.hpp"
#include "inventorywindow.hpp"
#include "itemwidget.hpp"
#include "spellicons.hpp"
#include "worlditemmodel.hpp"

namespace MWGui
{
    namespace
    {
        constexpr std::pair<std::string_view, HUD::Element> sElementIds[] = {
            { "DynamicStats", HUD::Element::DynamicStats },
            { "EnemyHealth", HUD::Element::EnemyHealth },
            { "Weapon", HUD::Element::Weapon },
            { "Spell", HUD::Element::Spell },
            { "WeaponSpellName", HUD::Element::WeaponSpellName },
            { "Sneak", HUD::Element::Sneak },
            { "ActiveEffects", HUD::Element::ActiveEffects },
            { "Minimap", HUD::Element::Minimap },
            { "CellName", HUD::Element::CellName },
            { "DrowningBar", HUD::Element::DrowningBar },
        };

        HUD::Element findElement(std::string_view elementId)
        {
            for (const auto& [id, element] : sElementIds)
            {
                if (id == elementId)
                    return element;
            }
            throw std::logic_error("Invalid HUD element: " + std::string(elementId));
        }
    }

    HUD::HUD(CustomMarkerCollection& customMarkers, DragAndDrop* dragAndDrop, MWRender::LocalMap* localMapRender)
        : WindowBase("openmw_hud.layout")
        , LocalMapBase(customMarkers, localMapRender, Settings::map().mLocalMapHudFogOfWar)
        , mDragAndDrop(dragAndDrop)
    {
        // Energy bars
        getWidget(mHealthFrame, "HealthFrame");
        getWidget(mHealth, "Health");
        getWidget(mMagicka, "Magicka");
        getWidget(mStamina, "Stamina");
        getWidget(mEnemyHealth, "EnemyHealth");
        mHealthManaStaminaBaseLeft = mHealthFrame->getLeft();

        MyGUI::Widget *healthFrame, *magickaFrame, *fatigueFrame;
        getWidget(healthFrame, "HealthFrame");
        getWidget(magickaFrame, "MagickaFrame");
        getWidget(fatigueFrame, "FatigueFrame");
        healthFrame->eventMouseButtonClick += MyGUI::newDelegate(this, &HUD::onHMSClicked);
        magickaFrame->eventMouseButtonClick += MyGUI::newDelegate(this, &HUD::onHMSClicked);
        fatigueFrame->eventMouseButtonClick += MyGUI::newDelegate(this, &HUD::onHMSClicked);

        // Drowning bar
        getWidget(mDrowningBar, "DrowningBar");
        getWidget(mDrowningFrame, "DrowningFrame");
        getWidget(mDrowning, "Drowning");
        getWidget(mDrowningFlash, "Flash");
        mDrowning->setProgressRange(200);

        const MyGUI::IntSize& viewSize = MyGUI::RenderManager::getInstance().getViewSize();

        // Item and spell images and status bars
        getWidget(mWeapBox, "WeapBox");
        getWidget(mWeapImage, "WeapImage");
        getWidget(mWeapStatus, "WeapStatus");
        mWeapBoxBaseLeft = mWeapBox->getLeft();
        mWeapBox->eventMouseButtonClick += MyGUI::newDelegate(this, &HUD::onWeaponClicked);

        getWidget(mSpellBox, "SpellBox");
        getWidget(mSpellImage, "SpellImage");
        getWidget(mSpellStatus, "SpellStatus");
        mSpellBoxBaseLeft = mSpellBox->getLeft();
        mSpellBox->eventMouseButtonClick += MyGUI::newDelegate(this, &HUD::onMagicClicked);

        getWidget(mSneakBox, "SneakBox");
        mSneakBoxBaseLeft = mSneakBox->getLeft();

        getWidget(mEffectBox, "EffectBox");
        mEffectBoxBaseRight = viewSize.width - mEffectBox->getRight();

        getWidget(mMinimapBox, "MiniMapBox");
        mMinimapBoxBaseRight = viewSize.width - mMinimapBox->getRight();
        getWidget(mMinimap, "MiniMap");
        getWidget(mCompass, "Compass");
        getWidget(mMinimapButton, "MiniMapButton");
        mMinimapButton->eventMouseButtonClick += MyGUI::newDelegate(this, &HUD::onMapClicked);

        getWidget(mCellNameBox, "CellName");
        getWidget(mWeaponSpellBox, "WeaponSpellName");

        getWidget(mCrosshair, "Crosshair");
        getWidget(mMouseEmulationCursor, "MouseEmulationCursor");

        mLocalMapZoom = 0.5f;

        LocalMapBase::init(mMinimap, mCompass);

        mMainWidget->eventMouseButtonClick += MyGUI::newDelegate(this, &HUD::onWorldClicked);
        mMainWidget->eventMouseMove += MyGUI::newDelegate(this, &HUD::onWorldMouseOver);
        mMainWidget->eventMouseLostFocus += MyGUI::newDelegate(this, &HUD::onWorldMouseLostFocus);

        mSpellIcons = std::make_unique<SpellIcons>();
    }

    HUD::~HUD()
    {
        mMainWidget->eventMouseLostFocus.clear();
        mMainWidget->eventMouseMove.clear();
        mMainWidget->eventMouseButtonClick.clear();
    }

    void HUD::setValue(std::string_view id, const MWMechanics::DynamicStat<float>& value)
    {
        int current = static_cast<int>(value.getCurrent());
        int modified = static_cast<int>(value.getModified());
        // Fatigue can be negative
        if (id != "FBar")
            current = std::max(0, current);

        MyGUI::Widget* w;
        std::string valStr = MyGUI::utility::toString(current) + " / " + MyGUI::utility::toString(modified);
        if (id == "HBar")
        {
            mHealth->setProgressRange(std::max(0, modified));
            mHealth->setProgressPosition(std::max(0, current));
            getWidget(w, "HealthFrame");
            w->setUserString("Caption_HealthDescription", "#{sHealthDesc}\n" + valStr);
        }
        else if (id == "MBar")
        {
            mMagicka->setProgressRange(std::max(0, modified));
            mMagicka->setProgressPosition(std::max(0, current));
            getWidget(w, "MagickaFrame");
            w->setUserString("Caption_HealthDescription", "#{sMagDesc}\n" + valStr);
        }
        else if (id == "FBar")
        {
            mStamina->setProgressRange(std::max(0, modified));
            mStamina->setProgressPosition(std::max(0, current));
            getWidget(w, "FatigueFrame");
            w->setUserString("Caption_HealthDescription", "#{sFatDesc}\n" + valStr);
        }
    }

    void HUD::setDrowningTimeLeft(float time, float maxTime)
    {
        size_t progress = static_cast<size_t>(time / maxTime * 200);
        mDrowning->setProgressPosition(progress);

        bool isDrowning = (progress == 0);
        if (isDrowning && !mIsDrowning) // Just started drowning
            mDrowningFlashTheta = 0.0f; // Start out on bright red every time.

        mDrowningFlash->setVisible(isDrowning);
        mIsDrowning = isDrowning;
    }

    void HUD::setDrowningBarVisible(bool visible)
    {
        mDrowningBarVisible = visible;
        mDrowningBar->setVisible(visible && !isDisabledByLua(Element::DrowningBar));
    }

    void HUD::dropDraggedItem(float mouseX, float mouseY)
    {
        if (!mDragAndDrop->mIsOnDragAndDrop)
            return;

        MWBase::Environment::get().getWorld()->breakInvisibility(MWMechanics::getPlayer());

        WorldItemModel drop(mouseX, mouseY);
        mDragAndDrop->drop(&drop, nullptr);
    }

    void HUD::onWorldClicked(MyGUI::Widget* /*sender*/)
    {
        if (!MWBase::Environment::get().getWindowManager()->isGuiMode())
            return;

        MWBase::WindowManager* winMgr = MWBase::Environment::get().getWindowManager();
        if (mDragAndDrop->mIsOnDragAndDrop)
        {
            const MyGUI::IntSize viewSize = MyGUI::RenderManager::getInstance().getViewSize();
            const MyGUI::IntPoint cursorPosition = MyGUI::InputManager::getInstance().getMousePosition();
            const float cursorX = cursorPosition.left / static_cast<float>(viewSize.width);
            const float cursorY = cursorPosition.top / static_cast<float>(viewSize.height);

            // drop item into the gameworld
            WorldItemModel worldItemModel(cursorX, cursorY);
            mDragAndDrop->drop(&worldItemModel, nullptr);

            winMgr->changePointer("arrow");
        }
        else
        {
            GuiMode mode = winMgr->getMode();

            if (!winMgr->isConsoleMode() && (mode != GM_Container) && (mode != GM_Inventory))
                return;

            MWWorld::Ptr object = MWBase::Environment::get().getWorld()->getFocusObject();

            if (winMgr->isConsoleMode())
                winMgr->setConsoleSelectedObject(object);
            else // if ((mode == GM_Container) || (mode == GM_Inventory))
            {
                // pick up object
                if (!object.isEmpty())
                    winMgr->getInventoryWindow()->pickUpObject(object);
            }
        }
    }

    void HUD::onWorldMouseOver(MyGUI::Widget* /*sender*/, int x, int y)
    {
        if (mDragAndDrop->mIsOnDragAndDrop)
        {
            mWorldMouseOver = false;

            MyGUI::IntSize viewSize = MyGUI::RenderManager::getInstance().getViewSize();
            MyGUI::IntPoint cursorPosition = MyGUI::InputManager::getInstance().getMousePosition();
            float mouseX = cursorPosition.left / float(viewSize.width);
            float mouseY = cursorPosition.top / float(viewSize.height);

            MWBase::World* world = MWBase::Environment::get().getWorld();

            // if we can't drop the object at the wanted position, show the "drop on ground" cursor.
            bool canDrop = world->canPlaceObject(mouseX, mouseY);

            if (!canDrop)
                MWBase::Environment::get().getWindowManager()->changePointer("drop_ground");
            else
                MWBase::Environment::get().getWindowManager()->changePointer("arrow");
        }
        else
        {
            MWBase::Environment::get().getWindowManager()->changePointer("arrow");
            mWorldMouseOver = true;
        }
    }

    void HUD::onWorldMouseLostFocus(MyGUI::Widget* /*sender*/, MyGUI::Widget* newWidget)
    {
        MWBase::Environment::get().getWindowManager()->changePointer("arrow");
        mWorldMouseOver = false;
    }

    void HUD::onHMSClicked(MyGUI::Widget* /*sender*/)
    {
        MWBase::Environment::get().getWindowManager()->toggleVisible(GW_Stats);
    }

    void HUD::onMapClicked(MyGUI::Widget* /*sender*/)
    {
        MWBase::Environment::get().getWindowManager()->toggleVisible(GW_Map);
    }

    void HUD::onWeaponClicked(MyGUI::Widget* /*sender*/)
    {
        const MWWorld::Ptr& player = MWMechanics::getPlayer();
        if (player.getClass().getNpcStats(player).isWerewolf())
        {
            MWBase::Environment::get().getWindowManager()->messageBox("#{sWerewolfRefusal}");
            return;
        }

        MWBase::Environment::get().getWindowManager()->toggleVisible(GW_Inventory);
    }

    void HUD::onMagicClicked(MyGUI::Widget* /*sender*/)
    {
        const MWWorld::Ptr& player = MWMechanics::getPlayer();
        if (player.getClass().getNpcStats(player).isWerewolf())
        {
            MWBase::Environment::get().getWindowManager()->messageBox("#{sWerewolfRefusal}");
            return;
        }

        MWBase::Environment::get().getWindowManager()->toggleVisible(GW_Magic);
    }

    void HUD::setCellName(const std::string& cellName)
    {
        if (mCellName != cellName)
        {
            mCellNameTimer = 5.0f;
            mCellName = cellName;

            mCellNameBox->setCaptionWithReplacing("#{sCell=" + mCellName + "}");
            mCellNameBox->setVisible(mMapVisible && !isDisabledByLua(Element::CellName));
        }
    }

    void HUD::onFrame(float dt)
    {
        LocalMapBase::onFrame(dt);

        mCellNameTimer -= dt;
        mWeaponSpellTimer -= dt;
        if (mCellNameTimer < 0)
            mCellNameBox->setVisible(false);
        if (mWeaponSpellTimer < 0)
            mWeaponSpellBox->setVisible(false);

        mEnemyHealthTimer -= dt;
        if (mEnemyHealthTimer < 0)
            setEnemyHealthVisible(false);

        mSpellIcons->updateWidgets(mEffectBox, true);

        if (mEnemyActor.isSet() && mEnemyHealth->getVisible())
        {
            updateEnemyHealthBar();
        }

        if (mDrowningBar->getVisible())
            mDrowningBar->setPosition(
                mMainWidget->getWidth() / 2 - mDrowningFrame->getWidth() / 2, mMainWidget->getTop());

        if (mIsDrowning)
        {
            mDrowningFlashTheta += dt * osg::PIf * 2;

            float intensity = (cos(mDrowningFlashTheta) + 2.0f) / 3.0f;

            mDrowningFlash->setAlpha(intensity);
        }
    }

    void HUD::setSelectedSpell(const ESM::RefId& spellId, int successChancePercent)
    {
        const ESM::Spell* spell = MWBase::Environment::get().getESMStore()->get<ESM::Spell>().find(spellId);

        const std::string& spellName = spell->mName;
        if (spellName != mSpellName && mSpellVisible)
        {
            mWeaponSpellTimer = 5.0f;
            mSpellName = spellName;
            mWeaponSpellBox->setCaption(mSpellName);
            mWeaponSpellBox->setVisible(!isDisabledByLua(Element::WeaponSpellName));
        }

        mSpellStatus->setProgressRange(100);
        mSpellStatus->setProgressPosition(successChancePercent);

        mSpellBox->setUserString("ToolTipType", "Spell");
        mSpellBox->setUserString("Spell", spellId.serialize());
        mSpellBox->setUserData(MyGUI::Any::Null);

        if (!spell->mEffects.mList.empty())
        {
            // use the icon of the first effect
            const ESM::MagicEffect* effect = MWBase::Environment::get().getESMStore()->get<ESM::MagicEffect>().find(
                spell->mEffects.mList.front().mData.mEffectID);
            const VFS::Path::Normalized iconPath = Misc::ResourceHelpers::correctBigIconPath(
                effect->mIcon.getNormalized(), *MWBase::Environment::get().getResourceSystem()->getVFS());
            mSpellImage->setSpellIcon(iconPath);
        }
        else
            mSpellImage->setSpellIcon({});
    }

    void HUD::setSelectedEnchantItem(const MWWorld::Ptr& item, int chargePercent)
    {
        std::string_view itemName = item.getClass().getName(item);
        if (itemName != mSpellName && mSpellVisible)
        {
            mWeaponSpellTimer = 5.0f;
            mSpellName = itemName;
            mWeaponSpellBox->setCaption(mSpellName);
            mWeaponSpellBox->setVisible(!isDisabledByLua(Element::WeaponSpellName));
        }

        mSpellStatus->setProgressRange(100);
        mSpellStatus->setProgressPosition(chargePercent);

        mSpellBox->setUserString("ToolTipType", "ItemPtr");
        mSpellBox->setUserData(MWWorld::Ptr(item));

        mSpellImage->setItem(item);
    }

    void HUD::setSelectedWeapon(const MWWorld::Ptr& item, int durabilityPercent)
    {
        std::string_view itemName = item.getClass().getName(item);
        if (itemName != mWeaponName && mWeaponVisible)
        {
            mWeaponSpellTimer = 5.0f;
            mWeaponName = itemName;
            mWeaponSpellBox->setCaption(mWeaponName);
            mWeaponSpellBox->setVisible(!isDisabledByLua(Element::WeaponSpellName));
        }

        mWeapBox->clearUserStrings();
        mWeapBox->setUserString("ToolTipType", "ItemPtr");
        mWeapBox->setUserData(MWWorld::Ptr(item));

        mWeapStatus->setProgressRange(100);
        mWeapStatus->setProgressPosition(durabilityPercent);

        mWeapImage->setItem(item);
    }

    void HUD::unsetSelectedSpell()
    {
        std::string_view spellName = "#{Interface:None}";
        if (spellName != mSpellName && mSpellVisible)
        {
            mWeaponSpellTimer = 5.0f;
            mSpellName = spellName;
            mWeaponSpellBox->setCaptionWithReplacing(mSpellName);
            mWeaponSpellBox->setVisible(!isDisabledByLua(Element::WeaponSpellName));
        }

        mSpellStatus->setProgressRange(100);
        mSpellStatus->setProgressPosition(0);
        mSpellImage->setItem(MWWorld::Ptr());
        mSpellBox->clearUserStrings();
        mSpellBox->setUserData(MyGUI::Any::Null);
    }

    void HUD::unsetSelectedWeapon()
    {
        std::string itemName = "#{sSkillHandtohand}";
        if (itemName != mWeaponName && mWeaponVisible)
        {
            mWeaponSpellTimer = 5.0f;
            mWeaponName = itemName;
            mWeaponSpellBox->setCaptionWithReplacing(mWeaponName);
            mWeaponSpellBox->setVisible(!isDisabledByLua(Element::WeaponSpellName));
        }

        mWeapStatus->setProgressRange(100);
        mWeapStatus->setProgressPosition(0);

        MWBase::World* world = MWBase::Environment::get().getWorld();
        MWWorld::Ptr player = world->getPlayerPtr();

        mWeapImage->setItem(MWWorld::Ptr());

        constexpr VFS::Path::NormalizedView werewolfHand("icons/k/tx_werewolf_hand.dds");
        constexpr VFS::Path::NormalizedView stealthHandToHand("icons/k/stealth_handtohand.dds");

        const VFS::Path::NormalizedView icon
            = (player.getClass().getNpcStats(player).isWerewolf()) ? werewolfHand : stealthHandToHand;

        mWeapImage->setIcon(icon);

        mWeapBox->clearUserStrings();
        mWeapBox->setUserString("ToolTipType", "Layout");
        mWeapBox->setUserString("ToolTipLayout", "HandToHandToolTip");
        mWeapBox->setUserString("Caption_HandToHandText", itemName);
        mWeapBox->setUserString("ImageTexture_HandToHandImage", icon.value());
        mWeapBox->setUserData(MyGUI::Any::Null);
    }

    void HUD::setCrosshairVisible(bool visible)
    {
        mCrosshair->setVisible(visible);
    }

    void HUD::setCrosshairOwned(bool owned)
    {
        if (owned)
        {
            mCrosshair->changeWidgetSkin("HUD_Crosshair_Owned");
        }
        else
        {
            mCrosshair->changeWidgetSkin("HUD_Crosshair");
        }
    }

    void HUD::setMouseEmulationCursorVisible(bool visible)
    {
        mMouseEmulationCursor->setVisible(visible);
    }

    void HUD::setMouseEmulationCursorPosition(int left, int top)
    {
        mMouseEmulationCursor->setPosition(
            left - mMouseEmulationCursor->getWidth() / 2, top - mMouseEmulationCursor->getHeight() / 2);
    }

    void HUD::setHmsVisible(bool visible)
    {
        mHmsVisible = visible;
        visible = visible && !isDisabledByLua(Element::DynamicStats);
        mHealth->setVisible(visible);
        mMagicka->setVisible(visible);
        mStamina->setVisible(visible);
        updatePositions();
    }

    void HUD::setWeapVisible(bool visible)
    {
        mWeaponVisible = visible;
        mWeapBox->setVisible(visible && !isDisabledByLua(Element::Weapon));
        updatePositions();
    }

    void HUD::setSpellVisible(bool visible)
    {
        mSpellVisible = visible;
        mSpellBox->setVisible(visible && !isDisabledByLua(Element::Spell));
        updatePositions();
    }

    void HUD::setSneakVisible(bool visible)
    {
        mSneakVisible = visible;
        mSneakBox->setVisible(visible && !isDisabledByLua(Element::Sneak));
        updatePositions();
    }

    void HUD::setEffectVisible(bool visible)
    {
        mEffectVisible = visible;
        mEffectBox->setVisible(visible && !isDisabledByLua(Element::ActiveEffects));
        updatePositions();
    }

    void HUD::setMinimapVisible(bool visible)
    {
        mMapVisible = visible;
        mMinimapBox->setVisible(visible && !isDisabledByLua(Element::Minimap));
        updatePositions();
    }

    void HUD::updatePositions()
    {
        int weapDx = 0, spellDx = 0, sneakDx = 0;
        if (!mHealth->getVisible())
            sneakDx = spellDx = weapDx = mWeapBoxBaseLeft - mHealthManaStaminaBaseLeft;

        if (!mWeapBox->getVisible())
        {
            spellDx += mSpellBoxBaseLeft - mWeapBoxBaseLeft;
            sneakDx = spellDx;
        }

        if (!mSpellBox->getVisible())
            sneakDx += mSneakBoxBaseLeft - mSpellBoxBaseLeft;

        if (!mWeaponVisible && !mSpellVisible)
            mWeaponSpellBox->setVisible(false);

        mWeapBox->setPosition(mWeapBoxBaseLeft - weapDx, mWeapBox->getTop());
        mSpellBox->setPosition(mSpellBoxBaseLeft - spellDx, mSpellBox->getTop());
        mSneakBox->setPosition(mSneakBoxBaseLeft - sneakDx, mSneakBox->getTop());

        const MyGUI::IntSize& viewSize = MyGUI::RenderManager::getInstance().getViewSize();

        // effect box can have variable width -> variable left coordinate
        int effectsDx = 0;
        if (!mMinimapBox->getVisible())
            effectsDx = mEffectBoxBaseRight - mMinimapBoxBaseRight;

        if (!mMapVisible)
            mCellNameBox->setVisible(false);

        mEffectBox->setPosition(
            (viewSize.width - mEffectBoxBaseRight) - mEffectBox->getWidth() + effectsDx, mEffectBox->getTop());
    }

    void HUD::updateEnemyHealthBar()
    {
        MWWorld::Ptr enemy = MWBase::Environment::get().getWorldModel()->getPtr(mEnemyActor);
        if (enemy.isEmpty())
            return;
        MWMechanics::CreatureStats& stats = enemy.getClass().getCreatureStats(enemy);
        mEnemyHealth->setProgressRange(100);
        // Health is usually cast to int before displaying. Actors die whenever they are < 1 health.
        // Therefore any value < 1 should show as an empty health bar. We do the same in statswindow :)
        mEnemyHealth->setProgressPosition(static_cast<size_t>(stats.getHealth().getRatio() * 100));

        static const float fNPCHealthBarFade = MWBase::Environment::get()
                                                   .getESMStore()
                                                   ->get<ESM::GameSetting>()
                                                   .find("fNPCHealthBarFade")
                                                   ->mValue.getFloat();
        if (fNPCHealthBarFade > 0.f)
            mEnemyHealth->setAlpha(std::clamp(mEnemyHealthTimer / fNPCHealthBarFade, 0.f, 1.f));
    }

    void HUD::setEnemy(const MWWorld::Ptr& enemy)
    {
        mEnemyActor = enemy.getCellRef().getRefNum();
        mEnemyHealthTimer = MWBase::Environment::get()
                                .getESMStore()
                                ->get<ESM::GameSetting>()
                                .find("fNPCHealthBarTime")
                                ->mValue.getFloat();
        setEnemyHealthVisible(true);
        updateEnemyHealthBar();
    }

    MWWorld::Ptr HUD::getEnemy(float& secondsLeft) const
    {
        secondsLeft = 0;
        if (!mEnemyActor.isSet() || mEnemyHealthTimer < 0)
            return {};
        secondsLeft = mEnemyHealthTimer;
        return MWBase::Environment::get().getWorldModel()->getPtr(mEnemyActor);
    }

    void HUD::setEnemyHealthVisible(bool visible)
    {
        visible = visible && !isDisabledByLua(Element::EnemyHealth);
        if (mEnemyHealth->getVisible() == visible)
            return;
        mEnemyHealth->setVisible(visible);
        // The weapon or spell name moves up to make room for the enemy's health bar
        mWeaponSpellBox->setPosition(mWeaponSpellBox->getPosition() + MyGUI::IntPoint(0, visible ? -20 : 20));
    }

    void HUD::clear()
    {
        mEnemyActor = {};
        mEnemyHealthTimer = -1;

        mWeaponSpellTimer = 0.f;
        mWeaponName.clear();
        mSpellName.clear();
        mWeaponSpellBox->setVisible(false);

        mWeapStatus->setProgressRange(100);
        mWeapStatus->setProgressPosition(0);
        mSpellStatus->setProgressRange(100);
        mSpellStatus->setProgressPosition(0);

        mWeapImage->setItem(MWWorld::Ptr());
        mSpellImage->setItem(MWWorld::Ptr());

        mWeapBox->clearUserStrings();
        mWeapBox->setUserData(MyGUI::Any::Null);
        mSpellBox->clearUserStrings();
        mSpellBox->setUserData(MyGUI::Any::Null);

        mActiveCell = nullptr;

        for (Element element : std::exchange(mElementsDisabledByLua, {}))
            updateElementVisibility(element);
    }

    std::vector<std::string_view> HUD::getElementIdsForLua()
    {
        std::vector<std::string_view> ids;
        for (const auto& [id, element] : sElementIds)
            ids.push_back(id);
        return ids;
    }

    void HUD::setElementDisabledByLua(std::string_view elementId, bool disabled)
    {
        const Element element = findElement(elementId);
        if (disabled)
            mElementsDisabledByLua.insert(element);
        else
            mElementsDisabledByLua.erase(element);
        updateElementVisibility(element);
    }

    bool HUD::isElementVisible(std::string_view elementId) const
    {
        const MyGUI::Widget* widget = nullptr;
        switch (findElement(elementId))
        {
            case Element::DynamicStats:
                widget = mHealth;
                break;
            case Element::EnemyHealth:
                widget = mEnemyHealth;
                break;
            case Element::Weapon:
                widget = mWeapBox;
                break;
            case Element::Spell:
                widget = mSpellBox;
                break;
            case Element::WeaponSpellName:
                widget = mWeaponSpellBox;
                break;
            case Element::Sneak:
                widget = mSneakBox;
                break;
            case Element::ActiveEffects:
                widget = mEffectBox;
                break;
            case Element::Minimap:
                widget = mMinimapBox;
                break;
            case Element::CellName:
                widget = mCellNameBox;
                break;
            case Element::DrowningBar:
                widget = mDrowningBar;
                break;
        }
        return widget->getInheritedVisible();
    }

    bool HUD::isElementWanted(std::string_view elementId) const
    {
        if (!mMainWidget->getInheritedVisible())
            return false;
        switch (findElement(elementId))
        {
            case Element::DynamicStats:
                return mHmsVisible;
            case Element::EnemyHealth:
                return mEnemyHealthTimer >= 0;
            case Element::Weapon:
                return mWeaponVisible;
            case Element::Spell:
                return mSpellVisible;
            case Element::WeaponSpellName:
                return mWeaponSpellTimer > 0 && (mWeaponVisible || mSpellVisible);
            case Element::Sneak:
                return mSneakVisible;
            case Element::ActiveEffects:
                return mEffectVisible;
            case Element::Minimap:
                return mMapVisible;
            case Element::CellName:
                return mCellNameTimer > 0 && mMapVisible;
            case Element::DrowningBar:
                return mDrowningBarVisible;
        }
        return false;
    }

    void HUD::updateElementVisibility(Element element)
    {
        switch (element)
        {
            case Element::DynamicStats:
                setHmsVisible(mHmsVisible);
                break;
            case Element::EnemyHealth:
                setEnemyHealthVisible(mEnemyHealthTimer >= 0);
                if (mEnemyActor.isSet() && mEnemyHealth->getVisible())
                    updateEnemyHealthBar();
                break;
            case Element::Weapon:
                setWeapVisible(mWeaponVisible);
                break;
            case Element::Spell:
                setSpellVisible(mSpellVisible);
                break;
            case Element::WeaponSpellName:
                mWeaponSpellBox->setVisible(
                    !isDisabledByLua(element) && mWeaponSpellTimer > 0 && (mWeaponVisible || mSpellVisible));
                break;
            case Element::Sneak:
                setSneakVisible(mSneakVisible);
                break;
            case Element::ActiveEffects:
                setEffectVisible(mEffectVisible);
                break;
            case Element::Minimap:
                setMinimapVisible(mMapVisible);
                break;
            case Element::CellName:
                mCellNameBox->setVisible(!isDisabledByLua(element) && mCellNameTimer > 0 && mMapVisible);
                break;
            case Element::DrowningBar:
                setDrowningBarVisible(mDrowningBarVisible);
                break;
        }
    }

    void HUD::customMarkerCreated(MyGUI::Widget* marker)
    {
        marker->eventMouseButtonClick += MyGUI::newDelegate(this, &HUD::onMapClicked);
    }

    void HUD::doorMarkerCreated(MyGUI::Widget* marker)
    {
        marker->eventMouseButtonClick += MyGUI::newDelegate(this, &HUD::onMapClicked);
    }

}
