#ifndef MWGUI_TOUCHTRACE_H
#define MWGUI_TOUCHTRACE_H

#include <cstdint>
#include <vector>

#include <MyGUI_Colour.h>
#include <MyGUI_Types.h>

namespace MyGUI
{
    class ImageBox;
    class Widget;
}

namespace MWGui
{
    /// Draws where fingers touch the screen, over everything else, so that a screen recording shows the touch controls
    /// being used. A tap leaves a ring that grows and fades where the finger lifted. A drag leaves a trail that fades,
    /// with a dot where the finger is. A finger held still fills a ring. Each finger has its own colour. The
    /// "show touches" setting turns it on. See ui-rework/design/touch.md.
    class TouchTrace
    {
    public:
        TouchTrace();
        ~TouchTrace();

        /// Fingers, with their positions in GUI coordinates and times in milliseconds, as SDL gives them.
        void press(std::int64_t id, const MyGUI::FloatPoint& position, std::uint32_t time);
        void move(std::int64_t id, const MyGUI::FloatPoint& position, std::uint32_t time);
        void release(std::int64_t id, const MyGUI::FloatPoint& position, std::uint32_t time);

        /// Draws the fingers as they are now. unitsPerMm is how many GUI units make a millimetre on the screen.
        void draw(std::uint32_t now, float unitsPerMm);

        /// Forgets every finger and hides what's drawn.
        void clear();

    private:
        struct Point
        {
            MyGUI::FloatPoint mPosition;
            std::uint32_t mTime;
        };

        struct Finger
        {
            std::int64_t mId = 0;
            int mColour = 0;
            bool mDown = true;
            /// Whether the finger has moved far enough to be a drag rather than a tap or a hold.
            bool mMoved = false;
            MyGUI::FloatPoint mStart;
            std::uint32_t mDownTime = 0;
            std::uint32_t mUpTime = 0;
            /// Where the finger has been, oldest first, for its trail.
            std::vector<Point> mPath;
        };

        Finger* find(std::int64_t id);
        /// Draws a disc or a ring, by its centre and radius in GUI units, from the pool of images.
        void addCircle(
            bool ring, const MyGUI::FloatPoint& centre, float radius, const MyGUI::Colour& colour, float alpha);

        MyGUI::Widget* mRoot = nullptr;
        std::vector<MyGUI::ImageBox*> mImages;
        /// Whether each image shows the ring rather than the disc.
        std::vector<bool> mImageRings;
        std::size_t mUsed = 0;
        std::vector<Finger> mFingers;
        float mUnitsPerMm = 1.f;
    };
}

#endif
