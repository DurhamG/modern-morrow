#ifndef MWGUI_LOCALMAP3DVIEW_H
#define MWGUI_LOCALMAP3DVIEW_H

#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <MyGUI_Delegate.h>
#include <MyGUI_MouseButton.h>
#include <MyGUI_Types.h>

#include <osg/Vec2f>
#include <osg/Vec3f>
#include <osg/ref_ptr>

#include <components/esm/refid.hpp>

#include "../mwrender/localmap.hpp"

namespace MyGUI
{
    class ITexture;
    class ImageBox;
    class Widget;
}

namespace osg
{
    class Image;
}

namespace MWRender
{
    class Map3D;
}

namespace MWWorld
{
    class CellStore;
}

namespace MWGui
{
    class CustomMarkerCollection;

    /// Shows the local map in 3D, in place of the flat map, with the same markers. The view looks north at the
    /// player from above at an angle, and follows the player as they move. The mouse drags it to look elsewhere, and
    /// the wheel zooms it when the map allows zooming. It draws only while it shows.
    class LocalMap3DView
    {
    public:
        /// Makes the view in a parent widget, at the coordinates of the flat map it replaces.
        LocalMap3DView(MyGUI::Widget* parent, const MyGUI::IntCoord& coord, MWRender::LocalMap* localMapRender,
            CustomMarkerCollection& customMarkers);
        ~LocalMap3DView();

        /// Shows or hides the view. It draws while it and the window it is in both show.
        void setVisible(bool visible);

        /// Starts or stops drawing as the window the view is in shows or hides.
        void updateDrawing();

        void onFrame(float dt, bool fogOfWar);

        /// Points the view at the player.
        void centerOnPlayer();

        /// Moves the view by a distance in the widget's units, such as for the controller's d-pad.
        void pan(int dx, int dy);

        void updateCustomMarkers();

        /// Forgets the markers of the game that ended.
        void clear();

        /// Fires when the player double-clicks the ground, with the world position under the mouse.
        MyGUI::delegates::MultiDelegate<float, float> eventGroundDoubleClicked;
        /// Fires when the player double-clicks a custom marker. The widget holds its ESM::CustomMarker.
        MyGUI::delegates::MultiDelegate<MyGUI::Widget*> eventCustomMarkerDoubleClicked;

    private:
        struct Marker
        {
            MyGUI::Widget* mWidget;
            osg::Vec3f mPosition;
        };

        /// Makes the renderer, or makes it again when the view's size changes.
        void updateRenderer();
        void update();
        /// How many world units one widget unit covers at the point the view looks at.
        float getUnitsPerWidgetUnit() const;
        /// The camera's distance from the point it looks at, for the zoom.
        float getCameraDistance() const;
        float getGroundHeight(float x, float y) const;

        void showFog(const MWRender::LocalMap::FogImage& fog);
        void updateFog(const MWWorld::CellStore& cell);
        void refreshFog();
        bool isExplored(const osg::Vec3f& position) const;

        void destroyMarkers(std::vector<Marker>& markers);
        void forwardMouseEvents(MyGUI::Widget* widget);
        void resetDoorMarkers();
        void addDoorMarkers();
        void updateMagicMarkers();
        void placeMarkers();
        void placePlayerArrow();

        /// Where a point in screen coordinates is in the picture, from 0 to 1 across and down.
        bool pointToPicture(MyGUI::IntPoint point, osg::Vec2f& picture) const;
        /// Where a point in screen coordinates shows on level ground at a height.
        bool pointToGround(MyGUI::IntPoint point, float height, osg::Vec3f& ground) const;
        /// Where a point in screen coordinates shows on the land.
        bool pointToLand(MyGUI::IntPoint point, osg::Vec3f& land) const;
        void onMousePressed(MyGUI::Widget* sender, int left, int top, MyGUI::MouseButton id);
        void onMouseDrag(MyGUI::Widget* sender, int left, int top, MyGUI::MouseButton id);
        void onDoubleClicked(MyGUI::Widget* sender);
        void onMouseWheel(MyGUI::Widget* sender, int rel);
        void onCustomMarkerDoubleClicked(MyGUI::Widget* sender);

        MWRender::LocalMap* mLocalMapRender;
        CustomMarkerCollection& mCustomMarkers;
        std::unique_ptr<MWRender::Map3D> mMap3D;
        std::unique_ptr<MyGUI::ITexture> mTexture;
        MyGUI::ImageBox* mImage;
        MyGUI::ImageBox* mPlayerArrow;
        bool mDrawing = false;
        bool mFogOfWar = true;

        // The point on the ground that the view looks at, and how far it is zoomed in.
        osg::Vec3f mFocus;
        float mZoom = 1.f;
        // Indoors, everything above this height is cut away.
        std::optional<float> mCeiling;
        // The cell the player was in when the view last drew.
        const MWWorld::CellStore* mCell = nullptr;
        MyGUI::IntPoint mLastDragPosition;
        // The height of the land the drag started on.
        float mDragHeight = 0.f;

        // The fog of war image. Outdoors, it covers a square of cells around the cell at its middle.
        osg::ref_ptr<osg::Image> mFogImage;
        osg::Vec3f mFogToU;
        osg::Vec3f mFogToV;
        std::optional<std::pair<int, int>> mFogMiddle;
        float mUpdateTimer = 0.f;

        // The door markers of each cell. Indoors, they are all under 0, 0.
        std::map<std::pair<int, int>, std::vector<Marker>> mDoorMarkers;
        // The worldspace, or the interior, the door markers are in.
        ESM::RefId mDoorSpace;
        std::vector<Marker> mCustomMarkerWidgets;
        std::vector<Marker> mMagicMarkers;
    };
}

#endif
