#include "textpacking.hpp"

#include <algorithm>
#include <string_view>

namespace MWGui
{
    std::vector<std::string> packStatus(
        const std::string& status, int room, const std::function<int(const std::string&)>& measure)
    {
        std::vector<std::string> lines = { status };
        if (measure(status) <= room)
            return lines;
        constexpr std::string_view gap = "      ";
        lines.assign(1, {});
        for (std::size_t start = 0; start <= status.size();)
        {
            const std::size_t end = std::min(status.find(gap, start), status.size());
            const std::string part = status.substr(start, end - start);
            const std::string longer = lines.back().empty() ? part : lines.back() + std::string(gap) + part;
            if (!lines.back().empty() && measure(longer) > room)
                lines.push_back(part);
            else
                lines.back() = longer;
            start = end + gap.size();
        }
        return lines;
    }

    std::vector<std::vector<std::size_t>> packRows(const std::vector<int>& widths, int gap, int width)
    {
        std::vector<std::vector<std::size_t>> rows(1);
        int rowWidth = 0;
        for (std::size_t i = 0; i < widths.size(); ++i)
        {
            if (!rows.back().empty() && rowWidth + gap + widths[i] > width)
            {
                rows.emplace_back();
                rowWidth = 0;
            }
            rowWidth += (rows.back().empty() ? 0 : gap) + widths[i];
            rows.back().push_back(i);
        }
        return rows;
    }
}
