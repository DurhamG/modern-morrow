#include "touchmanager.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <set>
#include <string>

#include <MyGUI_InputManager.h>
#include <MyGUI_ScrollBar.h>

#include <SDL_gamecontroller.h>
#include <SDL_timer.h>
#include <SDL_video.h>

#include <osg/Math>

#include <components/debug/debuglog.hpp>
#include <components/files/conversion.hpp>
#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/statemanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwgui/controllerscreen.hpp"
#include "../mwgui/mapwindow.hpp"
#include "../mwgui/mode.hpp"
#include "../mwgui/touchoverlay.hpp"

#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/creaturestats.hpp"
#include "../mwmechanics/drawstate.hpp"

#include "../mwstate/character.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/datetimemanager.hpp"
#include "../mwworld/player.hpp"

#include "actions.hpp"
#include "bindingsmanager.hpp"
#include "mousemanager.hpp"

namespace MWInput
{
    namespace
    {
        // Starting values, to tune on a phone. Distances are in millimetres on the glass, so screens of any size and
        // density feel the same. See ui-rework/design/touch.md.

        // A tap lifts within this time, having moved less than the slop.
        constexpr Uint32 sTapTime = 250;
        constexpr float sSlopMm = 3.f;
        // The right thumb starts charging the attack when it stays still this long in attack mode.
        constexpr Uint32 sChargeDelay = 180;
        // Walking starts past the dead zone and is at full speed this far from where the thumb went down, in any
        // direction, like a stick. Half way there, the player runs.
        constexpr float sMoveDeadZoneMm = 2.f;
        constexpr float sMoveFullMm = 15.f;
        // How far the camera turns for each millimetre the right thumb drags, in radians, before the camera
        // sensitivity setting. That holds within a circle around where the thumb went down. Past its edge to either
        // side, the camera keeps turning that way, like a stick: from standing still at the edge, so there's no jump
        // in speed there, rising evenly to the full speed a ramp's length farther out, in radians a second.
        constexpr float sLookRadiansPerMm = 0.026f;
        constexpr float sLookCircleMm = 10.f;
        constexpr float sLookRampMm = 20.f;
        const float sLookFullSpeed = osg::DegreesToRadians(130.f);
        // On the map, two fingers that touch down this close together in time are a pinch, which zooms.
        constexpr Uint32 sPinchWindow = 150;
        // A finger that goes down this close to a side edge, in the middle half of the edge's height, swipes in from
        // the edge once it has moved this far inwards, more in than up or down. GameActivity keeps Android's back
        // gesture off the same part of the edges.
        constexpr float sEdgeMm = 5.f;
        constexpr float sEdgeSwipeMm = 8.f;
        // The corners' squares.
        constexpr float sCornerMm = 12.f;
        // How long the quickload corner is held.
        constexpr Uint32 sQuickLoadTime = 1000;
        // The mode picker's items, and how far apart their centres are across and up and down. Use is under the
        // thumb, attack and then the quick keys to its left, magic above and jump below, in the order of the modes.
        constexpr float sPickerItemWidthMm = 16.f;
        constexpr float sPickerItemHeightMm = 9.f;
        constexpr float sPickerAcrossMm = 19.f;
        constexpr float sPickerUpDownMm = 13.f;
        constexpr float sPickerOffsets[][2] = { { 0, 0 }, { -1, 0 }, { -2, 0 }, { 0, -1 }, { 0, 1 } };
        // The quick keys radial: the press slots' ring, the hold slots' ring, and each slot's icon.
        constexpr float sRadialInnerMm = 16.f;
        constexpr float sRadialOuterMm = 30.f;
        constexpr float sRadialIconMm = 10.f;
        // A drag of this length in a menu scrolls a list by one row.
        constexpr float sScrollStepMm = 6.f;
        // A drag in a menu that is more sideways than up or down switches tabs, one for each time it goes this far, and
        // back again if it comes back: the big tabs, as the triggers do, on the left half of the screen, and the
        // screen's own tabs, as the shoulder buttons do, on the right half.
        constexpr float sTabSwipeMm = 12.f;
        // How many frames a tap holds the attack, so the game sees it.
        constexpr int sUseFrames = 3;
        // The tutorial shows again for a character after this long without playing with touch.
        constexpr std::chrono::seconds sTutorialAgain = std::chrono::hours(24 * 14);
        // How far in from a side edge the tutorial draws the edge swipes' bands, wider than they are so they show.
        constexpr float sTutorialEdgeMm = 8.f;

        // The tutorial's record is a line "played <seconds since 1970>", for when the game was last played with
        // touch, and a line "seen <folder>" for each character, by its folder of saves, that has seen the tutorial.
        struct TutorialRecord
        {
            std::int64_t mLastPlayed = 0;
            std::set<std::string> mCharacters;
        };

        TutorialRecord readTutorialRecord(const std::filesystem::path& file)
        {
            TutorialRecord record;
            std::ifstream stream(file);
            std::string line;
            while (std::getline(stream, line))
            {
                if (line.starts_with("played "))
                    record.mLastPlayed = std::atoll(line.c_str() + 7);
                else if (line.starts_with("seen "))
                    record.mCharacters.insert(line.substr(5));
            }
            return record;
        }

