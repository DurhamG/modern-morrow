#ifndef MWGUI_TOUCHOVERLAY_H
#define MWGUI_TOUCHOVERLAY_H

#include <string>
#include <vector>

#include <MyGUI_Types.h>

namespace MyGUI
{
    class Widget;
}

namespace MWGui
{
    /// Draws what the touch controls show over the game: the mode picker, the quick keys radial and the wait for a
    /// quickload. The touch manager works out where everything goes, in screen pixels, and this draws it on the
    /// ControllerNotification layer. See ui-rework/design/touch.md.
    class TouchOverlay
    {
    public:
        TouchOverlay();
        ~TouchOverlay();

        /// Draws the mode picker. Each item is a box in screen pixels with its caption. "current" is the mode the
        /// player is in, and "highlighted" the one under the thumb.
        void showModes(const std::vector<MyGUI::IntCoord>& boxes, const std::vector<std::string>& captions, int current,
            int highlighted, const MyGUI::IntSize& screen);

        /// Draws the quick keys radial: one box per d-pad slot from 1 to 8, in screen pixels, and the box of its
        /// centre, which shows what tapping outside does.
        void showRadial(
            const std::vector<MyGUI::IntCoord>& slots, const MyGUI::IntCoord& centre, const MyGUI::IntSize& screen);

        /// Draws the label and a bar that fills as a hold goes on, such as the quickload's.
        void showProgress(const std::string& label, float fraction);

        void hide();

    private:
        MyGUI::Widget* prepare();
        MyGUI::IntCoord toCanvas(const MyGUI::IntCoord& pixels, const MyGUI::IntSize& screen) const;

        MyGUI::Widget* mRoot = nullptr;
    };
}

#endif
