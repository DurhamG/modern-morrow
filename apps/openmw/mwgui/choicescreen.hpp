#ifndef MWGUI_CHOICESCREEN_H
#define MWGUI_CHOICESCREEN_H

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <MyGUI_Types.h>

namespace MyGUI
{
    class Widget;
}

namespace MWGui
{
    class ControllerList;

    /// A full screen choice from a list in controller menus mode, such as a class or a birthsign in character
    /// creation. The list is on the left, a picture in the middle, and the highlighted choice's details on the right.
    struct ChoiceScreen
    {
        std::string mTitle;
        std::vector<std::string> mChoices;
        int mFocus = 0;

        // The highlighted choice's details
        std::string mName;
        std::string mSubtitle;
        std::vector<std::pair<std::string, std::string>> mFacts;
        // Headings, each with lines below it, such as "Major Skills" and the skills
        std::vector<std::pair<std::string, std::vector<std::string>>> mSections;
        std::string mDescription;

        // The button hints along the bottom: an SDL controller button, and what it does
        std::vector<std::pair<int, std::string>> mHints;

        // What happens when the mouse points at a choice and when it clicks one. The list has focused the choice
        // before either runs. Either can be empty.
        std::function<void()> mOnPoint;
        std::function<void()> mOnClick;

        /// Draws the screen on the root, and returns where the picture goes, for the caller to fill. The list keeps
        /// the choices scrolled so mFocus is in view, and its focus is the choice the mouse picked.
        MyGUI::IntCoord draw(MyGUI::Widget* root, ControllerList& list) const;
    };
}

#endif