        void writeTutorialRecord(const std::filesystem::path& file, const TutorialRecord& record)
        {
            std::ofstream stream(file);
            stream << "played " << record.mLastPlayed << '\n';
            for (const std::string& character : record.mCharacters)
                stream << "seen " << character << '\n';
            if (!stream)
                Log(Debug::Warning) << "Failed to write the touch tutorial's record to " << file;
        }

        std::int64_t secondsNow()
        {
            return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
        }

        // The current character's folder of saves, or nothing for a new game's character, which has none yet.
        std::string currentCharacterFolder()
        {
            const MWState::Character* character = MWBase::Environment::get().getStateManager()->getCurrentCharacter();
            return character != nullptr ? Files::pathToUnicodeString(character->getPath().filename()) : std::string();
        }

        float distance(const MyGUI::FloatPoint& a, const MyGUI::FloatPoint& b)
        {
            return std::hypot(a.left - b.left, a.top - b.top);
        }

        bool contains(const MyGUI::IntCoord& box, const MyGUI::FloatPoint& point, float margin)
        {
            return point.left >= box.left - margin && point.left <= box.right() + margin
                && point.top >= box.top - margin && point.top <= box.bottom() + margin;
        }
    }

    TouchManager::TouchManager(BindingsManager* bindingsManager, MouseManager* mouseManager, SDL_Window* window,
        const std::filesystem::path& tutorialFile)
        : mBindingsManager(bindingsManager)
        , mMouseManager(mouseManager)
        , mWindow(window)
        , mOverlay(std::make_unique<MWGui::TouchOverlay>())
        , mTutorialFile(tutorialFile)
    {
    }

    TouchManager::~TouchManager() = default;

    TouchManager::Finger* TouchManager::find(SDL_FingerID id)
    {
        for (Finger& finger : mFingers)
            if (finger.mId == id)
                return &finger;
        return nullptr;
    }

    MyGUI::IntSize TouchManager::screenSize() const
    {
        int width = 0;
        int height = 0;
        SDL_GL_GetDrawableSize(mWindow, &width, &height);
        return { std::max(1, width), std::max(1, height) };
    }

    MyGUI::FloatPoint TouchManager::toPixels(const SDL_TouchFingerEvent& arg) const
    {
        const MyGUI::IntSize size = screenSize();
        return { arg.x * size.width, arg.y * size.height };
    }

    float TouchManager::pixelsPerMm() const
    {
        float dpi = 0;
        const int display = SDL_GetWindowDisplayIndex(mWindow);
        if (display < 0 || SDL_GetDisplayDPI(display, &dpi, nullptr, nullptr) != 0 || dpi <= 0)
            dpi = 160;
        // The DPI is for window coordinates, which can be smaller than the drawable's pixels.
        int windowWidth = 0;
        SDL_GetWindowSize(mWindow, &windowWidth, nullptr);
        const float pixelsPerPoint = windowWidth > 0 ? static_cast<float>(screenSize().width) / windowWidth : 1.f;
        return dpi / 25.4f * pixelsPerPoint;
    }

    bool TouchManager::isTap(const Finger& finger, Uint32 upTime) const
    {
        return upTime - finger.mDownTime <= sTapTime && finger.mFarthest < sSlopMm * pixelsPerMm();
    }

    bool TouchManager::inGame() const
    {
        const MWBase::Environment& env = MWBase::Environment::get();
        return env.getStateManager()->getState() == MWBase::StateManager::State_Running
            && !env.getWindowManager()->isGuiMode() && !env.getInputManager()->controlsDisabled();
    }

    int TouchManager::currentMode() const
    {
        return mJumpMode ? Mode_Jump : drawnMode();
    }

    int TouchManager::drawnMode() const
    {
        const MWWorld::Ptr player = MWMechanics::getPlayer();
        switch (player.getClass().getCreatureStats(player).getDrawState())
        {
            case MWMechanics::DrawState::Weapon:
                return Mode_Attack;
            case MWMechanics::DrawState::Spell:
                return Mode_Magic;
            default:
                return Mode_Use;
        }
    }

    TouchManager::Role TouchManager::roleForGame(const MyGUI::FloatPoint& point) const
    {
        const MyGUI::IntSize size = screenSize();
        const float corner = sCornerMm * pixelsPerMm();
        const bool top = point.top < corner;
        const bool left = point.left < corner;
        const bool right = point.left > size.width - corner;
        if (top && left)
            return Role::QuickSave;
        if (top && right)
            return Role::QuickLoad;
        if (right && point.top > size.height - corner)
            return Role::Sneak;
        return point.left < size.width / 2.f ? Role::Move : Role::Look;
    }

    int TouchManager::edgeAt(const MyGUI::FloatPoint& point) const
    {
        const MyGUI::IntSize size = screenSize();
        if (point.top < size.height / 4.f || point.top > size.height * 3 / 4.f)
            return 0;
        const float edge = sEdgeMm * pixelsPerMm();
        if (point.left < edge)
            return -1;
        if (point.left > size.width - edge)
            return 1;
        return 0;
    }

