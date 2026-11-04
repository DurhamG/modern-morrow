#ifndef MWGUI_CONTROLLERSCREEN_H
#define MWGUI_CONTROLLERSCREEN_H

#include <string>
#include <string_view>
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

        /// Draws the screen's own tabs, switched with the shoulder buttons, and a status line at their right.
        void drawTabRow(MyGUI::Widget* parent, int width, const std::vector<std::string>& captions, int selected,
            const std::string& status);
    }
}

#endif
