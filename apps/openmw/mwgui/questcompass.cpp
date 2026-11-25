#include "questcompass.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include <MyGUI_Gui.h>
#include <MyGUI_ImageBox.h>
#include <MyGUI_TextBox.h>

#include <osg/Math>
#include <osg/Vec2f>

#include "controllerscreen.hpp"

namespace MWGui
{
    namespace Compass
    {
        float bearing(const osg::Vec3f& from, const osg::Vec3f& to)
        {
            const float degrees = osg::RadiansToDegrees(std::atan2(to.x() - from.x(), to.y() - from.y()));
            return degrees < 0.f ? degrees + 360.f : degrees;
        }

        Place place(float direction, float heading, float halfWidth)
        {
            float offset = std::fmod(direction - heading, 360.f);
            if (offset > 180.f)
                offset -= 360.f;
            else if (offset < -180.f)
                offset += 360.f;
            if (std::abs(offset) > halfWidth)
                return Place{ offset > 0.f ? 1.f : -1.f, true };
            return Place{ offset / halfWidth, false };
        }

        Level level(float height, Level shown)
        {
            // About a storey.
            constexpr float storey = 256.f;
            constexpr float keep = 192.f;
            if (height > storey || (shown == Level::Above && height > keep))
                return Level::Above;
            if (height < -storey || (shown == Level::Below && height < -keep))
                return Level::Below;
            return Level::Same;
        }

        int markerSize(float distance)
        {
            // From ten cells away, which is farther than from Seyda Neen to Balmora, to one cell away, a marker grows
            // slowly, so that markers in the same direction at different distances, such as two towns, have
            // different sizes. In the last cell it grows faster, to show the player is nearly there. It is largest
            // within about seven metres.
            constexpr float cell = 8192.f;
            constexpr float far = 10 * cell;
            constexpr float near = 512.f;
            constexpr float smallest = 8.f;
            constexpr float atOneCell = 20.f;
            constexpr float largest = 26.f;
            float size = 0.f;
            if (distance >= cell)
                size = smallest + std::clamp((far - distance) / (far - cell), 0.f, 1.f) * (atOneCell - smallest);
            else
                size = atOneCell + std::clamp((cell - distance) / (cell - near), 0.f, 1.f) * (largest - atOneCell);
            return static_cast<int>(std::round(size));
        }
    }

    namespace
    {
        constexpr int sHeight = 38;
        constexpr int sArrowSize = 10;
        constexpr int sLabelWidth = 32;
        // The compass shows this many degrees each side of the way the camera faces.
        constexpr float sHalfWidth = 90.f;
        constexpr const char* sMarkerTexture = "textures\\omw\\quest_marker.png";

        struct Label
        {
            const char* mText;
            float mDirection;
            bool mMain;
        };
        constexpr Label sLabels[]
            = { { "N", 0.f, true }, { "NE", 45.f, false }, { "E", 90.f, true }, { "SE", 135.f, false },
                  { "S", 180.f, true }, { "SW", 225.f, false }, { "W", 270.f, true }, { "NW", 315.f, false } };
        constexpr float sTickStep = 15.f;
    }

    QuestCompass::QuestCompass(MyGUI::Widget* parent)
        : mWidth(std::min(440, parent->getWidth() - 40))
    {
        using namespace ControllerUi;
        mRoot = parent->createWidget<MyGUI::Widget>({},
            MyGUI::IntCoord((parent->getWidth() - mWidth) / 2, 10, mWidth, sHeight),
            MyGUI::Align::HCenter | MyGUI::Align::Top);
        mRoot->setNeedMouseFocus(false);
        addRect(mRoot, 0, 0, mWidth, sHeight, sPanelColour, 0.55f);
        addRect(mRoot, 0, sHeight - 1, mWidth, 1, sAccentColour, 0.6f);
        // The middle of the compass, where the camera faces.
        addRect(mRoot, mWidth / 2, 0, 1, 5, sBrightColour, 0.9f);

        for (float direction = 0.f; direction < 360.f; direction += sTickStep)
            mTicks.push_back(addRect(mRoot, 0, sHeight - 7, 1, 6, sDimColour, 0.8f));
        for (const Label& label : sLabels)
            mLabels.push_back(
                addText(mRoot, 0, 0, sLabelWidth, sHeight - 4, label.mText, label.mMain ? sFontNormal : sFontSmall,
                    label.mMain ? sBrightColour : sDimColour, MyGUI::Align::Center));
        for (std::size_t i = 0; i < mRoot->getChildCount(); ++i)
            mRoot->getChildAt(i)->setNeedMouseFocus(false);
    }

