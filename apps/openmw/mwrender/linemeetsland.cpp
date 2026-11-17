#include "linemeetsland.hpp"

#include <algorithm>
#include <cmath>

namespace MWRender
{
    std::optional<osg::Vec3f> findWhereLineMeetsLand(const osg::Vec3f& start, const osg::Vec3f& end, float step,
        const std::function<float(float x, float y)>& landHeight)
    {
        const osg::Vec3f line = end - start;
        const auto pointAt = [&](float along) { return start + line * along; };
        const auto isUnderLand = [&](float along) {
            const osg::Vec3f point = pointAt(along);
            return point.z() <= landHeight(point.x(), point.y());
        };

        if (isUnderLand(0.f))
            return start;
        const float length = line.length();
        if (!(length > 0.f) || !(step > 0.f))
            return std::nullopt;

        // Each step is a share of the whole line.
        const float stepAlong = std::min(1.f, step / length);
        float above = 0.f;
        while (above < 1.f)
        {
            const float next = std::min(1.f, above + stepAlong);
            if (isUnderLand(next))
            {
                // The line meets the land between the two points. Halving the gap twelve times leaves less than a
                // four-thousandth of a step.
                float under = next;
                for (int i = 0; i < 12; ++i)
                {
                    const float middle = (above + under) / 2.f;
                    if (isUnderLand(middle))
                        under = middle;
                    else
                        above = middle;
                }
                return pointAt(under);
            }
            above = next;
        }
        return std::nullopt;
    }
}
