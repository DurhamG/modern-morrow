#ifndef MWGUI_CONTROLLERSCREEN_H
#define MWGUI_CONTROLLERSCREEN_H

#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <MyGUI_Colour.h>
#include <MyGUI_TextBox.h>
#include <SDL_gamecontroller.h>

#include "windowbase.hpp"

namespace MyGUI
{
    class ImageBox;
}

namespace MWWorld
{
    class Ptr;
}

namespace MWGui
{
    /// A full screen menu in controller menus mode, such as the inventory or the magic screen. It fills the screen
    /// below the tab bar, on a canvas 720 units tall, and builds its widgets in code each time it redraws.
    class ControllerScreen : public WindowBase
    {
    public:
        ControllerScreen();

        void onResChange(int width, int height) override;

    protected:
        void setActiveControllerWindow(bool active) override;

        virtual void redraw() = 0;

        /// Destroys the widgets of the last redraw, and returns a new, empty canvas with the screen's background.
        MyGUI::Widget* clearContent();

        MyGUI::Widget* mContent = nullptr;
    };

    /// Reads the d-pad and the left stick. Reports a direction when one is first pressed, then again and again while
    /// it is held.
    class HeldDirection
    {
    public:
        /// Returns true when the selection should move by dx and dy.
        bool update(float duration, int& dx, int& dy);
        void reset();

    private:
        int mX = 0;
        int mY = 0;
        float mRepeatTimer = 0.f;
    };

    /// Sizes, colours and drawing functions the controller screens share, so they look alike.
    namespace ControllerUi
    {
        // The tab bar overlay covers the top 44 units of the canvas. The screen's own tabs sit below it.
        inline constexpr int sTabBarHeight = 44;
        inline constexpr int sMargin = 32;
        inline constexpr int sContentTop = 110;
        inline constexpr int sContentBottomMargin = 24;

        inline constexpr int sFontSmall = 15;
        inline constexpr int sFontNormal = 18;
        inline constexpr int sFontLarge = 22;
        inline constexpr int sFontTitle = 26;

        inline constexpr float sStickDeadzone = 0.15f;

        inline const MyGUI::Colour sTextColour = MyGUI::Colour::parse("#caa560");
        inline const MyGUI::Colour sBrightColour = MyGUI::Colour::parse("#dfc99f");
        inline const MyGUI::Colour sDimColour = MyGUI::Colour::parse("#8c7a55");
        inline const MyGUI::Colour sActiveColour = MyGUI::Colour::White;
        inline const MyGUI::Colour sAccentColour = MyGUI::Colour::parse("#caa560");
        inline const MyGUI::Colour sNewColour = MyGUI::Colour::parse("#7fb3ff");
        inline const MyGUI::Colour sBetterColour = MyGUI::Colour::parse("#7fd36b");
        inline const MyGUI::Colour sWorseColour = MyGUI::Colour::parse("#e0675a");
        inline const MyGUI::Colour sPanelColour = MyGUI::Colour::parse("#15120c");
        inline const MyGUI::Colour sIconBackColour = MyGUI::Colour::parse("#1a1610");

        /// Hides a window that a controller screen or panel shows instead, while the window keeps working.
        void hideWindow(MyGUI::Widget* mainWidget);

        /// Creates a hidden widget covering a layer, for drawing a panel on. Returns nullptr when the layer is
        /// missing, or when controller menus mode is off.
        MyGUI::Widget* createPanelRoot(const std::string& layer);

        /// Clears a panel's widget and sizes it to its layer, for drawing the panel again.
        void clearPanel(MyGUI::Widget* root);

        /// Where drawProgressPanel puts its panel.
        enum class ProgressPlacement
        {
            Middle,
            Bottom,
            /// A smaller panel in the bottom right corner, for something brief during play, such as saving.
            BottomRight,
        };

        /// Draws a label and a progress bar on a small panel of their own, such as while resting.
        void drawProgressPanel(MyGUI::Widget* root, const std::string& label, float fraction,
            ProgressPlacement placement = ProgressPlacement::Middle);

        /// Escapes text, such as an item name, that can contain characters MyGUI reads as colour codes.
        std::string escaped(std::string_view text);

        /// Replaces "#{...}" strings with their localised text, so the length of the text is known before it is
        /// wrapped.
        std::string localised(const std::string& text);

        /// Formats a number with at most two decimals, and none for whole numbers.
        std::string formatNumber(float value);

        MyGUI::ImageBox* addRect(
            MyGUI::Widget* parent, int x, int y, int w, int h, const MyGUI::Colour& colour, float alpha = 1.f);

        /// Shows text, where "#{Context:Key}" is replaced with the localised string.
        MyGUI::TextBox* addText(MyGUI::Widget* parent, int x, int y, int w, int h, std::string_view caption,
            int fontHeight, const MyGUI::Colour& colour,
            MyGUI::Align align = MyGUI::Align::Left | MyGUI::Align::VCenter);

