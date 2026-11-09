#ifndef OPENMW_COMPONENTS_FONTLOADER_H
#define OPENMW_COMPONENTS_FONTLOADER_H

#include <MyGUI_Version.h>
#include <MyGUI_XmlDocument.h>

#include <components/myguiplatform/myguidatamanager.hpp>
#include <components/toutf8/toutf8.hpp>

namespace VFS
{
    class Manager;
}

namespace MyGUI
{
    class ITexture;
    class ResourceManualFont;
}

namespace Gui
{
    /// @brief loads Morrowind's .fnt/.tex fonts for use with MyGUI and OSG
    /// @note The FontLoader needs to remain in scope as long as you want to use the loaded fonts.
    class FontLoader
    {
    public:
        /// @param exportFonts export the converted fonts (Images and XML with glyph metrics) to files?
        /// @param preferTrueType use the TrueType fonts that come with OpenMW in place of the game data's bitmap fonts.
        /// A configured font is still used when it is a TrueType font.
        /// @param glyphScale how many screen pixels the TrueType glyphs get per unit of the font size setting. The
        /// glyphs are always made at least as large as the GUI scaling factor needs. A larger value keeps text sharp
        /// where it is drawn larger than the font size.
        FontLoader(ToUTF8::FromType encoding, const VFS::Manager* vfs, float scalingFactor, bool exportFonts,
            bool preferTrueType = false, float glyphScale = 0.f);

        void overrideLineHeight(MyGUI::xml::ElementPtr node, std::string_view file, MyGUI::Version version);

        static std::string_view getFontForFace(std::string_view face);

    private:
        struct FontId
        {
            std::string_view mValue;
        };

        ToUTF8::FromType mEncoding;
        const VFS::Manager* mVFS;
        float mScalingFactor;
        bool mExportFonts;
        bool mPreferTrueType;
        float mGlyphScale;

        void loadFonts();
        /// Returns false when no font of that name was loaded. With allowBitmap false, a bitmap font of that name is
        /// left out.
        bool loadFont(FontId fontId, std::string_view fontName, bool allowBitmap = true);

        void loadBitmapFont(FontId fontId, const VFS::Path::Normalized& path, std::istream& stream);
        void loadTrueTypeFont(FontId fontId, const VFS::Path::Normalized& path, std::istream& stream, float glyphScale);

        FontLoader(const FontLoader&);
        void operator=(const FontLoader&);
    };

}

#endif
