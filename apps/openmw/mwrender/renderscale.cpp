#include "renderscale.hpp"

#include <algorithm>
#include <cmath>

namespace MWRender
{
    int scaledRenderSize(int screenSize, float scale)
    {
        const float fraction = std::clamp(scale, 0.f, 1.f);
        return std::max(1, static_cast<int>(std::lround(screenSize * fraction)));
    }
}
