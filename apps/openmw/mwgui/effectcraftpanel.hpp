#ifndef MWGUI_EFFECTCRAFTPANEL_H
#define MWGUI_EFFECTCRAFTPANEL_H

#include <memory>
#include <string>
#include <vector>

#include <SDL_events.h>

#include <components/esm/refid.hpp>

#include "../mwworld/ptr.hpp"

#include "controllerscreen.hpp"

namespace MyGUI
{
    class Widget;
}

namespace ESM
{
    struct MagicEffect;
}

namespace MyGUI
{
    class EditBox;
}

namespace MWGui
{
    class EffectEditorBase;
    class EnchantingDialog;
    class SpellCreationDialog;
    class TextInputDialog;
    class WindowBase;

    /// Making an enchantment or a spell in controller menus mode, as a full screen. The right side holds what is
    /// being made: for an enchantment the item, the soul gem and how it casts, then the effects, and what it costs.
    /// The left side is a list to pick from for the highlighted line on the right, such as the player's soul gems or
    /// the effects they know. Choosing an effect, or an effect already added, edits it on the left: its range,
    /// magnitude, duration and area, while the costs on the right follow along. The enchanting or spellmaking window
    /// stays hidden and still makes the enchantment or the spell. See ui-rework/design/crafting.md.
    class EffectCraftPanel
    {
    public:
        explicit EffectCraftPanel(EnchantingDialog& window);
        explicit EffectCraftPanel(SpellCreationDialog& window);
        ~EffectCraftPanel();

        void show();
        void hide();
        void update();

        void onFrame(float duration);
        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg);
        /// Handles a button while an effect is being edited. The effect dialog gets the buttons then, because it is
        /// the top modal window.
        bool onEditorButton(const SDL_ControllerButtonEvent& arg);

    private:
        // What the list on the left shows.
        enum class Picker
        {
            Effects,
            Items,
            SoulGems,
            Skills,
            Attributes,
        };

        // A line on the right side.
        struct Line
        {
            enum class Kind
            {
                Item,
                SoulGem,
                CastType,
                Effect,
                AddEffect,
            };
            Kind mKind;
            int mEffect = -1;
        };

        // A line in the list on the left: a group header, or something to pick.
        struct Entry
        {
            std::string mHeader;
            ESM::RefId mId;
            MWWorld::Ptr mItem;
            std::string mIcon;
            std::string mName;
            std::string mDetail;
            bool mAdded = false;
        };

        // A line of the effect editor.
        enum class Setting
        {
            Range,
            MagnitudeMin,
            MagnitudeMax,
            Duration,
            Area,
        };

        WindowBase& mWindow;
        EffectEditorBase& mEditor;
        // The window the panel stands in for. One of the two is set.
        EnchantingDialog* mEnchant = nullptr;
        SpellCreationDialog* mSpell = nullptr;
        MyGUI::Widget* mRoot = nullptr;

        Picker mPicker = Picker::Effects;
        std::vector<Entry> mEntries;
        bool mListFocused = false;
        ControllerList mList;
        int mLineFocus = 0;
        int mSettingFocus = 0;
        // The effect waiting for its skill or attribute to be picked
        const ESM::MagicEffect* mPendingEffect = nullptr;

        HeldDirection mHeldDirection;
        // How long left or right has been held while editing, to change values faster the longer it is held
        float mAdjustTime = 0.f;
        std::unique_ptr<TextInputDialog> mRenameDialog;

        bool isSelfEnchanting() const;
        MWWorld::Ptr merchant() const;
        MyGUI::EditBox* nameEdit() const;
        void nameAfterFirstEffect();
        bool isEditing() const;
        std::vector<Line> lines() const;
        std::vector<Setting> settings() const;
        void readEntries();
        int entryCount() const;
        const Entry* focusedEntry() const;
        void setPicker(Picker picker);

        void drawList(int x, int y, int w, int h);
        void drawEditor(int x, int y, int w, int h);
        void drawSummary(int x, int y, int w, int h);

        void chooseEntry();
        void chooseLine();
        void addEffect(const ESM::RefId& id);
        void pickItem(const MWWorld::Ptr& item);
        void pickSoulGem(const MWWorld::Ptr& gem);
        void focusNextLine();
        void adjust(int delta);
        /// Sets a setting's value to where the mouse is on its bar, which starts at barX and is barW wide.
        void setValueAtPointer(int setting, int barX, int barW);
        void rename();
        void onRenameDone(WindowBase* sender);
    };
}

#endif
