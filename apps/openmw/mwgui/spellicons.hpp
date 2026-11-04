#ifndef MWGUI_SPELLICONS_H
#define MWGUI_SPELLICONS_H

#include <map>

#include <components/esm/refid.hpp>

namespace MyGUI
{
    class Widget;
    class ImageBox;
}

namespace MWGui
{

    class SpellIcons
    {
    public:
        /// With hideLasting, effects that last for good leave no icon: those of abilities, such as from the
        /// player's race and birthsign, and of gear the player wears with a constant enchantment. Diseases and
        /// curses still show.
        explicit SpellIcons(bool hideLasting = false)
            : mHideLasting(hideLasting)
        {
        }

        void updateWidgets(MyGUI::Widget* parent, bool adjustSize);

    private:
        std::map<ESM::RefId, MyGUI::ImageBox*> mWidgetMap;
        bool mHideLasting;
    };

}

#endif
