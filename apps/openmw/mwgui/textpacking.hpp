#ifndef MWGUI_TEXTPACKING_H
#define MWGUI_TEXTPACKING_H

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace MWGui
{
    /// Breaks a screen's status line into lines that fit in the room, such as "6 in progress      2 finished". Its
    /// parts are kept apart by wide gaps of six spaces, and a line break only comes at a gap. A status that fits stays
    /// one line, and a part wider than the room gets a line of its own. Measure gives a text's width.
    std::vector<std::string> packStatus(
        const std::string& status, int room, const std::function<int(const std::string&)>& measure);

    /// Puts things of these widths, such as button hints, in rows that fit the width, with a gap between neighbours.
    /// Each row lists the things' indexes, and a thing wider than the width gets a row of its own.
    std::vector<std::vector<std::size_t>> packRows(const std::vector<int>& widths, int gap, int width);
}

#endif
