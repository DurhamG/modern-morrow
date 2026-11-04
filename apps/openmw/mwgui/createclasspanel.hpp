#ifndef MWGUI_CREATECLASSPANEL_H
#define MWGUI_CREATECLASSPANEL_H

#include <memory>
#include <string>
#include <vector>

#include <SDL_events.h>

#include <components/esm/refid.hpp>

#include "controllerscreen.hpp"

namespace MyGUI
{
    class Widget;
}

namespace MWGui
{
    class CreateClassDialog;
    class TextInputDialog;
    class WindowBase;

    /// Making a custom class in character creation, in controller menus mode, as a full screen. The class is on the
    /// left: its name, description, specialization, favorite attributes, and major and minor skills, one line each.
    /// The right side lists the choices for the highlighted line, with the current one marked, and describes the
    /// highlighted choice. The custom class window stays hidden and still holds the class.
    class CreateClassPanel
    {
    public:
        explicit CreateClassPanel(CreateClassDialog& window);
        ~CreateClassPanel();

        void show();
        void hide();
        void update();

        void onFrame(float duration);
        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg);

    private:
        // A line of the class on the left.
        struct Line
        {
            enum class Kind
            {
                Name,
                Description,
                Specialization,
                Attribute,
                MajorSkill,
                MinorSkill,
            };
            Kind mKind;
            // Which favorite attribute, or which major or minor skill
            int mIndex = 0;
        };

        // A choice on the right: a group header, or something to pick.
        struct Choice
        {
            std::string mHeader;
            ESM::RefId mId;
            int mSpecialization = -1;
            std::string mIcon;
            std::string mName;
            std::string mDetail;
            std::string mDescription;
            bool mCurrent = false;
            std::string mMarker;
        };

        CreateClassDialog& mWindow;
        MyGUI::Widget* mRoot = nullptr;
        std::vector<Line> mLines;
        std::vector<Choice> mChoices;
        int mLineFocus = 0;
        bool mChoicesFocused = false;
        int mChoiceFocus = 0;
        int mChoiceScroll = 0;
        HeldDirection mHeldDirection;
        std::unique_ptr<TextInputDialog> mTextDialog;
        // Whether the open text dialog is for the description rather than the name
        bool mEditingDescription = false;

        void readChoices();
        int choiceCount() const;
        const Choice* focusedChoice() const;
        ESM::RefId currentId(const Line& line) const;

        void drawLines(int x, int y, int w, int h);
        void drawChoices(int x, int y, int w, int h);

        void chooseLine();
        void pick(const Choice& choice);
        void editText(bool description);
        void onTextDone(WindowBase* sender);
    };
}

#endif