    void TouchManager::fingerPressed(const SDL_TouchFingerEvent& arg)
    {
        MWBase::Environment::get().getInputManager()->resetIdleTime();
        Finger finger;
        finger.mId = arg.fingerId;
        finger.mStart = finger.mPosition = toPixels(arg);
        finger.mDownTime = arg.timestamp;

        if (mTutorialPage != TutorialPage::None)
        {
            finger.mRole = Role::Tutorial;
            mFingers.push_back(finger);
            return;
        }

        if (MWBase::Environment::get().getWindowManager()->isGuiMode())
        {
            finger.mRole = Role::Menu;
            menuPointer(finger.mPosition);
            const MyGUI::IntPoint gui = toGui(finger.mPosition);
            finger.mMouse
                = !mMouseManager->usesControllerMenus() || !MWGui::ControllerUi::pointerTargetAt(gui.left, gui.top);
            // A slider is dragged sideways, so a finger on one never switches tabs.
            MyGUI::Widget* widget = MyGUI::InputManager::getInstance().getMouseFocusWidget();
            for (; widget != nullptr && !finger.mOnSlider; widget = widget->getParent())
                finger.mOnSlider = widget->isType<MyGUI::ScrollBar>();
        }
        else if (!inGame())
            finger.mRole = Role::Ignored;
        else if (mRadialOpen)
            finger.mRole = Role::Radial;
        else if (mPickerOpen)
            finger.mRole = Role::Ignored;
        else
        {
            finger.mRole = roleForGame(finger.mPosition);
            // One thumb per half of the screen. Another finger on the same half waits for the first to lift.
            for (const Finger& other : mFingers)
            {
                const Role otherRole = other.mRole == Role::Edge ? other.mFallback : other.mRole;
                if ((finger.mRole == Role::Move || finger.mRole == Role::Look) && otherRole == finger.mRole)
                    finger.mRole = Role::Ignored;
            }
        }

        // Either thumb can go down by its edge and swipe in. Until it does, it waits to see.
        finger.mEdge = edgeAt(finger.mPosition);
        if (finger.mEdge != 0
            && (finger.mRole == Role::Menu || finger.mRole == Role::Move || finger.mRole == Role::Look))
        {
            finger.mFallback = finger.mRole;
            finger.mRole = Role::Edge;
        }

        if (finger.mRole == Role::QuickLoad)
            mOverlay->showProgress("#{Interface:TouchQuickLoad}", 0.f);

        mFingers.push_back(finger);
        startPinch(mFingers.back());
    }

    void TouchManager::updateEdge(Finger& finger)
    {
        const float mm = pixelsPerMm();
        if (finger.mFarthest < sSlopMm * mm)
            return;
        const float inwards = (finger.mPosition.left - finger.mStart.left) * -finger.mEdge;
        const float across = std::abs(finger.mPosition.top - finger.mStart.top);
        if (inwards > across && inwards < sEdgeSwipeMm * mm)
            return;
        if (inwards <= across)
        {
            finger.mRole = finger.mFallback;
            return;
        }

        if (MWBase::Environment::get().getWindowManager()->isGuiMode())
        {
            finger.mRole = Role::Ignored;
            menuBack();
        }
        else if (!inGame())
            finger.mRole = Role::Ignored;
        else if (finger.mEdge < 0)
        {
            finger.mRole = Role::Ignored;
            queueAction(A_Inventory);
        }
        else
        {
            finger.mRole = Role::Select;
            openPicker(finger);
        }
    }

    bool TouchManager::startPinch(Finger& finger)
    {
        if (finger.mRole != Role::Menu)
            return false;
        MWBase::WindowManager* winMgr = MWBase::Environment::get().getWindowManager();
        if (dynamic_cast<MWGui::MapWindow*>(winMgr->getActiveControllerWindow()) == nullptr)
            return false;
        const float slop = sSlopMm * pixelsPerMm();
        for (Finger& other : mFingers)
        {
            if (&other == &finger || finger.mDownTime - other.mDownTime > sPinchWindow)
                continue;
            const bool fresh
                = other.mRole == Role::Menu && other.mScrolled == 0 && !other.mPressed && other.mFarthest < slop;
            if (!fresh)
                continue;
            finger.mRole = Role::Pinch;
            other.mRole = Role::Pinch;
            mPinchZoomedAt = distance(finger.mStart, other.mStart);
            return true;
        }
        return false;
    }

    void TouchManager::fingerMoved(const SDL_TouchFingerEvent& arg)
    {
        Finger* finger = find(arg.fingerId);
        if (finger == nullptr)
            return;
        MWBase::Environment::get().getInputManager()->resetIdleTime();
        const MyGUI::FloatPoint position = toPixels(arg);
        const MyGUI::FloatPoint delta(position.left - finger->mPosition.left, position.top - finger->mPosition.top);
        finger->mPosition = position;
        finger->mFarthest = std::max(finger->mFarthest, distance(finger->mStart, position));
        const float slop = sSlopMm * pixelsPerMm();

        // An edge finger that turns out not to swipe in carries on as its fallback from here.
        if (finger->mRole == Role::Edge)
            updateEdge(*finger);

        switch (finger->mRole)
        {
            case Role::Look:
                if (inGame())
                    look(*finger, MyGUI::FloatPoint(position.left - delta.left, position.top - delta.top));
                break;
            case Role::Move:
                if (inGame())
                    setMovement(*finger);
                break;
            case Role::Select:
                updatePicker(*finger);
                break;
            case Role::Pinch:
                updatePinch();
                break;
            case Role::Menu:
                if (finger->mFarthest < slop)
                    break;
                if (!finger->mDecided)
                {
                    finger->mDecided = true;
                    finger->mTabSwipe
                        = std::abs(position.left - finger->mStart.left) > std::abs(position.top - finger->mStart.top)
                        && canSwipeTabs(*finger);
                }
                if (finger->mTabSwipe)
                    menuSwipeTabs(*finger);
                else if (!finger->mMouse)
                    menuScroll(*finger);
                else
                {
                    const MyGUI::IntPoint start = toGui(finger->mStart);
                    const MyGUI::IntPoint now = toGui(position);
                    if (!finger->mPressed)
                    {
                        MyGUI::InputManager::getInstance().injectMouseMove(start.left, start.top, 0);
                        MyGUI::InputManager::getInstance().injectMousePress(
                            start.left, start.top, MyGUI::MouseButton::Left);
                        finger->mPressed = true;
                    }
                    MyGUI::InputManager::getInstance().injectMouseMove(now.left, now.top, 0);
                }
                break;
            default:
                break;
        }
    }

