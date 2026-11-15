#include "listscroll.hpp"

#include <algorithm>

namespace MWGui::ListScroll
{
    int itemCount(const std::vector<Row>& rows)
    {
        int count = 0;
        for (const Row& row : rows)
            count += row.mItem ? 1 : 0;
        return count;
    }

    int rowOfItem(const std::vector<Row>& rows, int item)
    {
        for (int i = 0, seen = 0; i < static_cast<int>(rows.size()); ++i)
        {
            if (!rows[i].mItem)
                continue;
            if (seen++ == item)
                return i;
        }
        return 0;
    }

    int itemsBefore(const std::vector<Row>& rows, int row)
    {
        int item = 0;
        for (int i = 0; i < row; ++i)
            item += rows[i].mItem ? 1 : 0;
        return item;
    }

    int scrollToShow(const std::vector<Row>& rows, int scroll, int focusRow, int height)
    {
        scroll = std::clamp(scroll, 0, static_cast<int>(rows.size()) - 1);
        if (focusRow < scroll)
            scroll = focusRow > 0 && !rows[focusRow - 1].mItem ? focusRow - 1 : focusRow;
        while (scroll < focusRow)
        {
            int bottomOfFocus = 0;
            for (int i = scroll; i <= focusRow; ++i)
                bottomOfFocus += rows[i].mHeight;
            if (bottomOfFocus <= height)
                break;
            ++scroll;
        }
        return scroll;
    }

    int maxScroll(const std::vector<Row>& rows, int height)
    {
        int result = static_cast<int>(rows.size()) - 1;
        for (int below = rows.back().mHeight; result > 0 && below + rows[result - 1].mHeight <= height;)
            below += rows[--result].mHeight;
        return result;
    }

    int itemRowAt(const std::vector<Row>& rows, int scroll, int height, int pointY)
    {
        int focusRow = -1;
        int lastItemRow = -1;
        int rowTop = 0;
        for (int i = scroll; i < static_cast<int>(rows.size()) && rowTop + rows[i].mHeight <= height; ++i)
        {
            rowTop += rows[i].mHeight;
            if (!rows[i].mItem)
                continue;
            lastItemRow = i;
            if (focusRow < 0 && pointY < rowTop)
                focusRow = i;
        }
        return focusRow >= 0 ? focusRow : lastItemRow;
    }
}
