#ifndef MWGUI_QUESTCOMPASS_H
#define MWGUI_QUESTCOMPASS_H

#include <vector>

#include <osg/Vec3f>

namespace MyGUI
{
    class Widget;
    class ImageBox;
    class TextBox;
}

namespace MWGui
{
    namespace Compass
    {
        /// Where a direction shows on a compass.
        struct Place
        {
            /// From -1 at the compass's left end to 1 at its right end.
            float mPosition = 0.f;
            /// Whether the direction is past an end, and shows at that end instead.
            bool mPinned = false;
        };

        /// The direction from one point to another, in degrees clockwise from north, which is +y.
        float bearing(const osg::Vec3f& from, const osg::Vec3f& to);

        /// Where a direction shows on a compass that spans halfWidth degrees each side of the heading. The direction
        /// and heading are in degrees clockwise from north.
        Place place(float direction, float heading, float halfWidth);

        /// Whether a target is above or below the player, enough for the compass to show an arrow with its marker.
        enum class Level
        {
            Same,
            Above,
            Below,
        };

        /// Whether a target that is height units higher than the player, or lower when height is negative, is above
        /// or below them. A target is above or below once it is more than a storey away. It stays so until it is well
        /// within a storey, given what the compass shows now, so that walking on uneven ground doesn't make the arrow
        /// come and go. A target more than a cell away across the ground is never above or below, because the arrow
        /// would only be noise that far away.
        Level level(float height, float distance, Level shown);

        /// The size in pixels of a marker for a target this far from the player, ignoring height. A marker grows as
        /// the player gets closer: it is smallest ten cells or more away, grows slowly to one cell away, and grows faster in
        /// the last cell, to be largest close by.
        int markerSize(float distance);
    }

    /// A compass along the top of the HUD, as in Skyrim. It shows north, east, south and west, and a marker for each
    /// tracked quest's target, or the door on the way to it. A marker past the compass's ends stays at the nearer end,
    /// and an arrow right above or below it says whether the target is above or below. A marker grows as the player
    /// gets closer to its target.
    class QuestCompass
    {
    public:
        explicit QuestCompass(MyGUI::Widget* parent);
        ~QuestCompass();

        /// Points the compass the way the camera faces, in degrees clockwise from north, and places the markers.
        void update(float heading, const osg::Vec3f& playerPos, const std::vector<osg::Vec3f>& targets);

        void setVisible(bool visible);

    private:
        MyGUI::Widget* mRoot;
        int mWidth;
        std::vector<MyGUI::TextBox*> mLabels;
        std::vector<MyGUI::ImageBox*> mTicks;
        std::vector<MyGUI::ImageBox*> mMarkers;
        std::vector<MyGUI::ImageBox*> mArrows;
        std::vector<Compass::Level> mLevels;
    };
}

#endif