    void TouchManager::fingerReleased(const SDL_TouchFingerEvent& arg)
    {
        Finger* found = find(arg.fingerId);
        if (found == nullptr)
            return;
        Finger finger = *found;
        mFingers.erase(mFingers.begin() + (found - mFingers.data()));
        MWBase::Environment::get().getInputManager()->resetIdleTime();

        // An edge finger that lifts without swiping in, such as a tap, does what its fallback would have.
        if (finger.mRole == Role::Edge)
            finger.mRole = finger.mFallback;

        const bool tap = isTap(finger, arg.timestamp);
        const bool still = finger.mFarthest < sSlopMm * pixelsPerMm();
        switch (finger.mRole)
        {
            case Role::Look:
                if (finger.mHolding)
                    pressUse(false);
                else if (tap && inGame())
                    act();
                break;
            case Role::Move:
                stopMovement();
                break;
            case Role::Select:
                closePicker(true);
                break;
            case Role::QuickSave:
                if (still && inGame())
                    queueAction(A_QuickSave);
                break;
            case Role::QuickLoad:
                mOverlay->hide();
                break;
            case Role::Sneak:
                if (still && inGame())
                    queueAction(A_Sneak);
                break;
            case Role::Radial:
                if (still)
                    tapRadial(finger.mPosition);
                break;
            case Role::Tutorial:
                if (still)
                    nextTutorialPage();
                break;
            case Role::Menu:
                if (finger.mPressed)
                {
                    const MyGUI::IntPoint at = toGui(finger.mPosition);
                    MyGUI::InputManager::getInstance().injectMouseRelease(at.left, at.top, MyGUI::MouseButton::Left);
                }
                else if (tap || (still && finger.mScrolled == 0))
                    menuTap(finger.mPosition);
                break;
            default:
                break;
        }
    }

    void TouchManager::updatePinch()
    {
        std::vector<Finger*> pinch;
        for (Finger& finger : mFingers)
            if (finger.mRole == Role::Pinch)
                pinch.push_back(&finger);
        if (pinch.size() != 2)
            return;

        // A pinch zooms the map for as long as the fingers stay down, as on a phone's own maps.
        const float now = distance(pinch[0]->mPosition, pinch[1]->mPosition);
        MWBase::WindowManager* winMgr = MWBase::Environment::get().getWindowManager();
        auto* map
            = winMgr->isGuiMode() ? dynamic_cast<MWGui::MapWindow*>(winMgr->getActiveControllerWindow()) : nullptr;
        if (map != nullptr && now > 0 && mPinchZoomedAt > 0)
        {
            const MyGUI::FloatPoint middle((pinch[0]->mPosition.left + pinch[1]->mPosition.left) / 2,
                (pinch[0]->mPosition.top + pinch[1]->mPosition.top) / 2);
            map->zoomByPinch(now / mPinchZoomedAt, toGui(middle));
        }
        mPinchZoomedAt = now;
    }

    void TouchManager::look(const Finger& finger, const MyGUI::FloatPoint& previous)
    {
        // Only the part of a sideways move inside the circle turns the camera directly. Past the edge, update()
        // keeps turning instead.
        const float mm = pixelsPerMm();
        const float circle = sLookCircleMm * mm;
        const auto inside = [&](float x) { return std::clamp(x - finger.mStart.left, -circle, circle); };
        turn((inside(finger.mPosition.left) - inside(previous.left)) / mm * sLookRadiansPerMm,
            (finger.mPosition.top - previous.top) / mm * sLookRadiansPerMm);
    }

    void TouchManager::keepTurning(const Finger& finger, float dt)
    {
        const float mm = pixelsPerMm();
        const float past = std::abs(finger.mPosition.left - finger.mStart.left) / mm - sLookCircleMm;
        if (past <= 0)
            return;
        const float speed = sLookFullSpeed * std::min(1.f, past / sLookRampMm);
        turn(std::copysign(speed * dt, finger.mPosition.left - finger.mStart.left), 0.f);
    }

