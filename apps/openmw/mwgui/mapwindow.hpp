#ifndef MWGUI_MAPWINDOW_H
#define MWGUI_MAPWINDOW_H

#include <cstdint>
#include <memory>
#include <optional>

#include <osg/Image>
#include <osg/Vec2f>
#include <osg/Vec3f>

#include <MyGUI_ITexture.h>

#include "windowpinnablebase.hpp"

#include <components/esm3/custommarkerstate.hpp>
#include <components/misc/constants.hpp>

namespace MWRender
{
    class GlobalMap;
    class LocalMap;
    class Map3D;
}

namespace ESM
{
    class ESMReader;
    class ESMWriter;
}

namespace MWWorld
{
    class Cell;
    class CellStore;
}

namespace Loading
{
    class Listener;
}

namespace SceneUtil
{
    class WorkQueue;
}

namespace MWGui
{
    class MarkerWidget;

    class CustomMarkerCollection
    {
    public:
        void addMarker(const ESM::CustomMarker& marker, bool triggerEvent = true);
        void deleteMarker(const ESM::CustomMarker& marker);
        void updateMarker(const ESM::CustomMarker& marker, const std::string& newNote);

        void clear();

        size_t size() const;

        typedef std::multimap<ESM::RefId, ESM::CustomMarker> ContainerType;

        typedef std::pair<ContainerType::const_iterator, ContainerType::const_iterator> RangeType;

        ContainerType::const_iterator begin() const;
        ContainerType::const_iterator end() const;

        RangeType getMarkers(const ESM::RefId& cellId) const;

        typedef MyGUI::delegates::MultiDelegate<> EventHandle_Void;
        EventHandle_Void eventMarkersChanged;

    private:
        ContainerType mMarkers;
    };

    class LocalMapBase
    {
    public:
        LocalMapBase(CustomMarkerCollection& markers, MWRender::LocalMap* localMapRender, bool fogOfWarEnabled);
        virtual ~LocalMapBase();
        void init(MyGUI::ScrollView* widget, MyGUI::ImageBox* compass, int cellDistance = Constants::CellGridRadius);

        void setActiveCell(const MWWorld::Cell& cell);
        void requestMapRender(const MWWorld::CellStore* cell);
        void setPlayerDir(const float x, const float y);
        void setPlayerPos(int cellX, int cellY, const float nx, const float ny);

        void onFrame(float dt);

        bool toggleFogOfWar();

        struct MarkerUserData
        {
            MarkerUserData(MWRender::LocalMap* map)
                : mLocalMapRender(map)
                , cellX(0)
                , cellY(0)
                , nX(0.f)
                , nY(0.f)
            {
            }

            bool isPositionExplored() const;

            MWRender::LocalMap* mLocalMapRender;
            int cellX;
            int cellY;
            float nX;
            float nY;
            std::vector<std::string> notes;
            std::string caption;
        };

    protected:
        void updateLocalMap();

        MWRender::LocalMap* mLocalMapRender;
        const MWWorld::Cell* mActiveCell = nullptr;
        osg::Vec2f mCurPos; // the position of the player in the world (in cell coords)

        MyGUI::ScrollView* mLocalMap = nullptr;
        MyGUI::ImageBox* mCompass = nullptr;
        float mLocalMapZoom = 1.f;
        bool mFogOfWarToggled = true;
        bool mFogOfWarEnabled;
        bool mNeedDoorMarkersUpdate = false;

        // Stores markers that were placed by a player. May be shared between multiple map views.
        CustomMarkerCollection& mCustomMarkers;

        struct MapEntry
        {
            MapEntry(MyGUI::ImageBox* mapWidget, MyGUI::ImageBox* fogWidget)
                : mMapWidget(mapWidget)
                , mFogWidget(fogWidget)
                , mCellX(0)
                , mCellY(0)
            {
            }

            MyGUI::ImageBox* mMapWidget;
            MyGUI::ImageBox* mFogWidget;
            // nullopt until requested; nullptr if the renderer had no texture for this segment
            std::optional<std::unique_ptr<MyGUI::ITexture>> mMapTexture;
            std::optional<std::unique_ptr<MyGUI::ITexture>> mFogTexture;
            int mCellX;
            int mCellY;
        };
        std::vector<MapEntry> mMaps;

        // Keep track of created marker widgets, just to easily remove them later.
        std::vector<MarkerWidget*> mExteriorDoorMarkerWidgets;
        std::map<std::pair<int, int>, std::vector<MarkerWidget*>> mExteriorDoorsByCell;
        std::vector<MarkerWidget*> mInteriorDoorMarkerWidgets;
        std::vector<MyGUI::Widget*> mMagicMarkerWidgets;
        std::vector<MyGUI::Widget*> mCustomMarkerWidgets;
        std::vector<MarkerWidget*> mDoorMarkersToRecycle;

