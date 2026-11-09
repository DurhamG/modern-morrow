#ifndef OPENMW_MWRENDER_MAP3D_H
#define OPENMW_MWRENDER_MAP3D_H

#include <memory>

#include <osg/Matrix>
#include <osg/Vec2f>
#include <osg/Vec3f>
#include <osg/ref_ptr>

namespace osg
{
    class Group;
    class Image;
    class Node;
}

namespace MyGUI
{
    class ITexture;
}

namespace MWRender
{
    class Map3DRenderToTexture;
    class Map3DFogPass;

    /// Draws the world around a point on the ground, seen from above at an angle, into a texture. It draws every frame
    /// while it is shown. It draws the same scene as the game's own view. Cells far from the player show the way
    /// distant land does: terrain, and merged copies of buildings and other objects that don't move.
    class Map3D
    {
    public:
        /// The texture is width by height pixels.
        Map3D(osg::Group* root, int width, int height);
        ~Map3D();

        /// Starts or stops drawing.
        void setShown(bool shown);

        /// Looks at a point on the ground from a distance, in world units.
        void setView(const osg::Vec3f& focus, float distance);

        /// Darkens the parts of the view that the player hasn't explored. The image has one channel, 255 where
        /// unexplored and 0 where explored. It covers a square of the world, size units across, with its first row and
        /// column at the south west corner. Places outside it count as unexplored. With nullptr, nothing is darkened.
        void setFog(osg::Image* image, const osg::Vec2f& southWest, float size);

        int getWidth() const { return mWidth; }
        int getHeight() const { return mHeight; }

        /// Makes a texture for a MyGUI widget. The picture is upside down, so the widget flips it.
        std::unique_ptr<MyGUI::ITexture> makeGuiTexture() const;

        /// Returns where a world position shows in the picture, from 0 to 1 across and down. Returns false when it is
        /// behind the camera.
        bool worldToPicture(const osg::Vec3f& position, osg::Vec2f& picture) const;

    private:
        osg::ref_ptr<osg::Group> mRoot;
        osg::ref_ptr<Map3DRenderToTexture> mRtt;
        osg::ref_ptr<Map3DFogPass> mFogPass;
        int mWidth;
        int mHeight;
    };
}

#endif
