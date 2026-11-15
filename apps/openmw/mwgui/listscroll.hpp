#ifndef MWGUI_LISTSCROLL_H
#define MWGUI_LISTSCROLL_H

#include <vector>

/// How the controller menus' lists scroll. A list is rows of different heights. Each row is an item, which can take the
/// focus, or something else, such as a group's header. The scroll is the first row drawn.
namespace MWGui::ListScroll
{
    struct Row
    {
        int mHeight = 0;
        bool mItem = true;
    };

    int itemCount(const std::vector<Row>& rows);

    /// The row of an item, counting items only, or 0 when there is no such item.
    int rowOfItem(const std::vector<Row>& rows, int item);

    /// How many items come before a row.
    int itemsBefore(const std::vector<Row>& rows, int row);

    /// The scroll that keeps the focused row in view, in a list this high, starting from the current scroll. Scrolling
    /// up to an item also shows its group's header. Scrolling down goes just far enough for the row to fit.
    int scrollToShow(const std::vector<Row>& rows, int scroll, int focusRow, int height);

    /// The furthest the list can scroll without leaving empty space below its last row.
    int maxScroll(const std::vector<Row>& rows, int height);

    /// The row of the item under a point, from the list's top, when the list is scrolled to a row. It is the first
    /// item that ends below the point among the rows that fit, so a header gives the item under it, or below the last
    /// row, the last item. Returns -1 when no item fits.
    int itemRowAt(const std::vector<Row>& rows, int scroll, int height, int pointY);
}

#endif
