#include "choicescreen.hpp"

#include <algorithm>

#include <MyGUI_ImageBox.h>
#include <SDL_gamecontroller.h>

#include "controllerscreen.hpp"

namespace MWGui
{
    MyGUI::IntCoord ChoiceScreen::draw(MyGUI::Widget* root, int& scroll) const
    {
        using namespace ControllerUi;
        clearPanel(root);
        const MyGUI::IntSize size = root->getSize();
        addRect(root, 0, 0, size.width, size.height, MyGUI::Colour::Black)->setNeedMouseFocus(true);

        addText(root, sMargin, 20, size.width - 2 * sMargin, 40, mTitle, sFontTitle, sBrightColour);
        addRect(root, sMargin, 66, size.width - 2 * sMargin, 1, sAccentColour, 0.2f);
        const int top = 82;
        const int bottom = size.height - 64;

        // The choices on the left.
        constexpr int entryH = 40;
        constexpr int listW = 230;
        const int count = static_cast<int>(mChoices.size());
        const int visible = std::max(1, (bottom - top - 12) / entryH);
        scroll = std::clamp(scroll, std::max(0, mFocus - visible + 1), std::max(0, mFocus));
        addRect(root, sMargin, top, listW, bottom - top, sPanelColour, 0.9f);
        for (int i = scroll; i < count && i < scroll + visible; ++i)
        {
            const int ry = top + 6 + (i - scroll) * entryH;
            const bool focused = i == mFocus;
            if (focused)
            {
                addRect(root, sMargin + 4, ry, listW - 8, entryH - 4, sAccentColour, 0.16f);
                addRect(root, sMargin + 4, ry, 3, entryH - 4, sAccentColour);
            }
            addText(root, sMargin + 18, ry, listW - 30, entryH - 4, escaped(mChoices[i]), sFontNormal,
                focused ? sActiveColour : sBrightColour);
        }

        // The picture in the middle, above the description.
        const int px = sMargin + listW + 20;
        const int pictureW = std::min(380, (size.width - px - sMargin) / 2);
        const int pictureH = pictureW / 2;
        const MyGUI::IntCoord picture(px, top, pictureW, pictureH);
        addRect(root, px, top, pictureW, pictureH, MyGUI::Colour::Black);
        int ty = top + pictureH + 16;
        addText(root, px, ty, pictureW, 34, escaped(mName), sFontLarge, sBrightColour);
        ty += 34;
        if (!mSubtitle.empty())
        {
            addText(root, px, ty, pictureW, 24, mSubtitle, sFontSmall, sDimColour);
            ty += 30;
        }
        for (const std::string& line : wrapText(root, localised(mDescription), pictureW, sFontSmall, 12))
        {
            if (ty + 22 > bottom)
                break;
            addText(root, px, ty, pictureW, 22, escaped(line), sFontSmall, sTextColour);
            ty += 22;
        }

        // The facts and sections on the right.
        const int dx = px + pictureW + 28;
        const int dw = size.width - sMargin - dx;
        ty = top;
        for (const auto& [label, value] : mFacts)
        {
            addText(root, dx, ty, dw, 28, label, sFontNormal, sTextColour);
            addText(
                root, dx, ty, dw, 28, value, sFontNormal, sBrightColour, MyGUI::Align::Right | MyGUI::Align::VCenter);
            ty += 30;
        }
        for (const auto& [heading, lines] : mSections)
        {
            ty += 8;
            if (ty + 26 > bottom)
                break;
            addText(root, dx, ty, dw, 24, heading, sFontSmall, sDimColour);
            ty += 26;
            for (const std::string& line : lines)
            {
                if (ty + 26 > bottom)
                    break;
                addText(root, dx, ty, dw, 26, line, sFontNormal, sTextColour);
                ty += 26;
            }
        }

        // The button hints along the bottom.
        int hx = sMargin;
        const int hintY = size.height - 48;
        for (const auto& [button, label] : mHints)
        {
            addGlyph(root, hx, hintY + 4, 22, static_cast<SDL_GameControllerButton>(button));
            MyGUI::TextBox* text = addText(root, hx + 30, hintY, 220, 30, label, sFontNormal, sBrightColour);
            hx += 30 + text->getTextSize().width + 22;
        }
        return picture;
    }
}
