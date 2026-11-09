#ifndef COMPONENTS_TERRAIN_QUADTREEWORLD_H
#define COMPONENTS_TERRAIN_QUADTREEWORLD_H

#include "terraingrid.hpp"

#include <atomic>
#include <memory>
#include <mutex>

#include <osg/Referenced>
#include <osg/Vec3f>

#include <optional>

#include <components/esm/refid.hpp>

namespace osg
{
    class NodeVisitor;
    class Group;
    class Stats;
}

namespace osg
{
    class Camera;
}

namespace Terrain
{
    /// Lets a camera choose how far the terrain and the distant objects reach, in place of the view distance setting,
    /// and how detailed they are. Set it as the camera's user data. Such a camera's choice of detail is kept apart from
    /// other cameras', so it doesn't change what the game's own view draws.
    class CameraDetail : public osg::Referenced
    {
    public:
        float mViewDistance = 0.f;
        /// When above zero, everything in view gets the detail it would have at this distance from the camera,
        /// however near or far it is. Then the detail doesn't change as the camera moves, and nothing pops in or out.
        float mDetailDistance = 0.f;
        /// When set, the terrain picks its pieces around this point instead of the camera's position. A camera can
        /// keep drawing the pieces it has while QuadTreeWorld::preloadForCamera builds the ones for a new point in the
        /// background, then move the point there.
        std::optional<osg::Vec3f> mSelectionPoint;

        /// Returns the choices a camera makes, or nullptr when it makes none.
        static const CameraDetail* get(const osg::Camera* camera);
    };

    class RootNode;
    class ViewDataMap;
    class ViewData;
    struct ViewDataEntry;

    class DebugChunkManager;

    /// @brief Terrain implementation that loads cells into a Quad Tree, with geometry LOD and texture LOD.
    class QuadTreeWorld
        : public TerrainGrid // note: derived from TerrainGrid is only to render default cells (see loadCell)
    {
    public:
        QuadTreeWorld(osg::Group* parent, osg::Group* compileRoot, Resource::ResourceSystem* resourceSystem,
            Storage* storage, unsigned int nodeMask, unsigned int preCompileMask, unsigned int borderMask,
            int compMapResolution, float comMapLevel, float lodFactor, int vertexLodMod, float maxCompGeometrySize,
            bool debugChunks, ESM::RefId worldspace, double expiryDelay);

        ~QuadTreeWorld();

        void accept(osg::NodeVisitor& nv);

        void enable(bool enabled) override;

        void setViewDistance(float distance) override;

        void cacheCell(View* view, int x, int y) override {}
        /// @note Not thread safe.
        void loadCell(int x, int y) override;
        /// @note Not thread safe.
        void unloadCell(int x, int y) override;

        std::unique_ptr<View> createView() override;
        void preload(View* view, const osg::Vec3f& eyePoint, const osg::Vec4i& cellgrid, std::atomic<bool>& abort,
            Loading::Reporter& reporter) override;

        /// Builds the pieces that a camera with its own detail would draw from a point, so that drawing them later
        /// finds them ready. Safe to call from a background thread. The view comes from createView(), and the grid is
        /// getActiveGrid() at the time.
        void preloadForCamera(View* view, const osg::Vec3f& viewPoint, const osg::Vec4i& grid, float viewDistance,
            float detailDistance, std::atomic<bool>& abort);
        void rebuildViews() override;

        void reportStats(unsigned int frameNumber, osg::Stats* stats) override;

        class ChunkManager
        {
        public:
            virtual ~ChunkManager() = default;
            ChunkManager() = default;
            ChunkManager(ESM::RefId worldspace)
                : ChunkManager()
            {
                mWorldspace = worldspace;
            }
            virtual osg::ref_ptr<osg::Node> getChunk(float size, const osg::Vec2f& center, unsigned char lod,
                unsigned int lodFlags, bool activeGrid, const osg::Vec3f& viewPoint, bool compile)
                = 0;
            virtual unsigned int getNodeMask() { return 0; }

            void setViewDistance(float viewDistance) { mViewDistance = viewDistance; }
            float getViewDistance() const { return mViewDistance; }

            // Automatically set by addChunkManager based on getViewDistance()
            unsigned int getMaxLodLevel() const { return mMaxLodLevel; }
            void setMaxLodLevel(unsigned int level) { mMaxLodLevel = level; }

        protected:
            ESM::RefId mWorldspace = ESM::RefId();

        private:
            float mViewDistance = 0.f;
            unsigned int mMaxLodLevel = ~0u;
        };
        void addChunkManager(ChunkManager*);

    private:
        void ensureQuadTreeBuilt();
        void loadRenderingNode(ViewDataEntry& entry, ViewData* vd, float cellWorldSize, const osg::Vec4i& gridbounds,
            bool compile, float detailDistance = 0.f);
        void preloadView(View* view, const osg::Vec3f& viewPoint, const osg::Vec4i& grid, float viewDistance,
            float detailDistance, std::atomic<bool>& abort, Loading::Reporter* reporter);

        osg::ref_ptr<RootNode> mRootNode;

        std::unique_ptr<ViewDataMap> mViewDataMap;

        std::vector<ChunkManager*> mChunkManagers;

        std::mutex mQuadTreeMutex;
        bool mQuadTreeBuilt;
        float mLodFactor;
        int mVertexLodMod;
        float mViewDistance;
        float mMinSize;
        bool mDebugTerrainChunks;
        std::unique_ptr<DebugChunkManager> mDebugChunkManager;
    };

}

#endif
