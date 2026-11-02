#ifndef OPENMW_MWAUTOMATION_UIDUMP_H
#define OPENMW_MWAUTOMATION_UIDUMP_H

#include <string>

#include "json.hpp"

namespace MWAutomation
{
    struct UiDumpOptions
    {
        // Include widgets that are hidden, and their children.
        bool mHidden = false;
        // When not empty, only dump the subtrees of widgets with this name.
        std::string mRoot;
        // Stop after this many levels below each root. 0 means no limit.
        int mMaxDepth = 0;
    };

    // Describes the MyGUI widget tree. Each widget has its type, its name when it has one, and "rect": [x, y, width,
    // height] in window pixels from the top left. Widgets that show text also have "text", "font" and "fontHeight".
    // "disabled", "keyFocus" and "mouseFocus" appear only when true. The result also names the widgets that have key
    // and mouse focus.
    Json dumpUi(const UiDumpOptions& options);
}

#endif
