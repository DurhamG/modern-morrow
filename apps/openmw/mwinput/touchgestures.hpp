#ifndef MWINPUT_TOUCHGESTURES_H
#define MWINPUT_TOUCHGESTURES_H

#include <cstdint>

#include <MyGUI_Types.h>

namespace MWInput::TouchGestures
{
    // Starting values, to tune on a phone. Distances are in millimetres on the glass, so screens of any size and
    // density feel the same. See ui-rework/design/touch.md.

    // A tap lifts within this time, in milliseconds, having moved less than the slop.
    constexpr std::uint32_t sTapTime = 250;
    constexpr float sSlopMm = 3.f;
    // Walking starts past the dead zone and is at full speed this far from where the thumb went down, in any
    // direction, like a stick. Half way there, the player runs.
    constexpr float sMoveDeadZoneMm = 2.f;
    constexpr float sMoveFullMm = 15.f;
    // How far the camera turns for each millimetre the right thumb drags, in radians, before the camera sensitivity
    // setting. That holds within a circle around where the thumb went down. Past its edge to either side, the camera
    // keeps turning that way, like a stick: from standing still at the edge, so there's no jump in speed there, rising
    // evenly to the full speed a ramp's length farther out, in radians a second.
    constexpr float sLookRadiansPerMm = 0.026f;
    constexpr float sLookCircleMm = 10.f;
    constexpr float sLookRampMm = 20.f;
    constexpr float sLookFullSpeed = 130.f * 3.14159265358979323846f / 180.f;
    // A finger that goes down this close to a side edge, in the middle half of the edge's height, swipes in from the
    // edge once it has moved this far inwards, more in than up or down. GameActivity keeps Android's back gesture off
    // the same part of the edges.
    constexpr float sEdgeMm = 5.f;
    constexpr float sEdgeSwipeMm = 8.f;
    // The corners' squares.
    constexpr float sCornerMm = 12.f;
    // How far apart the mode picker's items' centres are, across and up and down. Use is under the thumb, attack and
    // then the quick keys to its left, magic above and jump below, in the order of the modes.
    constexpr float sPickerAcrossMm = 19.f;
    constexpr float sPickerUpDownMm = 13.f;
    constexpr float sPickerOffsets[][2] = { { 0, 0 }, { -1, 0 }, { -2, 0 }, { 0, -1 }, { 0, 1 } };
    constexpr int sPickerItems = 5;
    // A drag of this length in a menu scrolls a list by one row.
    constexpr float sScrollStepMm = 6.f;
    // A drag in a menu that is more sideways than up or down switches tabs, one for each time it goes this far, and
    // back again if it comes back.
    constexpr float sTabSwipeMm = 12.f;

    /// How many of the screen's pixels make a millimetre. The DPI is for window coordinates, which can be smaller
    /// than the drawable's pixels. An unknown DPI, of 0 or less, counts as 160.
    float pixelsPerMm(float dpi, int drawableWidth, int windowWidth);

    /// What a thumb that goes down at a point during play does.
    enum class Zone
    {
        Move,
        Look,
        QuickSave,
        QuickLoad,
        Sneak,
    };

    /// The top left corner quicksaves, the top right corner quickloads and the bottom right corner sneaks. Otherwise
    /// the left half of the screen moves and the right half looks.
    Zone zoneAt(const MyGUI::FloatPoint& point, const MyGUI::IntSize& screen, float pixelsPerMm);

    /// The side edge a finger that goes down at a point can swipe in from: -1 for the left, 1 for the right, or 0.
    int edgeAt(const MyGUI::FloatPoint& point, const MyGUI::IntSize& screen, float pixelsPerMm);

    enum class EdgeSwipe
    {
        Undecided,
        NotASwipe,
        SwipedIn,
    };

    /// Whether a finger that went down by an edge has swiped in from it. Farthest is the farthest it has been from
    /// where it went down, in pixels.
    EdgeSwipe edgeSwipe(
        const MyGUI::FloatPoint& start, const MyGUI::FloatPoint& position, int edge, float farthest, float pixelsPerMm);

    /// Whether a finger that went down and lifted at these times, in milliseconds, is a tap.
    bool isTap(std::uint32_t downTime, std::uint32_t upTime, float farthest, float pixelsPerMm);

    /// The movement actions' values for the left thumb. They run from 0 for full forward or left to 1 for full back
    /// or right, as a stick's do, with 0.5 for still. Up the screen is forward.
    struct Movement
    {
        float mForwardBackward = 0.5f;
        float mLeftRight = 0.5f;
    };
    Movement movement(const MyGUI::FloatPoint& start, const MyGUI::FloatPoint& position, float pixelsPerMm);

    /// How far the camera turns, in radians, for the right thumb's move from previous to position. Only the part of a
    /// sideways move inside the circle around where it went down turns the camera directly.
    struct Turn
    {
        float mYaw = 0;
        float mPitch = 0;
    };
    Turn lookTurn(const MyGUI::FloatPoint& start, const MyGUI::FloatPoint& previous, const MyGUI::FloatPoint& position,
        float pixelsPerMm);

    /// How fast the camera keeps turning, in radians a second, with the right thumb past the circle's edge. Its sign
    /// is the side the thumb is on. It is 0 inside the circle.
    float turnSpeed(float startX, float currentX, float pixelsPerMm);

    /// How many tabs a sideways drag in a menu has moved, to the right when positive.
    int tabsSwiped(float startX, float currentX, float pixelsPerMm);

    /// How many rows a drag in a menu has scrolled. Dragging up is positive, which shows the rows below.
    int scrollSteps(float startY, float currentY, float pixelsPerMm);

    /// The mode picker's item nearest to how far the thumb has moved since the picker opened, so that the first
    /// item, Use, is under the thumb then.
    int pickerItem(const MyGUI::FloatPoint& origin, const MyGUI::FloatPoint& position, float pixelsPerMm);
}

#endif
