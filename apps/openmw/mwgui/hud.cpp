#include "hud.hpp"

#include <MyGUI_Button.h>
#include <MyGUI_Gui.h>
#include <MyGUI_ImageBox.h>
#include <MyGUI_InputManager.h>
#include <MyGUI_ProgressBar.h>
#include <MyGUI_RenderManager.h>
#include <MyGUI_ScrollView.h>

#include <algorithm>
#include <cmath>

#include <components/esm3/loadgmst.hpp>
#include <components/esm3/loadmgef.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/myguiplatform/scalinglayer.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/statemanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/player.hpp"
#include "../mwworld/worldmodel.hpp"

#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/npcstats.hpp"

#include "controllerscreen.hpp"
#include "draganddrop.hpp"
#include "inventorywindow.hpp"
#include "itemwidget.hpp"
#include "spellicons.hpp"
#include "worlditemmodel.hpp"

namespace MWGui
{
    namespace
    {
        // At a HUD size of 1, the HUD is laid out as if the screen were this many pixels tall.
        constexpr float sDesignHeight = 720.f;

        // How long HUD elements take to fade in or out, in seconds.
        constexpr float sFadeTime = 0.4f;

        // How long the weapon or spell box stays on screen after the selection changes.
        constexpr float sShowAfterChangeTime = 5.f;

        // How long the cell name stays on screen after entering a cell.
        constexpr float sCellNameTime = 5.f;

        // How long a quick key's name stays on screen after the player uses it.
        constexpr float sQuickKeyTime = 2.f;

        // How often to check whether the player is in combat, in seconds.
        constexpr float sCombatCheckInterval = 0.5f;

        // The opacity of something that is shown for a limited time: it fades in when the time starts and fades
        // out as the time runs out.
        float timedAlpha(float timeLeft, float totalTime)
        {
            return std::clamp(std::min(timeLeft, totalTime - timeLeft) / sFadeTime, 0.f, 1.f);
        }
    }

    void HUD::Fader::update(bool shown, float dt)
    {
        const float step = dt / sFadeTime;
        mAlpha = shown ? std::min(1.f, mAlpha + step) : std::max(0.f, mAlpha - step);
        mWidget->setAlpha(mAlpha);
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

        mHealthFader.mWidget = healthFrame;
        mMagickaFader.mWidget = magickaFrame;
        mFatigueFader.mWidget = fatigueFrame;
        mWeaponFader.mWidget = mWeapBox;
        mSpellFader.mWidget = mSpellBox;

        getWidget(mSneakBox, "SneakBox");
        mSneakBoxBaseLeft = mSneakBox->getLeft();

        getWidget(mEffectBox, "EffectBox");

        // The minimap is still updated, because the HUD shares the local map code with the map window, but
        // it is never shown.
        getWidget(mMinimapBox, "MiniMapBox");
        mMinimapBox->setVisible(false);
        getWidget(mMinimap, "MiniMap");
        getWidget(mCompass, "Compass");
        getWidget(mMinimapButton, "MiniMapButton");
        mMinimapButton->eventMouseButtonClick += MyGUI::newDelegate(this, &HUD::onMapClicked);

        getWidget(mCellNameBox, "CellName");

        getWidget(mCrosshair, "Crosshair");
        getWidget(mMouseEmulationCursor, "MouseEmulationCursor");

        mLocalMapZoom = 0.5f;

        LocalMapBase::init(mMinimap, mCompass);

        mMainWidget->eventMouseButtonClick += MyGUI::newDelegate(this, &HUD::onWorldClicked);
        mMainWidget->eventMouseMove += MyGUI::newDelegate(this, &HUD::onWorldMouseOver);
        mMainWidget->eventMouseLostFocus += MyGUI::newDelegate(this, &HUD::onWorldMouseLostFocus);

        mSpellIcons = std::make_unique<SpellIcons>();

        updateSize();
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
        // A full bar has nothing to tell the player, so it fades out.
        const bool full = current >= modified;
        if (id == "HBar")
        {
            mHealthFull = full;
            mHealth->setProgressRange(std::max(0, modified));
            mHealth->setProgressPosition(std::max(0, current));
            getWidget(w, "HealthFrame");
            w->setUserString("Caption_HealthDescription", "#{sHealthDesc}\n" + valStr);
        }
        else if (id == "MBar")
        {
            mMagickaFull = full;
            mMagicka->setProgressRange(std::max(0, modified));
            mMagicka->setProgressPosition(std::max(0, current));
            getWidget(w, "MagickaFrame");
            w->setUserString("Caption_HealthDescription", "#{sMagDesc}\n" + valStr);
        }
        else if (id == "FBar")
        {
            mFatigueFull = full;
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
        mDrowningBar->setVisible(visible);
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
            mCellNameTimer = sCellNameTime;
            mCellName = cellName;

            mCellNameBox->setCaptionWithReplacing("#{sCell=" + mCellName + "}");
            mCellNameBox->setVisible(true);
        }
    }

    void HUD::showQuickKey(const MWWorld::Ptr& item, const std::string& icon, const std::string& name)
    {
        using namespace ControllerUi;
        if (mQuickKeyBox != nullptr)
            MyGUI::Gui::getInstance().destroyWidget(mQuickKeyBox);

        // The box sits just above the weapon and spell boxes, and fits the name.
        constexpr int height = 40;
        constexpr int iconSize = height - 8;
        mQuickKeyBox = mMainWidget->createWidget<MyGUI::Widget>({},
            MyGUI::IntCoord(mWeapBox->getLeft(), mWeapBox->getTop() - height - 8, 400, height),
            MyGUI::Align::Left | MyGUI::Align::Bottom);
        mQuickKeyBox->setNeedMouseFocus(false);
        MyGUI::ImageBox* back = addRect(mQuickKeyBox, 0, 0, 400, height, sPanelColour, 0.8f);
        if (!item.isEmpty())
            addIcon(mQuickKeyBox, 4, 4, iconSize, item, false);
        else
            addImage(mQuickKeyBox, 4, 4, iconSize, icon);
        MyGUI::TextBox* text
            = addText(mQuickKeyBox, iconSize + 14, 0, 360, height, escaped(name), sFontNormal, sBrightColour);
        const int width = iconSize + 14 + text->getTextSize().width + 12;
        mQuickKeyBox->setSize(width, height);
        back->setSize(width, height);
        mQuickKeyTimer = sQuickKeyTime;
    }

