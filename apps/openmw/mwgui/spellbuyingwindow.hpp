#ifndef MWGUI_SpellBuyingWINDOW_H
#define MWGUI_SpellBuyingWINDOW_H

#include <memory>

#include <components/esm/refid.hpp>

#include "referenceinterface.hpp"
#include "servicepanel.hpp"
#include "windowbase.hpp"

namespace ESM
{
    struct Spell;
}

namespace MyGUI
{
    class Gui;
    class Widget;
}

namespace MWGui
{
    class SpellBuyingWindow : public ReferenceInterface, public WindowBase
    {
    public:
        SpellBuyingWindow();
        ~SpellBuyingWindow() override;

        void setPtr(const MWWorld::Ptr& actor) override;
        void setPtr(const MWWorld::Ptr& actor, int startOffset);

        void onOpen() override;
        void onClose() override;
        void onFrame(float dt) override;
        void clear() override { resetReference(); }

        void onResChange(int, int) override { center(); }

        std::string_view getWindowIdForLua() const override { return "SpellBuying"; }

    protected:
        MyGUI::Button* mCancelButton;
        MyGUI::TextBox* mPlayerGold;

        MyGUI::ScrollView* mSpellsView;

        std::map<MyGUI::Widget*, ESM::RefId> mSpellsWidgetMap;
        /// List of enabled/purchasable spells and their index in the full list.
        std::vector<std::pair<MyGUI::Button*, size_t>> mSpellButtons;

        void onCancelButtonClicked(MyGUI::Widget* sender);
        void onSpellButtonClick(MyGUI::Widget* sender);
        void onMouseWheel(MyGUI::Widget* sender, int rel);
        void addSpell(const ESM::Spell& spell);
        void clearSpells();
        int mCurrentY;

        void updateLabels();

        void onReferenceUnavailable() override;

        bool playerHasSpell(const ESM::RefId& id);

    private:
        static bool sortSpells(const ESM::Spell* left, const ESM::Spell* right);
        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg) override;
        size_t mControllerFocus = 0;

        // In controller menus mode, a panel on the right side of the screen shows the service instead of this window.
        std::unique_ptr<ServicePanel> mPanel;
        std::vector<ServicePanel::Entry> controllerEntries();
    };
}

#endif
