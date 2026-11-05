#ifndef OPENMW_LUAUI_TEXT
#define OPENMW_LUAUI_TEXT

#include <string>
#include <string_view>
#include <vector>

#include <MyGUI_EditBox.h>

#include "widget.hpp"

namespace LuaUi
{
    class LuaText : public MyGUI::EditBox, public WidgetExtension
    {
        MYGUI_RTTI_DERIVED(LuaText)

    public:
        LuaText();
        void initialize() override;
        void updateProperties() override;
        void setCaption(const MyGUI::UString& caption) override;

        const std::vector<std::string_view>& allUsedProperties() const override;

    private:
        bool mAutoSized;
        // The font of the widget's skin, which the Text widget uses unless a script names another one.
        std::string mSkinFont;

    protected:
        MyGUI::IntSize calculateSize() const override;
        MyGUI::IntCoord getTextRegion() const override;
        MyGUI::IntSize getViewSize() const override;
    };

    struct TextMeasureOptions
    {
        int mTextSize = 10;
        bool mMultiline = false;
        bool mWordWrap = false;
        // The width to wrap the text at. Only used with mWordWrap.
        int mWidth = 0;
        // The name of the font. Empty for the Text widget's usual font.
        std::string mFont;
    };

    // Throws an error unless the engine loaded a font with this name.
    void verifyFontName(const std::string& name);

    // The size that a Text widget with these properties gives the text. Uses MyGUI widgets, so it must be called
    // from the main thread.
    MyGUI::IntSize measureText(std::string_view text, const TextMeasureOptions& options);
}

#endif // OPENMW_LUAUI_TEXT
