#ifndef OPENMW_COMPONENTS_SETTINGS_CATEGORIES_HUD_H
#define OPENMW_COMPONENTS_SETTINGS_CATEGORIES_HUD_H

#include <components/settings/settingvalue.hpp>

#include <osg/Math>
#include <osg/Vec2f>
#include <osg/Vec3f>

#include <cstdint>
#include <string>
#include <string_view>

namespace Settings
{
    struct HUDCategory : WithIndex
    {
        using WithIndex::WithIndex;

        SettingValue<bool> mCrosshair{ mIndex, "HUD", "crosshair" };
        SettingValue<bool> mMouseEmulationCursor{ mIndex, "HUD", "mouse emulation cursor" };
        SettingValue<float> mSize{ mIndex, "HUD", "size", makeClampSanitizerFloat(0.25f, 4.f) };
    };
}

#endif
