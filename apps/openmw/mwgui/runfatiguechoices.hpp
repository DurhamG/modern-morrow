#ifndef OPENMW_MWGUI_RUNFATIGUECHOICES_H
#define OPENMW_MWGUI_RUNFATIGUECHOICES_H

#include <array>
#include <cmath>
#include <cstddef>

namespace MWGui
{
    /// The choices the menus offer for the Run Fatigue setting, in their order: Morrowind's fatigue, half, a quarter
    /// and none.
    inline constexpr std::array<float, 4> sRunFatigueChoices = { 1.f, 0.5f, 0.25f, 0.f };

    /// The choice nearest to a value, such as one set by hand in settings.cfg.
    inline std::size_t nearestRunFatigueChoice(float value)
    {
        std::size_t nearest = 0;
        for (std::size_t i = 1; i < sRunFatigueChoices.size(); ++i)
            if (std::abs(sRunFatigueChoices[i] - value) < std::abs(sRunFatigueChoices[nearest] - value))
                nearest = i;
        return nearest;
    }
}

#endif
