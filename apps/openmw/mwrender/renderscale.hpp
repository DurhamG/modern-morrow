#ifndef OPENMW_MWRENDER_RENDERSCALE_H
#define OPENMW_MWRENDER_RENDERSCALE_H

namespace MWRender
{
    /// How many pixels the world is drawn in along one side of a screen that is screenSize pixels along that side, at
    /// the render scale setting's fraction of it. The fraction is kept between 0 and 1, and the size is at least one
    /// pixel.
    int scaledRenderSize(int screenSize, float scale);
}

#endif
