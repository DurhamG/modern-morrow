#include "positioncellgrid.hpp"

#include <algorithm>
#include <cmath>

#include <components/esm/util.hpp>
#include <components/misc/constants.hpp>

namespace MWWorld
{
    std::vector<PositionCellGrid> terrainPreloadPositions(const osg::Vec3f& primary, const osg::Vec3f& playerPos,
        const osg::Vec3f& cameraPos, bool cameraDetached, const osg::Vec4i& grid, ESM::RefId worldspace)
    {
        std::vector<PositionCellGrid> positions{ PositionCellGrid{ primary, grid } };
        if (!cameraDetached)
            return positions;
        // Camera distance bounds terrain LOD preloads.
        const float distance = (cameraPos - playerPos).length();
        if (distance >= terrainPreloadMergeDistance && distance < static_cast<float>(ESM::getCellSize(worldspace)))
        {
            // Snapping keeps orbit preloads from restarting.
            constexpr float quantum = terrainPreloadMergeDistance;
            const osg::Vec3f snapped(std::floor(cameraPos.x() / quantum) * quantum,
                std::floor(cameraPos.y() / quantum) * quantum, std::floor(cameraPos.z() / quantum) * quantum);
            positions.push_back(PositionCellGrid{ snapped, grid });
        }
        return positions;
    }

    void addTerrainPreloadPosition(std::vector<PositionCellGrid>& positions, const PositionCellGrid& position)
    {
        constexpr float tolerance = Constants::CellSizeInUnits;
        const bool covered = std::ranges::any_of(positions, [&](const PositionCellGrid& existing) {
            return existing.mCellBounds == position.mCellBounds
                && (existing.mPosition - position.mPosition).length2() < tolerance * tolerance;
        });
        if (!covered)
            positions.push_back(position);
    }
}
