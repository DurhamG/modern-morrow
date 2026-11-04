#ifndef MWGUI_ALCHEMYPANEL_H
#define MWGUI_ALCHEMYPANEL_H

#include <memory>
#include <string>
#include <vector>

#include <SDL_events.h>

#include "../mwworld/ptr.hpp"

#include "controllerscreen.hpp"

namespace MyGUI
{
    class Widget;
}

namespace MWGui
{
    class AlchemyWindow;
    class TextInputDialog;
    class WindowBase;

    /// Brewing potions in controller menus mode, as a full screen. The player's ingredients are listed on the left
    /// with the effects they know, and A puts one in the potion or takes it out. Once the potion holds ingredients,
    /// the list puts the ones that share a known effect with them first. The potion is on the right: its
    /// ingredients, the effects it will have, the tools, and how many to brew. The alchemy window still owns the
    /// potion and brews it. See ui-rework/design/crafting.md.
    class AlchemyPanel
    {
    public:
        explicit AlchemyPanel(AlchemyWindow& window);
        ~AlchemyPanel();

        void show();
        void hide();
        void update();

        void onFrame(float duration);
        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg);

    private:
        // A line in the ingredient list: a group header, or an ingredient.
        struct Row
        {
            std::string mHeader;
            MWWorld::Ptr mItem;
            std::vector<std::string> mEffects;
            bool mInPotion = false;
        };

        AlchemyWindow& mWindow;
        MyGUI::Widget* mRoot = nullptr;
        std::vector<Row> mRows;
        int mFocus = 0;
        int mScroll = 0;
        HeldDirection mHeldDirection;
        std::unique_ptr<TextInputDialog> mRenameDialog;

        void readIngredients();
        const Row* focusedRow() const;
        int potionSlotOf(const MWWorld::Ptr& item) const;
        int ingredientCount() const;

        void drawList(int x, int y, int w, int h);
        void drawPotion(int x, int y, int w, int h);

        void toggleFocused();
        void brew();
        void rename();
        void onRenameDone(WindowBase* sender);
    };
}

#endif