        std::vector<MarkerWidget*>& currentDoorMarkersWidgets();

        virtual void updateCustomMarkers();

        void applyFogOfWar();

        MyGUI::IntPoint getPosition(int cellX, int cellY, float nx, float ny) const;
        MyGUI::IntPoint getMarkerPosition(float worldX, float worldY, MarkerUserData& markerPos) const;
        MyGUI::IntCoord getMarkerCoordinates(
            float worldX, float worldY, MarkerUserData& markerPos, unsigned short markerSize) const;
        MarkerWidget* createDoorMarker(const std::string& name, float x, float y) const;
        void updateMarkerCoordinates(MyGUI::Widget* widget, unsigned short markerSize) const;

        virtual void notifyPlayerUpdate() {}
        virtual void centerView();

        virtual void customMarkerCreated(MyGUI::Widget* marker) {}
        virtual void doorMarkerCreated(MyGUI::Widget* marker) {}

        void updateRequiredMaps();

        void updateMagicMarkers();
        void addDetectionMarkers(int type);

        void redraw();
        float getWidgetSize() const;

        MWGui::LocalMapBase::MapEntry& addMapEntry();

        MyGUI::IntRect mGrid{ -1, -1, 1, 1 };
        float mMarkerUpdateTimer = 0.f;

        float mLastDirectionX = 0.f;
        float mLastDirectionY = 0.f;

    private:
        void updateDoorMarkers();
    };

    class EditNoteDialog : public MWGui::WindowModal
    {
    public:
        EditNoteDialog();

        void onOpen() override;

        void showDeleteButton(bool show);
        bool getDeleteButtonShown();
        void setText(const std::string& text);
        std::string getText();

        typedef MyGUI::delegates::MultiDelegate<> EventHandle_Void;

        EventHandle_Void eventDeleteClicked;
        EventHandle_Void eventOkClicked;

        ControllerButtons* getControllerButtons() override;

    private:
        void onCancelButtonClicked(MyGUI::Widget* sender);
        void onOkButtonClicked(MyGUI::Widget* sender);
        void onDeleteButtonClicked(MyGUI::Widget* sender);

