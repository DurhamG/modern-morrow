#ifndef OPENMW_MWRENDER_LINEMEETSLAND_H
#define OPENMW_MWRENDER_LINEMEETSLAND_H

#include <functional>
#include <optional>

#include <osg/Vec3f>

namespace MWRender
{
    /// Follows a straight line from start to end, and returns the first place where it goes down to the land or below
    /// it. The land's height at a place comes from landHeight. The line is checked every step world units, so land
    /// narrower than a step can be missed. The place is then found to a small fraction of a step. Returns nothing when
    /// the line stays above the land all the way, and start when it begins under the land.
    std::optional<osg::Vec3f> findWhereLineMeetsLand(const osg::Vec3f& start, const osg::Vec3f& end, float step,
        const std::function<float(float x, float y)>& landHeight);
}

#endif
