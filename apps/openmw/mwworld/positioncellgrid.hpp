#ifndef OPENMW_APPS_OPENMW_MWWORLD_POSITIONCELLGRID_H
#define OPENMW_APPS_OPENMW_MWWORLD_POSITIONCELLGRID_H

#include <vector>

#include <osg/Vec3f>
#include <osg/Vec4i>

#include <components/esm/refid.hpp>

namespace MWWorld
{
    struct PositionCellGrid
    {
        osg::Vec3f mPosition;
        osg::Vec4i mCellBounds;
    };

    /// Positions closer than this share one preload.
    constexpr float terrainPreloadMergeDistance = 128.f;

    /// @return Primary and detached camera positions.
    std::vector<PositionCellGrid> terrainPreloadPositions(const osg::Vec3f& primary, const osg::Vec3f& playerPos,
        const osg::Vec3f& cameraPos, bool cameraDetached, const osg::Vec4i& grid, ESM::RefId worldspace);

    /// Adds a place where the player may arrive to the terrain preload positions, unless one already there covers it.
    /// A preload covers an arrival less than a cell away from its position with the same grid of cells, as
    /// CellPreloader::isTerrainLoaded checks. Each position costs a preload of its own, and several doors often lead to
    /// the same place, such as a tavern's front and back doors.
    void addTerrainPreloadPosition(std::vector<PositionCellGrid>& positions, const PositionCellGrid& position);
}

#endif
