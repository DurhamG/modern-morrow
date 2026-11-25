#ifndef OPENMW_MWGUI_RUNFATIGUECHOICES_H
#define OPENMW_MWGUI_RUNFATIGUECHOICES_H

#include <algorithm>
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

    /// The choice that steps from the one nearest a value, delta places along the list. Past either end, it wraps
    /// round when wrap is true, and stops at the end otherwise.
    inline float stepRunFatigueChoice(float value, int delta, bool wrap)
    {
        const int count = static_cast<int>(sRunFatigueChoices.size());
        int index = static_cast<int>(nearestRunFatigueChoice(value)) + delta;
        if (wrap)
            index = ((index % count) + count) % count;
        else
            index = std::clamp(index, 0, count - 1);
        return sRunFatigueChoices[static_cast<std::size_t>(index)];
    }
}

#endif