        MyGUI::TextBox* mTextEdit;
        MyGUI::Button* mOkButton;
        MyGUI::Button* mCancelButton;
        MyGUI::Button* mDeleteButton;

        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg) override;
        size_t mControllerFocus = 0;
    };

    class MapWindow : public MWGui::WindowPinnableBase, public LocalMapBase, public NoDrop
    {
    public:
        MapWindow(CustomMarkerCollection& customMarkers, DragAndDrop* drag, MWRender::LocalMap* localMapRender,
            const std::shared_ptr<SceneUtil::WorkQueue>& workQueue);
        virtual ~MapWindow();

        void setCellName(const std::string& cellName);

        void setAlpha(float alpha) override;
        void setVisible(bool visible) override;

        void renderGlobalMap();

        /// adds the marker to the global map
        /// @param name The ESM::Cell::mName
        void addVisitedLocation(const std::string& name, int x, int y);

        // reveals this cell's map on the global map
        void cellExplored(int x, int y);

        void setGlobalMapPlayerPosition(float worldX, float worldY);
        void setGlobalMapPlayerDir(const float x, const float y);

        void ensureGlobalMapLoaded();

        void onOpen() override;
        void onClose() override;

        void onFrame(float dt) override;

        void updateCustomMarkers() override;

        /// Clear all savegame-specific data
        void clear() override;

        void write(ESM::ESMWriter& writer, Loading::Listener& progress);
        void readRecord(ESM::ESMReader& reader, uint32_t type);

        void asyncPrepareSaveMap();

        std::string_view getWindowIdForLua() const override { return "Map"; }

    protected:
        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg) override;
        bool onControllerThumbstickEvent(const SDL_ControllerAxisEvent& arg) override;
        void setActiveControllerWindow(bool active) override;

    private:
        void onDragStart(MyGUI::Widget* sender, int left, int top, MyGUI::MouseButton id);
        void onMouseDrag(MyGUI::Widget* sender, int left, int top, MyGUI::MouseButton id);
        void onWorldButtonClicked(MyGUI::Widget* sender);
        void onMapDoubleClicked(MyGUI::Widget* sender);
        void onMapZoomed(MyGUI::Widget* sender, int rel);
        /// Zooms the map that shows by a factor, within its limits, keeping a point of the view still. The point is
        /// from the view's top left corner. Returns false when the zoom was already at its limit.
        bool zoomMap(float factor, MyGUI::IntPoint focus);
        /// Zooms in or out by a step, for the d-pad.
        void zoomWithController(bool zoomIn);
        void zoomAround(float speedDiff, MyGUI::IntPoint cursor);
        void updateGlobalMap();
        void onCustomMarkerDoubleClicked(MyGUI::Widget* sender);
        void onNoteEditOk();
        void onNoteEditDelete();
        void onNoteEditDeleteConfirm();
        void onNoteDoubleClicked(MyGUI::Widget* sender);
        void onChangeScrollWindowCoord(MyGUI::Widget* sender);
        void globalMapUpdatePlayer();
        void setGlobalMapMarkerTooltip(MyGUI::Widget* widget, int x, int y);
        float getMarkerSize(size_t agregatedWeight) const;
        void resizeGlobalMap();
        void fitGlobalMapToView();
        void shiftMap(int dx, int dy);
        void panLocalMapWithStick(float dt);
        void updateWorldToggleCaptions();
        void layoutControllerGuide();
        void drawControllerTabs();
        void worldPosToGlobalMapImageSpace(float x, float z, float& imageX, float& imageY) const;
        MyGUI::IntCoord createMarkerCoords(float x, float y, float agregatedWeight) const;
        MyGUI::Widget* createMarker(const std::string& name, float x, float y, float agregatedWeight);

        /// The marker that would go at a point on the local map, in screen coordinates, without a note.
        ESM::CustomMarker getMarkerAt(MyGUI::IntPoint point) const;
        /// Where the player's arrow is, from the local map's top left corner.
        MyGUI::IntPoint getPlayerOnMap() const;
        /// Where the crosshair is on the local map, in screen coordinates. While it is hidden, it is on the player.
        MyGUI::IntPoint getCrosshairPosition() const;
        /// Moves the crosshair with the left stick, showing it if it was hidden.
        void moveCrosshairWithStick(float dt);
        /// Shows the crosshair and its button hint on the local map, and finds the pin under the crosshair.
        void updateCrosshair();
        /// Places a pin at the crosshair, or removes the pin under it.
        void togglePinAtCrosshair();
        /// Draws the pins that are in exterior cells on the global map, again.
        void createGlobalMapPins();
        /// Moves the global map's pins to where their positions are at the global map's zoom.
        void placeGlobalMapPins();
        /// Switches the local map between the flat map and the 3D view.
        void setMap3DShown(bool shown);
        /// Shows the 3D view and starts drawing it, pointed at the player, with the fog of war as it is now.
        void startMap3D();
        /// Hides the 3D view and stops drawing it, so it costs nothing while the map is closed. The local map stays
        /// set to the 3D view, for the next time it opens.
        void stopMap3D();
        /// Pans the 3D view with the right stick, and draws it from its new place.
        void updateMap3D(float dt);
        /// Points the 3D view at the player.
        void centerMap3D();
        /// Makes the 3D view's fog of war again when the view has moved far, or hides it when it is turned off.
        void updateMap3DFog(const MWWorld::CellStore& cell);
        /// A marker's size in the 3D view, from a size on the canvas 720 units tall that the hints use.
        int getMap3DMarkerSize(int canvasSize) const;
        /// The height of the ground at a place outdoors, or the player's height indoors.
        float getMap3DGroundHeight(float x, float y) const;
        /// Draws the pins on the 3D view again, as when they change.
        void createMap3DPins();
        /// Draws the markers of detected creatures, keys and enchanted items, and of the Mark spell's place.
        void createMap3DMagicMarkers();
        /// Outdoors, adds the doors of a few more of the visited cells near the view, nearest first. Looking up a
        /// cell's doors can load it, so a few each frame keeps the view smooth. Indoors, adds the cell's doors.
        void addMap3DDoors();
        /// Moves the 3D view's markers to where their places show in the picture.
        void placeMap3DMarkers();
        /// The pin that X places in the 3D view, at the point the view looks at.
        ESM::CustomMarker getMap3DPinAtCenter() const;
        /// Whether the fog of war leaves a place in the 3D view to see.
        bool isMap3DExplored(const osg::Vec3f& position) const;

        MyGUI::ScrollView* mGlobalMap;
        std::unique_ptr<MyGUI::ITexture> mGlobalMapTexture;
        std::unique_ptr<MyGUI::ITexture> mGlobalMapOverlayTexture;
        MyGUI::ImageBox* mGlobalMapImage;
        MyGUI::ImageBox* mGlobalMapOverlay;
        MyGUI::ImageBox* mPlayerArrowLocal;
        MyGUI::ImageBox* mPlayerArrowGlobal;
        MyGUI::Button* mButton;
        MyGUI::Widget* mControllerGuide;
        // The Local and World tabs in controller menus mode, below the tab bar
        MyGUI::Widget* mControllerTabs = nullptr;
        // Where the tabs end, on the canvas 720 units tall that the tab bar uses
        static constexpr int sTabsBottom = 95;
        MyGUI::IntPoint mLastDragPos;
        // The part of a pixel that the stick moved the local map by, carried over to the next frame.
        MyGUI::FloatPoint mStickPanRemainder;

        // In controller menus mode, a crosshair on the local map, which the left stick moves. X places a pin there, or
        // removes the pin under it, and a hint above the others says which. The crosshair is hidden until the left
        // stick moves, and X then works on the player's position.
        MyGUI::Widget* mCrosshair = nullptr;
        bool mCrosshairShown = false;
        // Where the crosshair is, from the local map's top left corner.
        MyGUI::IntPoint mCrosshairOnMap;
        // The part of a pixel that the stick moved the crosshair by, carried over to the next frame.
        MyGUI::FloatPoint mCrosshairRemainder;
        MyGUI::Widget* mGuideMark = nullptr;
        // The hint that the d-pad zooms, on both maps.
        MyGUI::Widget* mGuideZoom = nullptr;
        MyGUI::TextBox* mGuideMarkText = nullptr;
        // Whether the pin hint says X removes a pin, rather than places one.
        bool mGuideMarkRemoves = false;
        // The pin under the crosshair, one of mCustomMarkerWidgets, or nullptr.
        MyGUI::Widget* mPinAtCrosshair = nullptr;

        // The 3D view of the local map, in controller menus mode. Clicking the right stick switches to it from the
        // flat map. The right stick pans it, the d-pad zooms it and Y points it at the player again.
        std::unique_ptr<MWRender::Map3D> mMap3D;
        std::unique_ptr<MyGUI::ITexture> mMap3DTexture;
        MyGUI::ImageBox* mMap3DImage = nullptr;
        // The player's arrow, on top of the 3D view.
        MyGUI::ImageBox* mMap3DPlayer = nullptr;
        // Whether the local map is set to the 3D view rather than the flat map.
        bool mMap3DShown = false;
        // The point on the ground that the 3D view looks at, and how far away the camera is, in world units.
        osg::Vec3f mMap3DFocus;
        float mMap3DDistance = 11718.75f;
        // Rooms are small, so indoors the view has a zoom of its own.
        float mMap3DInteriorDistance = 2000.f;
        // A marker on the 3D view: its widget, a child of the view's image, and its place in the world.
        struct Map3DMarker
        {
            MyGUI::Widget* mWidget;
            osg::Vec3f mPosition;
        };
        std::vector<Map3DMarker> mMap3DPins;
        std::vector<Map3DMarker> mMap3DDoors;
        std::vector<Map3DMarker> mMap3DMagicMarkers;
        // The cell, or the worldspace outdoors, that the door markers belong to, and the exterior cells whose doors
        // they include.
        ESM::RefId mMap3DDoorsSpace;
        std::set<std::pair<int, int>> mMap3DDoorCells;

        // Whether the 3D view shows the fog of war, and the cell in the middle of the square of cells it has the fog
        // of. Without a cell, the fog is made again on the next frame.
        bool mMap3DFogShown = false;
        std::optional<std::pair<int, int>> mMap3DFogCenter;
        // The fog of war the 3D view shows, as LocalMap::FogImage describes it.
        osg::ref_ptr<osg::Image> mMap3DFogImage;
        osg::Vec3f mMap3DFogToU;
        osg::Vec3f mMap3DFogToV;
        // Indoors, the height above which the 3D view cuts everything away.
        std::optional<float> mMap3DCeiling;

        // The pins in exterior cells, drawn on the global map, with their positions in the world.
        std::vector<std::pair<MyGUI::Widget*, osg::Vec2f>> mGlobalMapPins;

        MyGUI::IntCoord mLastScrollWindowCoordinates;

        // Markers on global map
        typedef std::pair<int, int> CellId;
        std::set<CellId> mMarkers;

        MyGUI::Button* mEventBoxGlobal;
        MyGUI::Button* mEventBoxLocal;

        float mGlobalMapZoom = 1.0f;
        std::unique_ptr<MWRender::GlobalMap> mGlobalMapRender;

        struct MapMarkerType
        {
            osg::Vec2f position;
            MyGUI::Widget* widget = nullptr;

            bool operator<(const MapMarkerType& right) const { return widget < right.widget; }
        };

        std::map<std::string, MapMarkerType> mGlobalMapMarkersByName;
        std::map<MapMarkerType, std::vector<MapMarkerType>> mGlobalMapMarkers;

        EditNoteDialog mEditNoteDialog;
        ESM::CustomMarker mEditingMarker;

        void onPinToggled() override;
        void onTitleDoubleClicked() override;

        void doorMarkerCreated(MyGUI::Widget* marker) override;
        void customMarkerCreated(MyGUI::Widget* marker) override;

        void notifyPlayerUpdate() override;

        void centerView() override;
    };
}
#endif