    void HUD::onFrame(float dt)
    {
        LocalMapBase::onFrame(dt);

        if (mQuickKeyBox != nullptr)
        {
            // The box shows at once, and fades out at the end of its time.
            mQuickKeyTimer -= dt;
            if (mQuickKeyTimer <= 0)
            {
                MyGUI::Gui::getInstance().destroyWidget(mQuickKeyBox);
                mQuickKeyBox = nullptr;
            }
            else
                mQuickKeyBox->setAlpha(std::min(1.f, mQuickKeyTimer / sFadeTime));
        }

        mCellNameTimer -= dt;
        if (mCellNameTimer < 0)
            mCellNameBox->setVisible(false);
        mCellNameBox->setAlpha(timedAlpha(mCellNameTimer, sCellNameTime));

        // The weapon and spell boxes show while the weapon or spell is ready, and for a while after it changes.
        mWeaponShowTimer -= dt;
        mSpellShowTimer -= dt;
        MWMechanics::DrawState drawState = MWMechanics::DrawState::Nothing;
        if (MWBase::Environment::get().getStateManager()->getState() == MWBase::StateManager::State_Running)
        {
            const MWWorld::Ptr player = MWMechanics::getPlayer();
            drawState = player.getClass().getCreatureStats(player).getDrawState();

            // Finding who is fighting the player means looking at every nearby actor, so don't do it every frame.
            mCombatCheckTimer -= dt;
            if (mCombatCheckTimer <= 0)
            {
                mCombatCheckTimer = sCombatCheckInterval;
                mInCombat = MWBase::Environment::get().getWorld()->getPlayer().isInCombat();
            }
        }
        else
            mInCombat = false;

        // During combat everything stays on screen, including an enemy the player attacked first.
        const bool showAll = mInCombat || mEnemyHealth->getVisible();
        mWeaponFader.update(showAll || drawState == MWMechanics::DrawState::Weapon || mWeaponShowTimer > 0, dt);
        mSpellFader.update(showAll || drawState == MWMechanics::DrawState::Spell || mSpellShowTimer > 0, dt);

        mHealthFader.update(showAll || !mHealthFull, dt);
        mMagickaFader.update(showAll || !mMagickaFull, dt);
        mFatigueFader.update(showAll || !mFatigueFull, dt);

        mEnemyHealthTimer -= dt;
        if (mEnemyHealth->getVisible() && mEnemyHealthTimer < 0)
            mEnemyHealth->setVisible(false);

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
            mSpellShowTimer = sShowAfterChangeTime;
            mSpellName = spellName;
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
            mSpellShowTimer = sShowAfterChangeTime;
            mSpellName = itemName;
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
            mWeaponShowTimer = sShowAfterChangeTime;
            mWeaponName = itemName;
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
            mSpellShowTimer = sShowAfterChangeTime;
            mSpellName = spellName;
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
            mWeaponShowTimer = sShowAfterChangeTime;
            mWeaponName = itemName;
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
        mHealth->setVisible(visible);
        mMagicka->setVisible(visible);
        mStamina->setVisible(visible);
        updatePositions();
    }

    void HUD::setWeapVisible(bool visible)
    {
        mWeapBox->setVisible(visible);
        updatePositions();
    }

    void HUD::setSpellVisible(bool visible)
    {
        mSpellBox->setVisible(visible);
        updatePositions();
    }

    void HUD::setSneakVisible(bool visible)
    {
        mSneakBox->setVisible(visible);
        updatePositions();
    }

    void HUD::setEffectVisible(bool visible)
    {
        mEffectBox->setVisible(visible);
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

        mWeaponVisible = mWeapBox->getVisible();
        mSpellVisible = mSpellBox->getVisible();

        mWeapBox->setPosition(mWeapBoxBaseLeft - weapDx, mWeapBox->getTop());
        mSpellBox->setPosition(mSpellBoxBaseLeft - spellDx, mSpellBox->getTop());
        mSneakBox->setPosition(mSneakBoxBaseLeft - sneakDx, mSneakBox->getTop());
    }

    void HUD::onResChange(int /*width*/, int /*height*/)
    {
        // The HUD layer scales its contents, so the HUD fills the layer's own size rather than the screen's.
        mMainWidget->setCoord(MyGUI::IntCoord(MyGUI::IntPoint(), mMainWidget->getLayer()->getSize()));
    }

    void HUD::updateSize()
    {
        if (auto* layer = dynamic_cast<MyGUIPlatform::ScalingLayer*>(mMainWidget->getLayer()))
            layer->setFixedHeight(static_cast<int>(std::lround(sDesignHeight / Settings::hud().mSize.get())));
        onResChange(0, 0);
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
        mEnemyHealth->setVisible(true);
        updateEnemyHealthBar();
    }

    void HUD::clear()
    {
        if (mQuickKeyBox != nullptr)
        {
            MyGUI::Gui::getInstance().destroyWidget(mQuickKeyBox);
            mQuickKeyBox = nullptr;
        }
        mEnemyActor = {};
        mEnemyHealthTimer = -1;

        mWeaponName.clear();
        mSpellName.clear();

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
