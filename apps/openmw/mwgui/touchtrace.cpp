#include "touchtrace.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#include <MyGUI_Gui.h>
#include <MyGUI_ImageBox.h>
#include <MyGUI_LayerManager.h>
#include <MyGUI_RenderManager.h>
#include <MyGUI_Widget.h>

namespace MWGui
{
    namespace
    {
        // Sizes are in millimetres on the glass, so the marks look the same on any screen, and times in milliseconds.

        // A finger that moves this far from where it went down is a drag.
        constexpr float sSlopMm = 3.f;
        // A finger held still shows a ring of this radius, which fills over this time.
        constexpr float sHoldRadiusMm = 6.f;
        constexpr float sHoldFillTime = 600.f;
        // A finger that lifts without dragging leaves a ring that grows to this radius as it fades, over this time.
        constexpr float sTapRadiusMm = 12.f;
        constexpr float sTapTime = 400.f;
        // A dragging finger has a dot of this radius, and a trail of smaller dots that fade over this time.
        constexpr float sDotRadiusMm = 3.f;
        constexpr float sTrailRadiusMm = 1.2f;
        constexpr float sTrailTime = 500.f;
        // The most dots a trail draws, so that a fast swipe doesn't make too many images.
        constexpr int sMaxTrailDots = 300;

        // The colours of the first fingers down, then the second, and so on, which Android numbers the same way.
        const std::array<MyGUI::Colour, 5> sColours = {
            MyGUI::Colour::parse("#4dd0e1"),
            MyGUI::Colour::parse("#ffb300"),
            MyGUI::Colour::parse("#ec407a"),
            MyGUI::Colour::parse("#9ccc65"),
            MyGUI::Colour::parse("#b39ddb"),
        };

        const std::string sDiscTexture = "touchtrace_disc";
        const std::string sRingTexture = "touchtrace_ring";

        // A white disc or ring on a clear square, with smooth edges, to colour as each finger's.
        void createTexture(const std::string& name, bool ring)
        {
            constexpr int size = 128;
            constexpr float outer = size / 2.f - 1.f;
            // A ring's line is a fifth of its radius.
            const float inner = ring ? outer * 0.8f : -1.f;
            MyGUI::ITexture* texture = MyGUI::RenderManager::getInstance().createTexture(name);
            texture->createManual(size, size, MyGUI::TextureUsage::Write, MyGUI::PixelFormat::R8G8B8A8);
            unsigned char* data = static_cast<unsigned char*>(texture->lock(MyGUI::TextureUsage::Write));
            for (int y = 0; y < size; ++y)
                for (int x = 0; x < size; ++x)
                {
                    const float distance = std::hypot(x + 0.5f - size / 2.f, y + 0.5f - size / 2.f);
                    const float alpha = std::clamp(outer - distance, 0.f, 1.f) * std::clamp(distance - inner, 0.f, 1.f);
                    *(data++) = 255;
                    *(data++) = 255;
                    *(data++) = 255;
                    *(data++) = static_cast<unsigned char>(alpha * 255);
                }
            texture->unlock();
        }

        float distance(const MyGUI::FloatPoint& a, const MyGUI::FloatPoint& b)
        {
            return std::hypot(a.left - b.left, a.top - b.top);
        }
    }

    TouchTrace::TouchTrace() = default;

    TouchTrace::~TouchTrace()
    {
        if (mRoot != nullptr)
            MyGUI::Gui::getInstance().destroyWidget(mRoot);
    }

    TouchTrace::Finger* TouchTrace::find(std::int64_t id)
    {
        for (Finger& finger : mFingers)
            if (finger.mId == id && finger.mDown)
                return &finger;
        return nullptr;
    }

    void TouchTrace::press(std::int64_t id, const MyGUI::FloatPoint& position, std::uint32_t time)
    {
        Finger finger;
        finger.mId = id;
        // The first colour that no finger still down has.
        while (std::any_of(mFingers.begin(), mFingers.end(),
            [&](const Finger& other) { return other.mDown && other.mColour == finger.mColour; }))
            ++finger.mColour;
        finger.mStart = position;
        finger.mDownTime = time;
        finger.mPath.push_back({ position, time });
        mFingers.push_back(finger);
    }

    void TouchTrace::move(std::int64_t id, const MyGUI::FloatPoint& position, std::uint32_t time)
    {
        Finger* finger = find(id);
        if (finger == nullptr)
            return;
        finger->mPath.push_back({ position, time });
        if (distance(finger->mStart, position) > sSlopMm * mUnitsPerMm)
            finger->mMoved = true;
    }

    void TouchTrace::release(std::int64_t id, const MyGUI::FloatPoint& position, std::uint32_t time)
    {
        Finger* finger = find(id);
        if (finger == nullptr)
            return;
        finger->mPath.push_back({ position, time });
        finger->mDown = false;
        finger->mUpTime = time;
    }

    void TouchTrace::clear()
    {
        mFingers.clear();
        if (mRoot != nullptr)
            mRoot->setVisible(false);
    }

