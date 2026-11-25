#include "touchgestures.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace MWInput::TouchGestures
{
    float pixelsPerMm(float dpi, int drawableWidth, int windowWidth)
    {
        if (dpi <= 0)
            dpi = 160;
        const float pixelsPerPoint = windowWidth > 0 ? static_cast<float>(drawableWidth) / windowWidth : 1.f;
        return dpi / 25.4f * pixelsPerPoint;
    }

    Zone zoneAt(const MyGUI::FloatPoint& point, const MyGUI::IntSize& screen, float pixelsPerMm)
    {
        const float corner = sCornerMm * pixelsPerMm;
        const bool top = point.top < corner;
        const bool left = point.left < corner;
        const bool right = point.left > screen.width - corner;
        if (top && left)
            return Zone::QuickSave;
        if (top && right)
            return Zone::QuickLoad;
        if (right && point.top > screen.height - corner)
            return Zone::Sneak;
        return point.left < screen.width / 2.f ? Zone::Move : Zone::Look;
    }

    int edgeAt(const MyGUI::FloatPoint& point, const MyGUI::IntSize& screen, float pixelsPerMm)
    {
        if (point.top < screen.height / 4.f || point.top > screen.height * 3 / 4.f)
            return 0;
        const float edge = sEdgeMm * pixelsPerMm;
        if (point.left < edge)
            return -1;
        if (point.left > screen.width - edge)
            return 1;
        return 0;
    }

    EdgeSwipe edgeSwipe(
        const MyGUI::FloatPoint& start, const MyGUI::FloatPoint& position, int edge, float farthest, float pixelsPerMm)
    {
        if (farthest < sSlopMm * pixelsPerMm)
            return EdgeSwipe::Undecided;
        const float inwards = (position.left - start.left) * -edge;
        const float across = std::abs(position.top - start.top);
        if (inwards > across && inwards < sEdgeSwipeMm * pixelsPerMm)
            return EdgeSwipe::Undecided;
        if (inwards <= across)
            return EdgeSwipe::NotASwipe;
        return EdgeSwipe::SwipedIn;
    }

    bool isTap(std::uint32_t downTime, std::uint32_t upTime, float farthest, float pixelsPerMm)
    {
        return upTime - downTime <= sTapTime && farthest < sSlopMm * pixelsPerMm;
    }

    bool isFlickUp(const MyGUI::FloatPoint& start, const MyGUI::FloatPoint& end, std::uint32_t downTime,
        std::uint32_t upTime, float pixelsPerMm)
    {
        const float up = start.top - end.top;
        const float sideways = std::abs(end.left - start.left);
        return upTime - downTime <= sFlickTime && up >= sFlickMm * pixelsPerMm && up > sideways;
    }

    Movement movement(const MyGUI::FloatPoint& start, const MyGUI::FloatPoint& position, float pixelsPerMm)
    {
        const float right = (position.left - start.left) / pixelsPerMm;
        const float forward = (start.top - position.top) / pixelsPerMm;
        const float length = std::hypot(right, forward);
        const float amount = std::clamp((length - sMoveDeadZoneMm) / (sMoveFullMm - sMoveDeadZoneMm), 0.f, 1.f)
            / std::max(length, 1e-3f);
        return { 0.5f - forward * amount / 2.f, 0.5f + right * amount / 2.f };
    }

    Turn lookTurn(const MyGUI::FloatPoint& start, const MyGUI::FloatPoint& previous, const MyGUI::FloatPoint& position,
        float pixelsPerMm)
    {
        const float circle = sLookCircleMm * pixelsPerMm;
        const auto inside = [&](float x) { return std::clamp(x - start.left, -circle, circle); };
        return { (inside(position.left) - inside(previous.left)) / pixelsPerMm * sLookRadiansPerMm,
            (position.top - previous.top) / pixelsPerMm * sLookRadiansPerMm };
    }

    float turnSpeed(float startX, float currentX, float pixelsPerMm)
    {
        const float past = std::abs(currentX - startX) / pixelsPerMm - sLookCircleMm;
        if (past <= 0)
            return 0;
        const float speed = sLookFullSpeed * std::min(1.f, past / sLookRampMm);
        return std::copysign(speed, currentX - startX);
    }

    int tabsSwiped(float startX, float currentX, float pixelsPerMm)
    {
        return static_cast<int>((currentX - startX) / (sTabSwipeMm * pixelsPerMm));
    }

    int scrollSteps(float startY, float currentY, float pixelsPerMm)
    {
        return static_cast<int>((startY - currentY) / (sScrollStepMm * pixelsPerMm));
    }

    int pickerItem(const MyGUI::FloatPoint& origin, const MyGUI::FloatPoint& position, float pixelsPerMm)
    {
        const float dx = (position.left - origin.left) / (sPickerAcrossMm * pixelsPerMm);
        const float dy = (position.top - origin.top) / (sPickerUpDownMm * pixelsPerMm);
        int nearestItem = 0;
        float nearest = std::numeric_limits<float>::max();
        for (int i = 0; i < sPickerItems; ++i)
        {
            const float d = std::hypot(dx - sPickerOffsets[i][0], dy - sPickerOffsets[i][1]);
            if (d < nearest)
            {
                nearest = d;
                nearestItem = i;
            }
        }
        return nearestItem;
    }
}