    QuestCompass::~QuestCompass() = default;

    void QuestCompass::setVisible(bool visible)
    {
        mRoot->setVisible(visible);
    }

    void QuestCompass::update(float heading, const osg::Vec3f& playerPos, const std::vector<osg::Vec3f>& targets)
    {
        const auto xOf = [&](float position) { return static_cast<int>((position + 1.f) * 0.5f * mWidth); };

        for (std::size_t i = 0; i < mTicks.size(); ++i)
        {
            const Compass::Place where = Compass::place(i * sTickStep, heading, sHalfWidth);
            mTicks[i]->setVisible(!where.mPinned);
            mTicks[i]->setPosition(xOf(where.mPosition), mTicks[i]->getTop());
        }
        for (std::size_t i = 0; i < mLabels.size(); ++i)
        {
            const Compass::Place where = Compass::place(sLabels[i].mDirection, heading, sHalfWidth);
            // The labels fade out towards the ends.
            mLabels[i]->setVisible(!where.mPinned && std::abs(where.mPosition) < 0.95f);
            mLabels[i]->setAlpha(std::clamp(1.4f - std::abs(where.mPosition), 0.f, 1.f));
            mLabels[i]->setPosition(xOf(where.mPosition) - sLabelWidth / 2, mLabels[i]->getTop());
        }

        while (mMarkers.size() < targets.size())
        {
            MyGUI::ImageBox* marker = mRoot->createWidget<MyGUI::ImageBox>("ImageBox",
                MyGUI::IntCoord(0, 0, 0, 0), MyGUI::Align::Default);
            marker->setImageTexture(sMarkerTexture);
            marker->setNeedMouseFocus(false);
            mMarkers.push_back(marker);
            MyGUI::ImageBox* arrow = mRoot->createWidget<MyGUI::ImageBox>("ImageBox",
                MyGUI::IntCoord(0, (sHeight - sArrowSize) / 2, sArrowSize, sArrowSize), MyGUI::Align::Default);
            arrow->setNeedMouseFocus(false);
            mArrows.push_back(arrow);
            mLevels.push_back(Compass::Level::Same);
        }
        for (std::size_t i = 0; i < mMarkers.size(); ++i)
        {
            if (i >= targets.size())
            {
                mMarkers[i]->setVisible(false);
                mArrows[i]->setVisible(false);
                mLevels[i] = Compass::Level::Same;
                continue;
            }
            const Compass::Place where = Compass::place(Compass::bearing(playerPos, targets[i]), heading, sHalfWidth);
            mLevels[i] = Compass::level(targets[i].z() - playerPos.z(), mLevels[i]);
            const bool above = mLevels[i] == Compass::Level::Above;
            const bool below = mLevels[i] == Compass::Level::Below;
            const osg::Vec3f offset = targets[i] - playerPos;
            const int size = Compass::markerSize(osg::Vec2f(offset.x(), offset.y()).length());
            const int x = std::clamp(xOf(where.mPosition), size / 2, mWidth - size / 2);
            // The arrow sits right above or below its marker, so the two read as one sign rather than two markers.
            int markerY = (sHeight - size) / 2;
            if (above)
                markerY = sHeight - size - 1;
            else if (below)
                markerY = 1;
            const float alpha = where.mPinned ? 0.6f : 1.f;
            mMarkers[i]->setVisible(true);
            mMarkers[i]->setAlpha(alpha);
            mMarkers[i]->setCoord(x - size / 2, markerY, size, size);
            mArrows[i]->setVisible(above || below);
            if (above || below)
            {
                mArrows[i]->setImageTexture(
                    above ? "textures\\omw\\quest_arrow_up.png" : "textures\\omw\\quest_arrow_down.png");
                mArrows[i]->setAlpha(alpha);
                mArrows[i]->setPosition(x - sArrowSize / 2, above ? markerY - sArrowSize : markerY + size);
            }
        }
    }
}