    void TouchTrace::addCircle(
        bool ring, const MyGUI::FloatPoint& centre, float radius, const MyGUI::Colour& colour, float alpha)
    {
        if (mUsed == mImages.size())
        {
            auto* image = mRoot->createWidget<MyGUI::ImageBox>("ImageBox", MyGUI::IntCoord(), MyGUI::Align::Default);
            image->setNeedMouseFocus(false);
            image->setImageTexture(ring ? sRingTexture : sDiscTexture);
            mImages.push_back(image);
            mImageRings.push_back(ring);
        }
        MyGUI::ImageBox* image = mImages[mUsed];
        if (mImageRings[mUsed] != ring)
        {
            image->setImageTexture(ring ? sRingTexture : sDiscTexture);
            mImageRings[mUsed] = ring;
        }
        ++mUsed;
        const int size = std::max(1, static_cast<int>(std::lround(radius * 2)));
        image->setCoord(static_cast<int>(std::lround(centre.left - size / 2.f)),
            static_cast<int>(std::lround(centre.top - size / 2.f)), size, size);
        image->setColour(colour);
        image->setAlpha(std::clamp(alpha, 0.f, 1.f));
        image->setVisible(true);
    }

    void TouchTrace::draw(std::uint32_t now, float unitsPerMm)
    {
        mUnitsPerMm = unitsPerMm;
        // A finger is forgotten once all it left has faded.
        std::erase_if(mFingers, [&](const Finger& finger) {
            return !finger.mDown && now - finger.mUpTime > std::max(sTapTime, sTrailTime);
        });
        if (mRoot == nullptr)
        {
            if (mFingers.empty())
                return;
            // Over everything, videos and the mouse cursor's layer included, in GUI coordinates.
            MyGUI::ILayer* layer = MyGUI::LayerManager::getInstance().getByName("Pointer", false);
            if (layer == nullptr)
                return;
            // The render manager would look for an unknown texture name in the files, so these are made once, before
            // anything asks for them.
            static const bool created = [] {
                createTexture(sDiscTexture, false);
                createTexture(sRingTexture, true);
                return true;
            }();
            (void)created;
            mRoot = MyGUI::Gui::getInstance().createWidget<MyGUI::Widget>(
                "", MyGUI::IntCoord(MyGUI::IntPoint(), layer->getSize()), MyGUI::Align::Default, "Pointer");
            mRoot->setNeedMouseFocus(false);
        }
        mRoot->setVisible(true);
        mRoot->setSize(mRoot->getLayer()->getSize());
        mUsed = 0;

        // The trails go under the dots and rings.
        for (Finger& finger : mFingers)
        {
            std::erase_if(finger.mPath, [&](const Point& point) { return now - point.mTime > sTrailTime; });
            if (!finger.mMoved || finger.mPath.empty())
                continue;
            const MyGUI::Colour& colour = sColours[finger.mColour % sColours.size()];
            const float radius = sTrailRadiusMm * unitsPerMm;
            // Dots half their radius apart, so they read as a line.
            const float spacing = std::max(1.f, radius / 2.f);
            int dots = 0;
            const auto addDot = [&](const MyGUI::FloatPoint& at, float time) {
                const float age = static_cast<float>(now) - time;
                addCircle(false, at, radius, colour, 0.7f * (1.f - age / sTrailTime));
                ++dots;
            };
            addDot(finger.mPath.front().mPosition, static_cast<float>(finger.mPath.front().mTime));
            for (std::size_t i = 1; i < finger.mPath.size() && dots < sMaxTrailDots; ++i)
            {
                const Point& from = finger.mPath[i - 1];
                const Point& to = finger.mPath[i];
                const int steps = std::max(1, static_cast<int>(distance(from.mPosition, to.mPosition) / spacing));
                for (int step = 1; step <= steps && dots < sMaxTrailDots; ++step)
                {
                    const float t = static_cast<float>(step) / steps;
                    const MyGUI::FloatPoint at(from.mPosition.left + (to.mPosition.left - from.mPosition.left) * t,
                        from.mPosition.top + (to.mPosition.top - from.mPosition.top) * t);
                    addDot(at, from.mTime + (static_cast<float>(to.mTime) - from.mTime) * t);
                }
            }
        }

        for (const Finger& finger : mFingers)
        {
            const MyGUI::Colour& colour = sColours[finger.mColour % sColours.size()];
            const MyGUI::FloatPoint& position = finger.mPath.empty() ? finger.mStart : finger.mPath.back().mPosition;
            if (finger.mDown && finger.mMoved)
                addCircle(false, position, sDotRadiusMm * unitsPerMm, colour, 0.9f);
            else if (finger.mDown)
            {
                // A ring that fills from the middle as the finger is held.
                const float held = std::min(1.f, (now - finger.mDownTime) / sHoldFillTime);
                const float radius = sHoldRadiusMm * unitsPerMm;
                addCircle(false, position, radius * held, colour, 0.5f);
                addCircle(true, position, radius, colour, 0.9f);
            }
            else if (!finger.mMoved && now - finger.mUpTime < sTapTime)
            {
                // A ring that grows from the held ring's size as it fades.
                const float t = (now - finger.mUpTime) / sTapTime;
                const float eased = 1.f - (1.f - t) * (1.f - t);
                const float radius = (sHoldRadiusMm + (sTapRadiusMm - sHoldRadiusMm) * eased) * unitsPerMm;
                addCircle(true, position, radius, colour, 0.9f * (1.f - t));
            }
        }

        for (std::size_t i = mUsed; i < mImages.size(); ++i)
            mImages[i]->setVisible(false);
    }
}
