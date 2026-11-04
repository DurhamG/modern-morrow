#ifndef MWGUI_READINGPANEL_H
#define MWGUI_READINGPANEL_H

#include <string>

#include <MyGUI_Types.h>

namespace MyGUI
{
    class ImageBox;
    class Widget;
}

namespace MWGui
{
    /// A book or scroll being read in controller menus mode. The text is one tall column on a page in the middle of the
    /// screen, which the sticks and the d-pad scroll, instead of the book's pairs of pages. The book and scroll windows
    /// stay hidden and still handle taking and closing.
    class ReadingPanel
    {
    public:
        ~ReadingPanel();

        void show(const std::string& title, const std::string& text, bool shrinkTextAtLastTag, bool canTake);
        void hide();

        /// Scrolls the text while a stick or the d-pad is held.
        void onFrame(float duration);

    private:
        MyGUI::Widget* mRoot = nullptr;
        MyGUI::Widget* mPaper = nullptr;
        MyGUI::ImageBox* mScrollThumb = nullptr;
        MyGUI::IntCoord mTrack;
        float mScroll = 0.f;
        int mScrollMax = 0;

        void place();
    };
}

#endif