        /// Shows the controller's icon for a button.
        void addGlyph(MyGUI::Widget* parent, int x, int y, int size, SDL_GameControllerButton button);

        /// Shows an item's icon on a dark square.
        void addIcon(MyGUI::Widget* parent, int x, int y, int size, const MWWorld::Ptr& ptr, bool equipped);

        /// Shows an image, such as a magic effect's icon, on a dark square.
        void addImage(MyGUI::Widget* parent, int x, int y, int size, const std::string& texture);

        /// Splits text into lines that fit the width, at most maxLines of them. The last line holds whatever is left.
        /// The text is measured in a hidden text box on the parent.
        std::vector<std::string> wrapText(
            MyGUI::Widget* parent, const std::string& text, int width, int fontHeight, std::size_t maxLines = 2);

        /// A button hint: an SDL controller button, and what it does.
        using Hint = std::pair<SDL_GameControllerButton, std::string>;

        /// Draws button hints in rows along the bottom of an area, starting another row above when one is full.
        /// Returns the top of the highest row, or the bottom when there are no hints.
        int drawHints(MyGUI::Widget* parent, int x, int width, int bottom, const std::vector<Hint>& hints);

        /// Draws the screen's own tabs, switched with the shoulder buttons, and a status line at their right.
        /// Clicking a tab switches to it, as pressing a shoulder button that many times does.
        void drawTabRow(MyGUI::Widget* parent, int width, const std::vector<std::string>& captions, int selected,
            const std::string& status);

        /// Makes an area that the mouse can point at and click, such as a row of a list. Moving the mouse onto it
        /// calls onPoint, and a left click calls onClick. Either can be empty. They run at the start of the next
        /// frame, since they usually draw the panel again, which destroys the area's widget.
        void addPointerTarget(MyGUI::Widget* parent, int x, int y, int w, int h, std::function<void()> onPoint,
            std::function<void()> onClick);

        /// Points at the target under the mouse, when it is a different one than before. The mouse manager calls it
        /// when the mouse moves in controller menus mode.
        void pointerMoved(int x, int y);
        /// Clicks the target under the mouse. Returns true when there was one, so the click goes no further.
        bool pointerClicked(int x, int y);

        /// Makes an area that the mouse wheel scrolls, such as a page of text. The wheel otherwise works as the
        /// d-pad's up and down. onWheel gets the wheel's steps, positive when it turns away from the player, and runs
        /// at the start of the next frame.
        void addWheelTarget(MyGUI::Widget* parent, int x, int y, int w, int h, std::function<void(int steps)> onWheel);
        /// Turns the wheel over the area under the mouse. Returns true when an area took it.
        bool pointerWheel(int x, int y, int steps);
        /// How far the wheel scrolls text per step, in canvas units.
        inline constexpr float sWheelScrollStep = 90.f;

        /// Presses a controller button for the menu that shows, as clicking a button hint does.
        void pressButton(SDL_GameControllerButton button);

        /// Runs what the mouse asked for since the last frame. The window manager calls it every frame, before the
        /// windows update.
        void runPointerActions();
    }

    /// A list for a controller screen that scrolls to keep its focused row in view. Its rows are group headers and
    /// items, and only items take the focus. The screen draws each row, and the list keeps the focus and the scrolling
    /// for it. It also moves the focus with the held d-pad or left stick, and with the mouse: pointing at an item
    /// focuses it, and clicking an item focuses it and activates it, as A does.
    class ControllerList
    {
    public:
        struct Row
        {
            int mHeight = 0;
            bool mItem = true;
        };

        /// The focused item, counting items only.
        int getFocus() const { return mFocus; }
        void setFocus(int item) { mFocus = item; }
        /// Focuses the first item and scrolls to the top.
        void reset();

        /// The first row drawn. Setting it puts a row at the top, such as a group's header, as long as the focused
        /// item stays in view.
        int getScroll() const { return mScroll; }
        void setScroll(int row) { mScroll = row; }

        /// Moves the focus with the held d-pad or left stick, within itemCount items. Returns true when it moved, so
        /// the screen draws again.
        bool updateHeld(float duration, int itemCount);
        /// Forgets a held direction, as when the screen stops being the active one.
        void resetHeld() { mHeld.reset(); }

        /// Draws the rows that fit in an area, keeping the focused item in view, and never scrolling so far that space
        /// is left empty below the last row. drawRow draws a row at a height,
        /// and says whether the row is the focused item. Returns false when there are no items to draw, so the screen
        /// can show a message instead. Pointing at an item calls onChange after focusing it. Clicking one does the
        /// same, then calls onActivate.
        bool draw(MyGUI::Widget* parent, int x, int y, int w, int h, const std::vector<Row>& rows,
            const std::function<void(std::size_t row, int y, bool focused)>& drawRow, std::function<void()> onChange,
            std::function<void()> onActivate);

    private:
        int mFocus = 0;
        int mScroll = 0;
        HeldDirection mHeld;
    };
}

#endif
