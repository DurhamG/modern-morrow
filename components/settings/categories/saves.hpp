#ifndef OPENMW_COMPONENTS_SETTINGS_CATEGORIES_SAVES_H
#define OPENMW_COMPONENTS_SETTINGS_CATEGORIES_SAVES_H

#include <components/settings/sanitizerimpl.hpp>
#include <components/settings/settingvalue.hpp>

#include <osg/Math>
#include <osg/Vec2f>
#include <osg/Vec3f>

#include <cstdint>
#include <string>
#include <string_view>

namespace Settings
{
    struct SavesCategory : WithIndex
    {
        using WithIndex::WithIndex;

        SettingValue<std::string> mCharacter{ mIndex, "Saves", "character" };
        SettingValue<bool> mAutosave{ mIndex, "Saves", "autosave" };
        SettingValue<int> mMaxQuicksaves{ mIndex, "Saves", "max quicksaves", makeMaxSanitizerInt(1) };
        SettingValue<bool> mQuicksaveHistory{ mIndex, "Saves", "quicksave history" };
        SettingValue<int> mAutosaveInterval{ mIndex, "Saves", "autosave interval", makeMaxSanitizerInt(0) };
        SettingValue<bool> mLoadNewestSave{ mIndex, "Saves", "load newest save" };
    };
}

#endif
