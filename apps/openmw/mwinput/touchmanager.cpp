#include "touchmanager.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include <SDL_timer.h>
#include <SDL_video.h>

#include <osg/Math>

#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/statemanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwgui/touchoverlay.hpp"

#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/creaturestats.hpp"
#include "../mwmechanics/drawstate.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/datetimemanager.hpp"
#include "../mwworld/player.hpp"

#include "actions.hpp"
#include "bindingsmanager.hpp"

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
        // How many frames a tap holds the attack, so the game sees it.
        constexpr int sUseFrames = 3;

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

    TouchManager::TouchManager(BindingsManager* bindingsManager, SDL_Window* window)
        : mBindingsManager(bindingsManager)
        , mWindow(window)
        , mOverlay(std::make_unique<MWGui::TouchOverlay>())
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

        if (!inGame())
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
        if (finger.mEdge != 0 && (finger.mRole == Role::Move || finger.mRole == Role::Look))
        {
            finger.mFallback = finger.mRole;
            finger.mRole = Role::Edge;
        }

        if (finger.mRole == Role::QuickLoad)
            mOverlay->showProgress("#{Interface:TouchQuickLoad}", 0.f);

        mFingers.push_back(finger);
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

        if (!inGame())
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
            default:
                break;
        }
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

        // A menu that opens, such as from talking to someone, ends whatever the thumbs were doing in the game. Fingers
        // in menus do nothing yet.
        if (!inGame())
        {
            stopMovement();
            pressUse(false);
            mPickerOpen = false;
            mRadialOpen = false;
            mOverlay->hide();
            pause(false);
            for (Finger& finger : mFingers)
                finger.mRole = Role::Ignored;
            return;
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
    }
}