    void TouchManager::turn(float yaw, float pitch)
    {
        MWBase::InputManager* input = MWBase::Environment::get().getInputManager();
        const float sensitivity = Settings::input().mCameraSensitivity;
        const float x = yaw * sensitivity * (Settings::input().mInvertXAxis ? -1 : 1);
        const float y
            = pitch * sensitivity * (Settings::input().mInvertYAxis ? -1 : 1) * Settings::input().mCameraYMultiplier;

        // As the mouse turns the camera: only the player turns, outside of vanity mode.
        float rotation[3] = { -y, 0.f, -x };
        MWBase::World* world = MWBase::Environment::get().getWorld();
        if (!world->vanityRotateCamera(rotation) && input->getControlSwitch("playerlooking"))
        {
            MWWorld::Player& player = world->getPlayer();
            player.yaw(x);
            player.pitch(y);
        }
        else if (!input->getControlSwitch("playerlooking"))
            world->disableDeferredPreviewRotation();
    }

    void TouchManager::act()
    {
        const int mode = currentMode();
        if (mode == Mode_Use)
            queueAction(A_Activate);
        else if (mode == Mode_Jump)
            queueAction(A_Jump);
        else
        {
            // Attacking and casting are the Use action, which a tap holds for a few frames.
            pressUse(true);
            mUseFrames = sUseFrames;
        }
    }

    void TouchManager::setMovement(const Finger& finger)
    {
        // Up the screen is forward. The actions' values run from 0 for full forward or left to 1 for full back or
        // right, as a stick's do.
        const float mm = pixelsPerMm();
        const float right = (finger.mPosition.left - finger.mStart.left) / mm;
        const float forward = (finger.mStart.top - finger.mPosition.top) / mm;
        const float length = std::hypot(right, forward);
        const float amount = std::clamp((length - sMoveDeadZoneMm) / (sMoveFullMm - sMoveDeadZoneMm), 0.f, 1.f)
            / std::max(length, 1e-3f);
        mBindingsManager->setTouchActionValue(A_MoveForwardBackward, 0.5f - forward * amount / 2.f);
        mBindingsManager->setTouchActionValue(A_MoveLeftRight, 0.5f + right * amount / 2.f);
        mMoving = true;
    }

    void TouchManager::stopMovement()
    {
        if (!mMoving)
            return;
        mBindingsManager->clearTouchActionValue(A_MoveForwardBackward);
        mBindingsManager->clearTouchActionValue(A_MoveLeftRight);
        mMoving = false;
    }

    void TouchManager::pressUse(bool pressed)
    {
        if (pressed == mUseHeld)
            return;
        if (pressed)
            mBindingsManager->setTouchActionValue(A_Use, 1.f);
        else
            mBindingsManager->clearTouchActionValue(A_Use);
        mUseHeld = pressed;
        mUseFrames = 0;
    }

    void TouchManager::openPicker(const Finger& finger)
    {
        mPickerOpen = true;
        mPickerCurrent = currentMode();
        mPickerOrigin = finger.mPosition;
        pause(true);

        // Use sits under the thumb, as far as the screen allows the others to fit around it.
        const MyGUI::IntSize size = screenSize();
        const float mm = pixelsPerMm();
        const float width = sPickerItemWidthMm * mm;
        const float height = sPickerItemHeightMm * mm;
        const float across = sPickerAcrossMm * mm;
        const float upDown = sPickerUpDownMm * mm;
        const float margin = 2 * mm;
        const float minX = margin + 2 * across + width / 2;
        const float minY = margin + upDown + height / 2;
        mPickerCentre.left = std::clamp(finger.mPosition.left, minX, std::max(minX, size.width - margin - width / 2));
        mPickerCentre.top = std::clamp(finger.mPosition.top, minY, std::max(minY, size.height - minY));
        mPickerBoxes.clear();
        for (const auto& offset : sPickerOffsets)
        {
            const float x = mPickerCentre.left + offset[0] * across;
            const float y = mPickerCentre.top + offset[1] * upDown;
            mPickerBoxes.emplace_back(static_cast<int>(x - width / 2), static_cast<int>(y - height / 2),
                static_cast<int>(width), static_cast<int>(height));
        }

        mPickerHighlight = -1;
        updatePicker(finger);
    }

    void TouchManager::updatePicker(const Finger& finger)
    {
        if (!mPickerOpen || mPickerBoxes.empty())
            return;
        // The item nearest to how far the thumb has moved since the picker opened, so that Use is under it then.
        const float mm = pixelsPerMm();
        const float dx = (finger.mPosition.left - mPickerOrigin.left) / (sPickerAcrossMm * mm);
        const float dy = (finger.mPosition.top - mPickerOrigin.top) / (sPickerUpDownMm * mm);
        int highlight = Mode_Use;
        float nearest = std::numeric_limits<float>::max();
        for (int i = 0; i < Mode_Count; ++i)
        {
            const float d = std::hypot(dx - sPickerOffsets[i][0], dy - sPickerOffsets[i][1]);
            if (d < nearest)
            {
                nearest = d;
                highlight = i;
            }
        }
        if (highlight == mPickerHighlight)
            return;
        mPickerHighlight = highlight;
        static const std::vector<std::string> captions = { "#{Interface:TouchModeUse}", "#{Interface:TouchModeAttack}",
            "#{Interface:TouchQuickKeys}", "#{Interface:TouchModeMagic}", "#{Interface:TouchModeJump}" };
        mOverlay->showModes(mPickerBoxes, captions, mPickerCurrent, mPickerHighlight, screenSize());
    }

