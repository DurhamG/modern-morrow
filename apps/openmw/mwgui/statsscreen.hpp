#ifndef MWGUI_STATSSCREEN_H
#define MWGUI_STATSSCREEN_H

#include <array>
#include <string>
#include <vector>

#include <MyGUI_Types.h>

#include <components/esm/refid.hpp>

#include "controllerscreen.hpp"

namespace MWGui
{
    /// The stats menu in controller menus mode. The Character tab lays out the character's race, class, level,
    /// health and attributes as tiles. The Skills and Factions tabs list the rest. Selecting anything shows its
    /// details beside it, so the player never needs a tooltip. The screen also shows how the next level up will
    /// go, which the normal menu only shows in a tooltip. See ui-rework/design/stats.md.
    class StatsScreen : public ControllerScreen
    {
    public:
        StatsScreen();

        void onOpen() override;
        void onFrame(float duration) override;
        void clear() override;

    protected:
        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg) override;
        bool onControllerThumbstickEvent(const SDL_ControllerAxisEvent& arg) override;

    private:
        enum class Tab
        {
            Character,
            Skills,
            Factions,
        };
        static constexpr int sTabCount = 3;

        // Something the player can select on the Character tab, or in a list.
        struct Info
        {
            enum class Kind
            {
                Race,
                Class,
                BirthSign,
                Level,
                Health,
                Magicka,
                Fatigue,
                Attribute,
                Skill,
                Reputation,
                Bounty,
                Faction,
            };
            Kind mKind = Kind::Level;
            // The attribute, skill or faction
            ESM::RefId mId;
        };

        // A line in a list: a group header, or an index into the list's entries.
        struct Row
        {
            std::string mHeader;
            int mEntry = -1;
        };

        struct ListState
        {
            int mFocus = 0;
            int mScroll = 0;
        };

        Tab mTab = Tab::Character;
        std::array<ListState, sTabCount> mLists;

        // The Character tab's tiles, where the last redraw put them, for moving between them with the d-pad.
        std::vector<std::pair<Info, MyGUI::IntCoord>> mTiles;

        // A summary of what the screen last showed, to tell when it needs to redraw.
        std::string mShownState;
        float mRefreshTimer = 0.f;
        HeldDirection mHeldDirection;

        std::string currentState() const;
        void redraw() override;

        void drawCharacter(int width);
        void drawTile(int x, int y, int w, int h, const Info& info, bool focused);
        void drawBar(int x, int y, int w, int h, float fraction, const MyGUI::Colour& colour);

        std::vector<Info> listEntries() const;
        std::vector<Row> buildRows(const std::vector<Info>& entries) const;
        static std::vector<int> groupStarts(const std::vector<Row>& rows);
        void drawList(int x, int y, int w, int h);
        void drawEntry(int x, int y, int w, int h, const Info& info, bool focused);

        void drawDetails(int x, int y, int w, int h, const Info& info);
        int drawParagraph(int x, int y, int w, const std::string& text, int fontHeight, const MyGUI::Colour& colour);
        int drawValueRow(int x, int y, int w, const std::string& label, const std::string& value,
            const MyGUI::Colour& colour);

        void move(int dx, int dy);
        void moveOnTiles(int dx, int dy);
        void changeTab(int delta);
        void nextGroup();
    };
}

#endif
