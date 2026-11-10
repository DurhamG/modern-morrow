#ifndef OPENMW_MWRENDER_MAP3D_H
#define OPENMW_MWRENDER_MAP3D_H

#include <memory>
#include <optional>

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

namespace SceneUtil
{
    class WorkQueue;
}

namespace Terrain
{
    class QuadTreeWorld;
    class View;
    class World;
}

namespace MWRender
{
    class Map3DRenderToTexture;
    class Map3DFogPass;
    class Map3DPreload;

    /// Draws the world around a point on the ground, seen from above at an angle, into a texture. It draws every frame
    /// while it is shown. It draws the same scene as the game's own view. Cells far from the player show the way
    /// distant land does: terrain, and merged copies of buildings and other objects that don't move.
    ///
    /// With distant terrain on, the pieces of land for a new place or zoom are built on a background thread. Until
    /// they are ready, the view keeps drawing the pieces it has, so moving the view doesn't stall the game.
    class Map3D
    {
    public:
        /// The texture is width by height pixels.
        Map3D(osg::Group* root, Terrain::World* terrain, std::shared_ptr<SceneUtil::WorkQueue> workQueue, int width,
            int height);
        ~Map3D();

        /// Starts or stops drawing.
        void setShown(bool shown);

        /// Looks at a point on the ground from a distance, in world units.
        void setView(const osg::Vec3f& focus, float distance);

        /// Hides the parts of the view that the player hasn't explored. The image has one channel, 255 where
        /// unexplored and 0 where explored. A place's texture coordinates come from its world position x, y as
        /// dot(toU, (x, y, 1)) and dot(toV, (x, y, 1)). Places outside the image count as unexplored. With nullptr,
        /// everything shows.
        void setFog(osg::Image* image, const osg::Vec3f& toU, const osg::Vec3f& toV);

        /// Indoors, the view cuts away everything above a height, so the rooms show from above, and builds no distant
        /// land. Without a height, the view is outdoors.
        void setIndoors(std::optional<float> ceiling);

        int getWidth() const { return mWidth; }
        int getHeight() const { return mHeight; }

        /// Makes a texture for a MyGUI widget. The picture is upside down, so the widget flips it.
        std::unique_ptr<MyGUI::ITexture> makeGuiTexture() const;

        /// Returns where a world position shows in the picture, from 0 to 1 across and down. Returns false when it is
        /// behind the camera.
        bool worldToPicture(const osg::Vec3f& position, osg::Vec2f& picture) const;

        /// Returns where a point of the picture, from 0 to 1 across and down, meets level ground at a height. Returns
        /// false when the point looks at the sky.
        bool pictureToGround(const osg::Vec2f& picture, float height, osg::Vec3f& ground) const;

    private:
        /// Starts building the pieces for where the view is now, and switches to them once they are built.
        void updateTerrainPieces();

        osg::ref_ptr<osg::Group> mRoot;
        osg::ref_ptr<Map3DRenderToTexture> mRtt;
        osg::ref_ptr<Map3DFogPass> mFogPass;
        // The distant terrain, or nullptr when it is off and the view draws only the loaded cells.
        Terrain::QuadTreeWorld* mTerrain = nullptr;
        std::shared_ptr<SceneUtil::WorkQueue> mWorkQueue;
        // The background thread's own record of which pieces it chose.
        std::unique_ptr<Terrain::View> mPreloadView;
        std::shared_ptr<Map3DPreload> mPreload;
        int mWidth;
        int mHeight;
        bool mIndoors = false;
    };
}

#endif