    void TouchManager::closePicker(bool commit)
    {
        if (!mPickerOpen)
            return;
        mPickerOpen = false;
        mOverlay->hide();
        const int target = mPickerHighlight;
        if (commit && target == Mode_QuickKeys)
        {
            openRadial();
            return;
        }
        pause(false);
        if (!commit || target < 0)
            return;
        mJumpMode = target == Mode_Jump;
        const int drawn = drawnMode();
        if (mJumpMode || target == drawn)
            return;
        // Readying the weapon or the spell puts the other away, and readying the one that's out puts it away.
        const bool weapon = target == Mode_Attack || (drawn == Mode_Attack && target == Mode_Use);
        queueAction(weapon ? A_ToggleWeapon : A_ToggleSpell, 2);
    }

    void TouchManager::openRadial()
    {
        mRadialOpen = true;
        pause(true);

        // The slots go in the d-pad's directions around where the thumb went down: the press slots on the inner
        // ring, the hold slots on the outer one. A small screen shrinks the rings to fit.
        const MyGUI::IntSize size = screenSize();
        const float mm = pixelsPerMm();
        const float icon = sRadialIconMm * mm;
        const float label = 4 * mm;
        const float room = std::min(size.width / 2.f, size.height / 2.f) - icon / 2 - label;
        const float scale = std::min(1.f, room / (sRadialOuterMm * mm));
        const float inner = sRadialInnerMm * mm * scale;
        const float outer = sRadialOuterMm * mm * scale;
        const float reach = outer + icon / 2;
        const float x = std::clamp(mPickerCentre.left, reach, std::max(reach, size.width - reach));
        const float y = std::clamp(mPickerCentre.top, reach, std::max(reach, size.height - reach - label));

        constexpr float directions[4][2] = { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 } };
        mRadialSlots.clear();
        for (int i = 0; i < 8; ++i)
        {
            const float radius = i < 4 ? inner : outer;
            const float cx = x + directions[i % 4][0] * radius;
            const float cy = y + directions[i % 4][1] * radius;
            mRadialSlots.emplace_back(static_cast<int>(cx - icon / 2), static_cast<int>(cy - icon / 2),
                static_cast<int>(icon), static_cast<int>(icon));
        }
        mRadialCentre = MyGUI::IntCoord(static_cast<int>(x - icon / 2), static_cast<int>(y - icon / 4),
            static_cast<int>(icon), static_cast<int>(icon / 2));
        mOverlay->showRadial(mRadialSlots, mRadialCentre, size);
    }

    void TouchManager::tapRadial(const MyGUI::FloatPoint& point)
    {
        const float margin = 2 * pixelsPerMm();
        int slot = -1;
        for (std::size_t i = 0; i < mRadialSlots.size(); ++i)
            if (contains(mRadialSlots[i], point, margin))
                slot = static_cast<int>(i) + 1;
        closeRadial();
        if (slot > 0)
            queueAction(A_QuickKey1 + slot - 1, 2);
    }

    void TouchManager::closeRadial()
    {
        if (!mRadialOpen)
            return;
        mRadialOpen = false;
        mOverlay->hide();
        pause(false);
    }

    MyGUI::IntPoint TouchManager::toGui(const MyGUI::FloatPoint& point) const
    {
        const float scale = MWBase::Environment::get().getWindowManager()->getScalingFactor();
        return { static_cast<int>(point.left / scale), static_cast<int>(point.top / scale) };
    }

    void TouchManager::menuPointer(const MyGUI::FloatPoint& point)
    {
        const float scale = MWBase::Environment::get().getWindowManager()->getScalingFactor();
        const int x = static_cast<int>(point.left / scale);
        const int y = static_cast<int>(point.top / scale);
        MyGUI::InputManager::getInstance().injectMouseMove(x, y, 0);
    }

    void TouchManager::menuTap(const MyGUI::FloatPoint& point)
    {
        menuPointer(point);
        const float scale = MWBase::Environment::get().getWindowManager()->getScalingFactor();
        const int x = static_cast<int>(point.left / scale);
        const int y = static_cast<int>(point.top / scale);
        if (mMouseManager->usesControllerMenus())
        {
            if (MWGui::ControllerUi::pointerTapped(x, y))
                return;
            // Tapping the game beside a conversation's panel, such as the speaker above it, ends the conversation.
            if (!MWGui::ControllerUi::pointerOnPanel(x, y) && leaveConversation())
                return;
        }
        MyGUI::InputManager::getInstance().injectMousePress(x, y, MyGUI::MouseButton::Left);
        MyGUI::InputManager::getInstance().injectMouseRelease(x, y, MyGUI::MouseButton::Left);
    }

    void TouchManager::menuScroll(Finger& finger)
    {
        // Dragging up moves the list up, which shows the rows below, as turning the wheel towards the player does.
        const float step = sScrollStepMm * pixelsPerMm();
        const float dragged = (finger.mStart.top - finger.mPosition.top) / step;
        const int steps = static_cast<int>(dragged) - static_cast<int>(finger.mScrolled);
        if (steps == 0)
            return;
        finger.mScrolled += steps;
        if (!mMouseManager->usesControllerMenus())
            return;
        const float scale = MWBase::Environment::get().getWindowManager()->getScalingFactor();
        const int x = static_cast<int>(finger.mStart.left / scale);
        const int y = static_cast<int>(finger.mStart.top / scale);
        MWBase::InputManager* input = MWBase::Environment::get().getInputManager();
        for (int i = 0; i < std::abs(steps); ++i)
        {
            const int wheel = steps > 0 ? -1 : 1;
            if (!MWGui::ControllerUi::pointerWheel(x, y, wheel))
                input->pressControllerMenuButton(
                    wheel > 0 ? SDL_CONTROLLER_BUTTON_DPAD_UP : SDL_CONTROLLER_BUTTON_DPAD_DOWN);
        }
    }

    bool TouchManager::canSwipeTabs(const Finger& finger) const
    {
        // The map pans when dragged, and its tabs can be tapped instead.
        MWBase::WindowManager* winMgr = MWBase::Environment::get().getWindowManager();
        return mMouseManager->usesControllerMenus() && !finger.mOnSlider
            && dynamic_cast<MWGui::MapWindow*>(winMgr->getActiveControllerWindow()) == nullptr;
    }

    void TouchManager::menuSwipeTabs(Finger& finger)
    {
        // Dragging right goes to the tabs on the right. The tab follows the finger, so overshooting is undone by
        // pulling back.
        const float dragged = (finger.mPosition.left - finger.mStart.left) / (sTabSwipeMm * pixelsPerMm());
        const int tabs = static_cast<int>(dragged);
        const bool big = finger.mStart.left < screenSize().width / 2.f;
        for (; finger.mTabsMoved != tabs; finger.mTabsMoved += tabs > finger.mTabsMoved ? 1 : -1)
        {
            const bool right = tabs > finger.mTabsMoved;
            if (big)
                MWBase::Environment::get().getWindowManager()->controllerTriggerPressed(right);
            else
                MWBase::Environment::get().getInputManager()->pressControllerMenuButton(
                    right ? SDL_CONTROLLER_BUTTON_RIGHTSHOULDER : SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
        }
    }

    void TouchManager::menuBack()
    {
        if (mMouseManager->usesControllerMenus())
            MWBase::Environment::get().getInputManager()->pressControllerMenuButton(SDL_CONTROLLER_BUTTON_B);
    }

    bool TouchManager::leaveConversation()
    {
        using namespace MWGui;
        MWBase::WindowManager* winMgr = MWBase::Environment::get().getWindowManager();
        const auto& modes = winMgr->getGuiModeStack();
        if (std::find(modes.begin(), modes.end(), GM_Dialogue) == modes.end())
            return false;
        // The services a conversation opens close first, then the conversation says goodbye, unless the speaker
        // waits for an answer.
        constexpr GuiMode conversation[] = { GM_Dialogue, GM_Barter, GM_Companion, GM_SpellBuying, GM_Travel,
            GM_SpellCreation, GM_Enchanting, GM_Training, GM_MerchantRepair };
        for (int i = 0; i < 5 && winMgr->isGuiMode(); ++i)
        {
            const GuiMode top = winMgr->getMode();
            if (std::find(std::begin(conversation), std::end(conversation), top) == std::end(conversation))
                break;
            winMgr->exitCurrentGuiMode();
            if (winMgr->isGuiMode() && winMgr->getMode() == top)
                break;
        }
        return true;
    }

    void TouchManager::queueAction(int action, int frames)
    {
        mPending.push_back({ action, frames });
    }

    void TouchManager::pause(bool paused)
    {
        if (paused == mPaused)
            return;
        mPaused = paused;
        MWWorld::DateTimeManager* time = MWBase::Environment::get().getWorld()->getTimeManager();
        if (paused)
            time->pause("touch");
        else
            time->unpause("touch");
    }

    void TouchManager::update(float dt)
    {
        std::vector<PendingAction> due;
        for (auto it = mPending.begin(); it != mPending.end();)
        {
            if (it->mFrames <= 0)
            {
                due.push_back(*it);
                it = mPending.erase(it);
            }
            else
            {
                --it->mFrames;
                ++it;
            }
        }
        for (const PendingAction& pending : due)
            MWBase::Environment::get().getInputManager()->executeAction(pending.mAction);

        if (mUseFrames > 0 && --mUseFrames == 0)
            pressUse(false);

        // The tutorial is for touch controls, so it waits until they're on.
        if (!Settings::input().mTouchControls)
            return;
        const bool running
            = MWBase::Environment::get().getStateManager()->getState() == MWBase::StateManager::State_Running;
        if (running && !mWasRunning)
            mTutorialCheck = true;
        mWasRunning = running;
        if (mTutorialCharacterDue)
            recordTutorialCharacter();

        MWBase::WindowManager* winMgr = MWBase::Environment::get().getWindowManager();
        const bool inventoryOpen
            = winMgr->isGuiMode() && winMgr->getMode() == MWGui::GM_Inventory && mMouseManager->usesControllerMenus();
        if (mTutorialPage == TutorialPage::Menus)
        {
            // The page closes with the inventory screens, such as when a gamepad closes them.
            if (!inventoryOpen)
            {
                mOverlay->hide();
                mTutorialPage = TutorialPage::None;
            }
            return;
        }
        if (mMenusTutorialDue && inventoryOpen && mFingers.empty())
        {
            mMenusTutorialDue = false;
            showTutorial(TutorialPage::Menus);
            return;
        }

        // A menu that opens, such as from talking to someone, ends whatever the thumbs were doing in the game.
        if (!inGame())
        {
            // A menu that a script opens over the play page, such as a message, hides it until the menu closes.
            if (mTutorialPage == TutorialPage::Play)
            {
                mTutorialPage = TutorialPage::None;
                mPlayTutorialDue = true;
            }
            stopMovement();
            pressUse(false);
            mPickerOpen = false;
            mRadialOpen = false;
            mOverlay->hide();
            pause(false);
            for (Finger& finger : mFingers)
            {
                const bool menu = finger.mRole == Role::Menu || finger.mRole == Role::Pinch
                    || (finger.mRole == Role::Edge && finger.mFallback == Role::Menu);
                if (!menu)
                    finger.mRole = Role::Ignored;
            }
            return;
        }

        if (mTutorialPage == TutorialPage::Play)
            return;
        // The play page waits until the player can move and look, such as after a new game's opening scene, whose
        // scripts and voices go on whether the game is paused or not.
        MWBase::InputManager* input = MWBase::Environment::get().getInputManager();
        if (mFingers.empty() && input->getControlSwitch("playercontrols") && input->getControlSwitch("playerlooking"))
        {
            if (mPlayTutorialDue)
            {
                mPlayTutorialDue = false;
                showTutorial(TutorialPage::Play);
                return;
            }
            if (mTutorialCheck)
                checkTutorial();
        }

        const Uint32 now = SDL_GetTicks();
        for (Finger& finger : mFingers)
        {
            if (finger.mRole == Role::Look)
            {
                keepTurning(finger, dt);
                // A right thumb that stays still in attack mode charges the attack, and can then look around while
                // it charges. One that moves first only looks.
                if (!finger.mHolding && finger.mFarthest < sSlopMm * pixelsPerMm()
                    && now - finger.mDownTime >= sChargeDelay && currentMode() == Mode_Attack)
                {
                    finger.mHolding = true;
                    pressUse(true);
                }
            }
            else if (finger.mRole == Role::QuickLoad)
            {
                const float fraction = static_cast<float>(now - finger.mDownTime) / sQuickLoadTime;
                if (fraction >= 1.f)
                {
                    finger.mRole = Role::Ignored;
                    mOverlay->hide();
                    queueAction(A_QuickLoad);
                }
                else
                    mOverlay->showProgress("#{Interface:TouchQuickLoad}", fraction);
            }
        }
    }

    void TouchManager::reset()
    {
        mFingers.clear();
        mPending.clear();
        stopMovement();
        pressUse(false);
        mPickerOpen = false;
        mRadialOpen = false;
        mOverlay->hide();
        pause(false);
        mTutorialPage = TutorialPage::None;
        mPlayTutorialDue = false;
        mMenusTutorialDue = false;
        mTutorialCheck = true;
    }

    void TouchManager::checkTutorial()
    {
        mTutorialCheck = false;
        TutorialRecord record = readTutorialRecord(mTutorialFile);
        const std::string character = currentCharacterFolder();
        const std::int64_t now = secondsNow();
        const bool away = record.mLastPlayed > 0 && now - record.mLastPlayed >= sTutorialAgain.count();
        record.mLastPlayed = now;
        if (character.empty() || !record.mCharacters.contains(character) || away)
        {
            showTutorial(TutorialPage::Play);
            if (character.empty())
                mTutorialCharacterDue = true;
            else
                record.mCharacters.insert(character);
        }
        writeTutorialRecord(mTutorialFile, record);
    }

    void TouchManager::recordTutorialCharacter()
    {
        const std::string character = currentCharacterFolder();
        if (character.empty())
            return;
        mTutorialCharacterDue = false;
        TutorialRecord record = readTutorialRecord(mTutorialFile);
        if (record.mCharacters.insert(character).second)
            writeTutorialRecord(mTutorialFile, record);
    }

    void TouchManager::showTutorial(TutorialPage page)
    {
        mTutorialPage = page;
        const MyGUI::IntSize size = screenSize();
        const float mm = pixelsPerMm();
        if (page == TutorialPage::Menus)
        {
            mOverlay->showTutorialMenus(size, mm);
            return;
        }

        // The zones are where the gestures start, as roleForGame() and edgeAt() find them.
        pause(true);
        MWGui::TouchTutorialZones zones;
        const int corner = static_cast<int>(sCornerMm * mm);
        zones.mQuickSave = MyGUI::IntCoord(0, 0, corner, corner);
        zones.mQuickLoad = MyGUI::IntCoord(size.width - corner, 0, corner, corner);
        zones.mSneak = MyGUI::IntCoord(size.width - corner, size.height - corner, corner, corner);
        const int edge = static_cast<int>(sTutorialEdgeMm * mm);
        zones.mLeftEdge = MyGUI::IntCoord(0, size.height / 4, edge, size.height / 2);
        zones.mRightEdge = MyGUI::IntCoord(size.width - edge, size.height / 4, edge, size.height / 2);
        mOverlay->showTutorialPlay(zones, size, mm);
    }

    void TouchManager::nextTutorialPage()
    {
        if (mTutorialPage == TutorialPage::None)
            return;
        mOverlay->hide();
        if (mTutorialPage == TutorialPage::Play)
        {
            pause(false);
            mMenusTutorialDue = true;
        }
        mTutorialPage = TutorialPage::None;
    }
}
