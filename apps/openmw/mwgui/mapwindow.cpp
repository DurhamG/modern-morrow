#include "mapwindow.hpp"

#include <MyGUI_Button.h>
#include <MyGUI_FactoryManager.h>
#include <MyGUI_Gui.h>
#include <MyGUI_ImageBox.h>
#include <MyGUI_InputManager.h>
#include <MyGUI_LayerManager.h>
#include <MyGUI_LanguageManager.h>
#include <MyGUI_RenderManager.h>
#include <MyGUI_RotatingSkin.h>
#include <MyGUI_ScrollView.h>
#include <MyGUI_TextIterator.h>
#include <MyGUI_Window.h>

#include <components/esm3/esmwriter.hpp>
#include <components/esm3/globalmap.hpp>
#include <components/settings/values.hpp>
#include <components/widgets/box.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/cellstore.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/player.hpp"
#include "../mwworld/worldmodel.hpp"

#include "../mwrender/globalmap.hpp"
#include "../mwrender/localmap.hpp"
#include "../mwrender/map3d.hpp"
#include "../mwrender/renderingmanager.hpp"

#include "confirmationdialog.hpp"
#include "controllerscreen.hpp"

#include <numeric>

namespace
{

    constexpr int cellSize = Constants::CellSizeInUnits;
    constexpr float speed = 1.08f; // the zoom speed, it should be greater than 1

    enum LocalMapWidgetDepth
    {
        Local_MarkerAboveFogLayer = 0,
        Local_CompassLayer = 1,
        Local_FogLayer = 2,
        Local_MarkerLayer = 3,
        Local_MapLayer = 4
    };

    enum GlobalMapWidgetDepth
    {
        Global_CompassLayer = 0,
        Global_MarkerLayer = 1,
        Global_ExploreOverlayLayer = 2,
        Global_MapLayer = 3
    };

    // How far a stick moves something on the map, from -1 to 1, given how far the stick is pushed. Squaring the
    // deflection gives finer control near the centre of the stick.
    float stickResponse(float value)
    {
        constexpr float deadzone = 0.15f;
        const float magnitude = std::abs(value);
        if (magnitude < deadzone)
            return 0.f;
        const float scaled = (magnitude - deadzone) / (1.f - deadzone);
        return std::copysign(scaled * scaled, value);
    }
}

namespace MWGui
{
    /// @brief A widget that changes its color when hovered.
    class MarkerWidget final : public MyGUI::Widget
    {
        MYGUI_RTTI_DERIVED(MarkerWidget)

    public:
        void setNormalColour(const MyGUI::Colour& colour)
        {
            mNormalColour = colour;
            setColour(colour);
        }

        void setHoverColour(const MyGUI::Colour& colour) { mHoverColour = colour; }

    private:
        MyGUI::Colour mNormalColour;
        MyGUI::Colour mHoverColour;

        void onMouseLostFocus(MyGUI::Widget* /*newWidget*/) override { setColour(mNormalColour); }

        void onMouseSetFocus(MyGUI::Widget* /*oldWidget*/) override { setColour(mHoverColour); }
    };

    MyGUI::IntRect createRect(const MyGUI::IntPoint& center, int radius)
    {
        return { center.left - radius, center.top - radius, center.left + radius, center.top + radius };
    }

    int getLocalViewingDistance()
    {
        return MWRender::getLocalMapCellDistance();
    }

    ESM::RefId getCellIdInWorldSpace(const MWWorld::Cell& cell, int x, int y)
    {
        if (cell.isExterior())
            return ESM::Cell::generateIdForCell(true, {}, x, y);
        return cell.getId();
    }

    void setCanvasSize(MyGUI::ScrollView* scrollView, const MyGUI::IntRect& grid, int widgetSize)
    {
        scrollView->setCanvasSize(widgetSize * (grid.width() + 1), widgetSize * (grid.height() + 1));
    }

    void CustomMarkerCollection::addMarker(const ESM::CustomMarker& marker, bool triggerEvent)
    {
        mMarkers.insert(std::make_pair(marker.mCell, marker));
        if (triggerEvent)
            eventMarkersChanged();
    }

    void CustomMarkerCollection::deleteMarker(const ESM::CustomMarker& marker)
    {
        std::pair<ContainerType::iterator, ContainerType::iterator> range = mMarkers.equal_range(marker.mCell);

        for (ContainerType::iterator it = range.first; it != range.second; ++it)
        {
            if (it->second == marker)
            {
                mMarkers.erase(it);
                eventMarkersChanged();
                return;
            }
        }
        throw std::runtime_error("can't find marker to delete");
    }

    void CustomMarkerCollection::updateMarker(const ESM::CustomMarker& marker, const std::string& newNote)
    {
        std::pair<ContainerType::iterator, ContainerType::iterator> range = mMarkers.equal_range(marker.mCell);

        for (ContainerType::iterator it = range.first; it != range.second; ++it)
        {
            if (it->second == marker)
            {
                it->second.mNote = newNote;
                eventMarkersChanged();
                return;
            }
        }
        throw std::runtime_error("can't find marker to update");
    }

    void CustomMarkerCollection::clear()
    {
        mMarkers.clear();
        eventMarkersChanged();
    }

    CustomMarkerCollection::ContainerType::const_iterator CustomMarkerCollection::begin() const
    {
        return mMarkers.begin();
    }

    CustomMarkerCollection::ContainerType::const_iterator CustomMarkerCollection::end() const
    {
        return mMarkers.end();
    }

    CustomMarkerCollection::RangeType CustomMarkerCollection::getMarkers(const ESM::RefId& cellId) const
    {
        return mMarkers.equal_range(cellId);
    }

    size_t CustomMarkerCollection::size() const
    {
        return mMarkers.size();
    }

    // ------------------------------------------------------

    LocalMapBase::LocalMapBase(
        CustomMarkerCollection& markers, MWRender::LocalMap* localMapRender, bool fogOfWarEnabled)
        : mLocalMapRender(localMapRender)
        , mFogOfWarEnabled(fogOfWarEnabled)
        , mCustomMarkers(markers)
    {
        mCustomMarkers.eventMarkersChanged += MyGUI::newDelegate(this, &LocalMapBase::updateCustomMarkers);
    }

    LocalMapBase::~LocalMapBase()
    {
        mCustomMarkers.eventMarkersChanged -= MyGUI::newDelegate(this, &LocalMapBase::updateCustomMarkers);
    }

    MWGui::LocalMapBase::MapEntry& LocalMapBase::addMapEntry()
    {
        const int mapWidgetSize = static_cast<int>(getWidgetSize());
        MyGUI::ImageBox* map = mLocalMap->createWidget<MyGUI::ImageBox>(
            "ImageBox", MyGUI::IntCoord(0, 0, mapWidgetSize, mapWidgetSize), MyGUI::Align::Top | MyGUI::Align::Left);
        map->setDepth(Local_MapLayer);

        MyGUI::ImageBox* fog = mLocalMap->createWidget<MyGUI::ImageBox>(
            "ImageBox", MyGUI::IntCoord(0, 0, mapWidgetSize, mapWidgetSize), MyGUI::Align::Top | MyGUI::Align::Left);
        fog->setDepth(Local_FogLayer);
        fog->setColour(MyGUI::Colour(0, 0, 0));

        map->setNeedMouseFocus(false);
        fog->setNeedMouseFocus(false);

        return mMaps.emplace_back(map, fog);
    }

    void LocalMapBase::init(MyGUI::ScrollView* widget, MyGUI::ImageBox* compass, int cellDistance)
    {
        mLocalMap = widget;
        mCompass = compass;
        mGrid = createRect({ 0, 0 }, cellDistance);

        const int mapWidgetSize = static_cast<int>(getWidgetSize());
        setCanvasSize(mLocalMap, mGrid, mapWidgetSize);

        mCompass->setDepth(Local_CompassLayer);
        mCompass->setNeedMouseFocus(false);

        int numCells = (mGrid.width() + 1) * (mGrid.height() + 1);
        for (int i = 0; i < numCells; ++i)
            addMapEntry();
    }

    bool LocalMapBase::toggleFogOfWar()
    {
        mFogOfWarToggled = !mFogOfWarToggled;
        applyFogOfWar();
        return mFogOfWarToggled;
    }

    void LocalMapBase::applyFogOfWar()
    {
        if (!mFogOfWarToggled || !mFogOfWarEnabled)
        {
            for (auto& entry : mMaps)
            {
                entry.mFogWidget->setImageTexture({});
                entry.mFogTexture.reset();
            }
        }

        redraw();
    }

    MyGUI::IntPoint LocalMapBase::getPosition(int cellX, int cellY, float nX, float nY) const
    {
        // normalized cell coordinates
        auto mapWidgetSize = getWidgetSize();
        return MyGUI::IntPoint(static_cast<int>(std::round((nX + cellX - mGrid.left) * mapWidgetSize)),
            static_cast<int>(std::round((nY - cellY + mGrid.bottom) * mapWidgetSize)));
    }

    MyGUI::IntPoint LocalMapBase::getMarkerPosition(float worldX, float worldY, MarkerUserData& markerPos) const
    {
        osg::Vec2i cellIndex;
        // normalized cell coordinates
        float nX, nY;

        if (mActiveCell->isExterior())
        {
            ESM::ExteriorCellLocation cellPos = ESM::positionToExteriorCellLocation(worldX, worldY);
            cellIndex.x() = cellPos.mX;
            cellIndex.y() = cellPos.mY;

            nX = (worldX - cellSize * cellIndex.x()) / cellSize;
            // Image space is -Y up, cells are Y up
            nY = 1 - (worldY - cellSize * cellIndex.y()) / cellSize;
        }
        else
            mLocalMapRender->worldToInteriorMapPosition({ worldX, worldY }, nX, nY, cellIndex.x(), cellIndex.y());

        markerPos.cellX = cellIndex.x();
        markerPos.cellY = cellIndex.y();
        markerPos.nX = nX;
        markerPos.nY = nY;
        return getPosition(markerPos.cellX, markerPos.cellY, markerPos.nX, markerPos.nY);
    }

    MyGUI::IntCoord LocalMapBase::getMarkerCoordinates(
        float worldX, float worldY, MarkerUserData& markerPos, unsigned short markerSize) const
    {
        int halfMarkerSize = markerSize / 2;
        auto position = getMarkerPosition(worldX, worldY, markerPos);
        return MyGUI::IntCoord(position.left - halfMarkerSize, position.top - halfMarkerSize, markerSize, markerSize);
    }

    MarkerWidget* LocalMapBase::createDoorMarker(const std::string& name, float x, float y) const
    {
        MarkerUserData data(mLocalMapRender);
        data.caption = name;
        MarkerWidget* markerWidget = mLocalMap->createWidget<MarkerWidget>(
            "MarkerButton", getMarkerCoordinates(x, y, data, 8), MyGUI::Align::Default);
        markerWidget->setNormalColour(
            MyGUI::Colour::parse(MyGUI::LanguageManager::getInstance().replaceTags("#{fontcolour=normal}")));
        markerWidget->setHoverColour(
            MyGUI::Colour::parse(MyGUI::LanguageManager::getInstance().replaceTags("#{fontcolour=normal_over}")));
        markerWidget->setDepth(Local_MarkerLayer);
        markerWidget->setNeedMouseFocus(true);
        // Used by tooltips to not show the tooltip if marker is hidden by fog of war
        markerWidget->setUserString("ToolTipType", "MapMarker");

        markerWidget->setUserData(data);
        return markerWidget;
    }

    void LocalMapBase::centerView()
    {
        MyGUI::IntPoint pos = mCompass->getPosition() + MyGUI::IntPoint{ 16, 16 };
        MyGUI::IntSize viewsize = mLocalMap->getSize();
        MyGUI::IntPoint viewOffset((viewsize.width / 2) - pos.left, (viewsize.height / 2) - pos.top);
        mLocalMap->setViewOffset(viewOffset);
    }

    void LocalMapBase::updateMarkerCoordinates(MyGUI::Widget* widget, unsigned short markerSize) const
    {
        MarkerUserData& markerPos(*widget->getUserData<MarkerUserData>());
        auto position = getPosition(markerPos.cellX, markerPos.cellY, markerPos.nX, markerPos.nY);
        MyGUI::IntCoord coord(position.left - markerSize / 2, position.top - markerSize / 2, markerSize, markerSize);
        widget->setCoord(coord);
    }

    std::vector<MarkerWidget*>& LocalMapBase::currentDoorMarkersWidgets()
    {
        return mActiveCell->isExterior() ? mExteriorDoorMarkerWidgets : mInteriorDoorMarkerWidgets;
    }

    void LocalMapBase::updateCustomMarkers()
    {
        for (MyGUI::Widget* widget : mCustomMarkerWidgets)
            MyGUI::Gui::getInstance().destroyWidget(widget);
        mCustomMarkerWidgets.clear();
        if (!mActiveCell)
            return;
        auto updateMarkers = [this](CustomMarkerCollection::RangeType markers) {
            for (auto it = markers.first; it != markers.second; ++it)
            {
                const ESM::CustomMarker& marker = it->second;
                MarkerUserData markerPos(mLocalMapRender);
                MarkerWidget* markerWidget = mLocalMap->createWidget<MarkerWidget>("CustomMarkerButton",
                    getMarkerCoordinates(marker.mWorldX, marker.mWorldY, markerPos, 16), MyGUI::Align::Default);
                markerWidget->setDepth(Local_MarkerAboveFogLayer);
                markerWidget->setUserString("ToolTipType", "Layout");
                markerWidget->setUserString("ToolTipLayout", "TextToolTipOneLine");
                markerWidget->setUserString("Caption_TextOneLine", MyGUI::TextIterator::toTagsString(marker.mNote));
                markerWidget->setNormalColour(MyGUI::Colour(0.6f, 0.6f, 0.6f));
                markerWidget->setHoverColour(MyGUI::Colour(1.0f, 1.0f, 1.0f));
                markerWidget->setUserData(marker);
                markerWidget->setNeedMouseFocus(true);
                customMarkerCreated(markerWidget);
                mCustomMarkerWidgets.push_back(markerWidget);
            }
        };
        if (mActiveCell->isExterior())
        {
            for (int x = mGrid.left; x <= mGrid.right; ++x)
            {
                for (int y = mGrid.top; y <= mGrid.bottom; ++y)
                {
                    ESM::RefId cellRefId = getCellIdInWorldSpace(*mActiveCell, x, y);
                    updateMarkers(mCustomMarkers.getMarkers(cellRefId));
                }
            }
        }
        else
            updateMarkers(mCustomMarkers.getMarkers(mActiveCell->getId()));

        redraw();
    }

    void LocalMapBase::setActiveCell(const MWWorld::Cell& cell)
    {
        if (&cell == mActiveCell)
            return; // don't do anything if we're still in the same cell

        // Remove all interior door markers
        mDoorMarkersToRecycle.insert(
            mDoorMarkersToRecycle.end(), mInteriorDoorMarkerWidgets.begin(), mInteriorDoorMarkerWidgets.end());
        for (MarkerWidget* widget : mInteriorDoorMarkerWidgets)
            widget->setVisible(false);
        mInteriorDoorMarkerWidgets.clear();

        const int x = cell.getGridX();
        const int y = cell.getGridY();

        const MyGUI::IntRect prevGrid = mGrid;

        if (cell.isExterior())
        {
            std::optional<MyGUI::IntRect> previousActiveGrid;
            if (mActiveCell && mActiveCell->isExterior())
                previousActiveGrid
                    = createRect({ mActiveCell->getGridX(), mActiveCell->getGridY() }, Constants::CellGridRadius);
            mGrid = createRect({ x, y }, getLocalViewingDistance());
            const MyGUI::IntRect activeGrid = createRect({ x, y }, Constants::CellGridRadius);

            mExteriorDoorMarkerWidgets.clear();
            for (auto it = mExteriorDoorsByCell.begin(); it != mExteriorDoorsByCell.end();)
            {
                const auto& [coord, doors] = *it;
                const MyGUI::IntPoint pos(coord.first, coord.second);
                // Remove markers that fall outside the rendered map and ones that are new to the active grid.
                // Scripts can enable/disable doors, requiring us to update the markers. Morrowind.exe only updates
                // markers when a cell is added to the active grid.
                if (!mGrid.inside(pos)
                    || (previousActiveGrid && !previousActiveGrid->inside(pos) && activeGrid.inside(pos)))
                {
                    mDoorMarkersToRecycle.insert(mDoorMarkersToRecycle.end(), doors.begin(), doors.end());
                    for (MarkerWidget* widget : doors)
                        widget->setVisible(false);
                    it = mExteriorDoorsByCell.erase(it);
                }
                else
                {
                    mExteriorDoorMarkerWidgets.insert(mExteriorDoorMarkerWidgets.end(), doors.begin(), doors.end());
                    ++it;
                }
            }

            if (mActiveCell && mActiveCell->isExterior())
            {
                for (int cx = prevGrid.left; cx <= prevGrid.right; ++cx)
                {
                    for (int cy = prevGrid.top; cy <= prevGrid.bottom; ++cy)
                    {
                        if (!mGrid.inside({ cx, cy }))
                            mLocalMapRender->removeExteriorCell(cx, cy);
                    }
                }
            }
        }
        else
        {
            mGrid = mLocalMapRender->getInteriorGrid();
            // Remove all exterior door markers
            mDoorMarkersToRecycle.insert(
                mDoorMarkersToRecycle.end(), mExteriorDoorMarkerWidgets.begin(), mExteriorDoorMarkerWidgets.end());
            for (MarkerWidget* widget : mExteriorDoorMarkerWidgets)
                widget->setVisible(false);
            mExteriorDoorMarkerWidgets.clear();
            mExteriorDoorsByCell.clear();
        }

        mActiveCell = &cell;

        constexpr auto resetEntry = [](MapEntry& entry, bool visible, const MyGUI::IntPoint* position) {
            entry.mMapWidget->setVisible(visible);
            entry.mFogWidget->setVisible(visible);
            if (position)
            {
                entry.mMapWidget->setPosition(*position);
                entry.mFogWidget->setPosition(*position);
            }
            entry.mMapWidget->setRenderItemTexture(nullptr);
            entry.mFogWidget->setRenderItemTexture(nullptr);
            entry.mMapTexture.reset();
            entry.mFogTexture.reset();
        };

        std::size_t usedEntries = 0;
        for (int cx = mGrid.left; cx <= mGrid.right; ++cx)
        {
            for (int cy = mGrid.top; cy <= mGrid.bottom; ++cy)
            {
                MapEntry& entry = usedEntries < mMaps.size() ? mMaps[usedEntries] : addMapEntry();
                entry.mCellX = cx;
                entry.mCellY = cy;
                MyGUI::IntPoint position = getPosition(cx, cy, 0, 0);
                resetEntry(entry, true, &position);
                ++usedEntries;
            }
        }
        for (std::size_t i = usedEntries; i < mMaps.size(); ++i)
        {
            resetEntry(mMaps[i], false, nullptr);
        }

        if (prevGrid.width() != mGrid.width() || prevGrid.height() != mGrid.height())
            updateLocalMap();

        // Delay the door markers update until scripts have been given a chance to run.
        // If we don't do this, door markers that should be disabled will still appear on the map.
        mNeedDoorMarkersUpdate = true;

        for (MyGUI::Widget* widget : currentDoorMarkersWidgets())
            updateMarkerCoordinates(widget, 8);

        updateMagicMarkers();
        updateCustomMarkers();
    }

    void LocalMapBase::requestMapRender(const MWWorld::CellStore* cell)
    {
        mLocalMapRender->requestMap(cell);
    }

    void LocalMapBase::redraw()
    {
        // Redraw children in proper order
        mLocalMap->getParent()->_updateChilds();
    }

    float LocalMapBase::getWidgetSize() const
    {
        return mLocalMapZoom * Settings::map().mLocalMapWidgetSize;
    }

    void LocalMapBase::setPlayerPos(int cellX, int cellY, const float nx, const float ny)
    {
        MyGUI::IntPoint pos = getPosition(cellX, cellY, nx, ny) - MyGUI::IntPoint{ 16, 16 };

        if (pos != mCompass->getPosition())
        {
            notifyPlayerUpdate();

            mCompass->setPosition(pos);
        }
        osg::Vec2f curPos((cellX + nx) * cellSize, (cellY + 1 - ny) * cellSize);
        if ((curPos - mCurPos).length2() > 0.001)
        {
            mCurPos = curPos;
            centerView();
        }
    }

    void LocalMapBase::setPlayerDir(const float x, const float y)
    {
        if (x == mLastDirectionX && y == mLastDirectionY)
            return;

        notifyPlayerUpdate();

        MyGUI::ISubWidget* main = mCompass->getSubWidgetMain();
        MyGUI::RotatingSkin* rotatingSubskin = main->castType<MyGUI::RotatingSkin>();
        rotatingSubskin->setCenter(MyGUI::IntPoint(16, 16));
        float angle = std::atan2(x, y);
        rotatingSubskin->setAngle(angle);

        mLastDirectionX = x;
        mLastDirectionY = y;
    }

    void LocalMapBase::addDetectionMarkers(int type)
    {
        std::vector<MWWorld::Ptr> markers;
        MWBase::World* world = MWBase::Environment::get().getWorld();
        world->listDetectedReferences(world->getPlayerPtr(), markers, MWBase::World::DetectionType(type));
        if (markers.empty())
            return;

        std::string_view markerTexture;
        if (type == MWBase::World::Detect_Creature)
        {
            markerTexture = "textures\\detect_animal_icon.dds";
        }
        if (type == MWBase::World::Detect_Key)
        {
            markerTexture = "textures\\detect_key_icon.dds";
        }
        if (type == MWBase::World::Detect_Enchantment)
        {
            markerTexture = "textures\\detect_enchantment_icon.dds";
        }

        for (const MWWorld::Ptr& ptr : markers)
        {
            const ESM::Position& worldPos = ptr.getRefData().getPosition();
            MarkerUserData markerPos(mLocalMapRender);
            MyGUI::ImageBox* markerWidget = mLocalMap->createWidget<MyGUI::ImageBox>("ImageBox",
                getMarkerCoordinates(worldPos.pos[0], worldPos.pos[1], markerPos, 8), MyGUI::Align::Default);
            markerWidget->setDepth(Local_MarkerAboveFogLayer);
            markerWidget->setImageTexture(markerTexture);
            markerWidget->setImageCoord(MyGUI::IntCoord(0, 0, 8, 8));
            markerWidget->setNeedMouseFocus(false);
            markerWidget->setUserData(markerPos);
            mMagicMarkerWidgets.push_back(markerWidget);
        }
    }

    void LocalMapBase::onFrame(float dt)
    {
        if (mNeedDoorMarkersUpdate)
        {
            updateDoorMarkers();
            mNeedDoorMarkersUpdate = false;
        }

        mMarkerUpdateTimer += dt;

        if (mMarkerUpdateTimer >= 0.25)
        {
            mMarkerUpdateTimer = 0;
            updateMagicMarkers();
        }

        updateRequiredMaps();
    }

    bool widgetCropped(MyGUI::Widget* widget, MyGUI::Widget* cropTo)
    {
        MyGUI::IntRect coord = widget->getAbsoluteRect();
        MyGUI::IntRect croppedCoord = cropTo->getAbsoluteRect();
        return !coord.intersect(croppedCoord);
    }

    void LocalMapBase::updateRequiredMaps()
    {
        bool needRedraw = false;
        for (MapEntry& entry : mMaps)
        {
            if (!entry.mMapWidget->getVisible() || widgetCropped(entry.mMapWidget, mLocalMap))
                continue;

            if (!entry.mMapTexture)
            {
                if (mActiveCell->isExterior())
                    requestMapRender(&MWBase::Environment::get().getWorldModel()->getExterior(
                        ESM::ExteriorCellLocation(entry.mCellX, entry.mCellY, ESM::Cell::sDefaultWorldspaceId)));

                entry.mMapTexture = mLocalMapRender->makeMapGuiTexture(entry.mCellX, entry.mCellY);
                if (*entry.mMapTexture)
                {
                    entry.mMapWidget->setRenderItemTexture(entry.mMapTexture->get());
                    // The widget is Y-down, the RTT image is Y-up, so this UV is inverted
                    entry.mMapWidget->getSubWidgetMain()->_setUVSet(MyGUI::FloatRect(0.f, 1.f, 1.f, 0.f));
                    needRedraw = true;
                }
            }
            if (!entry.mFogTexture && mFogOfWarToggled && mFogOfWarEnabled)
            {
                entry.mFogTexture = mLocalMapRender->makeFogOfWarGuiTexture(entry.mCellX, entry.mCellY);
                if (*entry.mFogTexture)
                {
                    entry.mFogWidget->setRenderItemTexture(entry.mFogTexture->get());
                    // For inexplicable historical reasons the fog texture is Y-down so this UV is *not* inverted
                    entry.mFogWidget->getSubWidgetMain()->_setUVSet(MyGUI::FloatRect(0.f, 0.f, 1.f, 1.f));
                }
                else
                    entry.mFogWidget->setImageTexture("black");
                needRedraw = true;
                // Newly uncovered chunk, make sure to draw door markers right away instead of waiting for a cell
                // transition
                mNeedDoorMarkersUpdate = true;
            }
        }
        if (needRedraw)
            redraw();
    }

    void LocalMapBase::updateDoorMarkers()
    {
        std::vector<MWBase::World::DoorMarker> doors;
        MWBase::World* world = MWBase::Environment::get().getWorld();
        MWWorld::WorldModel* worldModel = MWBase::Environment::get().getWorldModel();

        if (!mActiveCell->isExterior())
        {
            MWWorld::CellStore& cell = worldModel->getInterior(mActiveCell->getNameId());
            world->getDoorMarkers(cell, doors);
        }
        else
        {
            for (MapEntry& entry : mMaps)
            {
                if (!entry.mMapWidget->getVisible() || widgetCropped(entry.mMapWidget, mLocalMap))
                    continue;
                if (mExteriorDoorsByCell.contains({ entry.mCellX, entry.mCellY }))
                    continue;
                ESM::ExteriorCellLocation id(entry.mCellX, entry.mCellY, ESM::Cell::sDefaultWorldspaceId);
                world->getDoorMarkers(worldModel->getExterior(id), doors);
            }
        }

        // Create a widget for each marker
        for (MWBase::World::DoorMarker& marker : doors)
        {
            std::vector<std::string> destNotes;
            CustomMarkerCollection::RangeType markers = mCustomMarkers.getMarkers(marker.dest);
            for (auto iter = markers.first; iter != markers.second; ++iter)
                destNotes.push_back(iter->second.mNote);

            MarkerWidget* markerWidget = nullptr;
            MarkerUserData* data;
            if (mDoorMarkersToRecycle.empty())
            {
                markerWidget = createDoorMarker(marker.name, marker.x, marker.y);
                data = markerWidget->getUserData<MarkerUserData>();
                data->notes = std::move(destNotes);
                doorMarkerCreated(markerWidget);
            }
            else
            {
                markerWidget = mDoorMarkersToRecycle.back();
                mDoorMarkersToRecycle.pop_back();

                data = markerWidget->getUserData<MarkerUserData>();
                data->notes = std::move(destNotes);
                data->caption = marker.name;
                markerWidget->setCoord(getMarkerCoordinates(marker.x, marker.y, *data, 8));
                markerWidget->setVisible(true);
            }

            currentDoorMarkersWidgets().push_back(markerWidget);
            if (mActiveCell->isExterior())
                mExteriorDoorsByCell[{ data->cellX, data->cellY }].push_back(markerWidget);
        }

        for (MyGUI::Widget* widget : currentDoorMarkersWidgets())
            updateMarkerCoordinates(widget, 8);
    }

    void LocalMapBase::updateMagicMarkers()
    {
        // clear all previous markers
        for (MyGUI::Widget* widget : mMagicMarkerWidgets)
            MyGUI::Gui::getInstance().destroyWidget(widget);
        mMagicMarkerWidgets.clear();

        addDetectionMarkers(MWBase::World::Detect_Creature);
        addDetectionMarkers(MWBase::World::Detect_Key);
        addDetectionMarkers(MWBase::World::Detect_Enchantment);

        // Add marker for the spot marked with Mark magic effect
        MWWorld::CellStore* markedCell = nullptr;
        ESM::Position markedPosition;
        MWBase::Environment::get().getWorld()->getPlayer().getMarkedPosition(markedCell, markedPosition);
        if (markedCell && markedCell->getCell()->getWorldSpace() == mActiveCell->getWorldSpace())
        {
            MarkerUserData markerPos(mLocalMapRender);
            MyGUI::ImageBox* markerWidget = mLocalMap->createWidget<MyGUI::ImageBox>("ImageBox",
                getMarkerCoordinates(markedPosition.pos[0], markedPosition.pos[1], markerPos, 8),
                MyGUI::Align::Default);
            markerWidget->setDepth(Local_MarkerAboveFogLayer);
            markerWidget->setImageTexture("textures\\menu_map_smark.dds");
            markerWidget->setNeedMouseFocus(false);
            markerWidget->setUserData(markerPos);
            mMagicMarkerWidgets.push_back(markerWidget);
        }

        redraw();
    }

    void LocalMapBase::updateLocalMap()
    {
        auto mapWidgetSize = getWidgetSize();
        setCanvasSize(mLocalMap, mGrid, static_cast<int>(getWidgetSize()));

        const auto size
            = MyGUI::IntSize(static_cast<int>(std::ceil(mapWidgetSize)), static_cast<int>(std::ceil(mapWidgetSize)));
        for (auto& entry : mMaps)
        {
            if (!entry.mMapWidget->getVisible())
                continue;
            const auto position = getPosition(entry.mCellX, entry.mCellY, 0, 0);
            entry.mMapWidget->setCoord({ position, size });
            entry.mFogWidget->setCoord({ position, size });
        }

        MarkerUserData markerPos(mLocalMapRender);
        for (MyGUI::Widget* widget : currentDoorMarkersWidgets())
            updateMarkerCoordinates(widget, 8);

        for (MyGUI::Widget* widget : mCustomMarkerWidgets)
        {
            const auto& marker = *widget->getUserData<ESM::CustomMarker>();
            widget->setCoord(getMarkerCoordinates(marker.mWorldX, marker.mWorldY, markerPos, 16));
        }

        for (MyGUI::Widget* widget : mMagicMarkerWidgets)
            updateMarkerCoordinates(widget, 8);
    }

    // ------------------------------------------------------------------------------------------
    MapWindow::MapWindow(CustomMarkerCollection& customMarkers, DragAndDrop* drag, MWRender::LocalMap* localMapRender,
        const std::shared_ptr<SceneUtil::WorkQueue>& workQueue)
#ifdef USE_OPENXR
        : WindowPinnableBase("openmw_map_window_vr.layout")
#else
        : WindowPinnableBase("openmw_map_window.layout")
#endif
        , LocalMapBase(customMarkers, localMapRender, true)
        , NoDrop(drag, mMainWidget)
        , mGlobalMap(nullptr)
        , mGlobalMapImage(nullptr)
        , mGlobalMapOverlay(nullptr)
        , mEventBoxGlobal(nullptr)
        , mEventBoxLocal(nullptr)
        , mGlobalMapRender(std::make_unique<MWRender::GlobalMap>(localMapRender->getRoot(), workQueue))
        , mEditNoteDialog()
    {
        [[maybe_unused]] static const bool registered = [] {
            MyGUI::FactoryManager::getInstance().registerFactory<MarkerWidget>("Widget");
            return true;
        }();

        mEditNoteDialog.setVisible(false);
        mEditNoteDialog.eventOkClicked += MyGUI::newDelegate(this, &MapWindow::onNoteEditOk);
        mEditNoteDialog.eventDeleteClicked += MyGUI::newDelegate(this, &MapWindow::onNoteEditDelete);

        setCoord(500, 0, 320, 300);

        getWidget(mLocalMap, "LocalMap");
        getWidget(mGlobalMap, "GlobalMap");
        getWidget(mGlobalMapImage, "GlobalMapImage");
        getWidget(mGlobalMapOverlay, "GlobalMapOverlay");
        getWidget(mPlayerArrowLocal, "CompassLocal");
        getWidget(mPlayerArrowGlobal, "CompassGlobal");

        mPlayerArrowGlobal->setDepth(Global_CompassLayer);
        mPlayerArrowGlobal->setNeedMouseFocus(false);
        mGlobalMapImage->setDepth(Global_MapLayer);
        mGlobalMapOverlay->setDepth(Global_ExploreOverlayLayer);

        mLastScrollWindowCoordinates = mLocalMap->getCoord();
        mLocalMap->eventChangeCoord += MyGUI::newDelegate(this, &MapWindow::onChangeScrollWindowCoord);

        mGlobalMap->setVisible(false);

        getWidget(mButton, "WorldButton");
        mButton->eventMouseButtonClick += MyGUI::newDelegate(this, &MapWindow::onWorldButtonClicked);

        getWidget(mControllerGuide, "ControllerGuide");
        MyGUI::ImageBox* guideImage;
        getWidget(guideImage, "GuideCenterImage");
        guideImage->setImageTexture(
            MWBase::Environment::get().getInputManager()->getControllerButtonIcon(SDL_CONTROLLER_BUTTON_Y));
        mControllerGuide->setVisible(false);

        getWidget(mGuideZoom, "GuideZoom");
        getWidget(guideImage, "GuideZoomImage");
        guideImage->setImageTexture(
            MWBase::Environment::get().getInputManager()->getControllerButtonIcon(SDL_CONTROLLER_BUTTON_DPAD_UP));
        mGuideZoom->setVisible(false);

        getWidget(mGuideMark, "GuideMark");
        getWidget(mGuideMarkText, "GuideMarkText");
        getWidget(guideImage, "GuideMarkImage");
        guideImage->setImageTexture(
            MWBase::Environment::get().getInputManager()->getControllerButtonIcon(SDL_CONTROLLER_BUTTON_X));
        mGuideMark->setVisible(false);
        if (Settings::gui().mControllerMenus)
        {
            // It sits over the local map, beside it in the window, so it stays put as the map pans.
            mCrosshair = mLocalMap->getParent()->createWidget<MyGUI::Widget>(
                "", MyGUI::IntCoord(0, 0, 24, 24), MyGUI::Align::Default);
            mCrosshair->setNeedMouseFocus(false);
            mCrosshair->setVisible(false);
        }

        const bool global = Settings::map().mGlobal;

        updateWorldToggleCaptions();

        getWidget(mEventBoxGlobal, "EventBoxGlobal");
        mEventBoxGlobal->eventMouseDrag += MyGUI::newDelegate(this, &MapWindow::onMouseDrag);
        mEventBoxGlobal->eventMouseButtonPressed += MyGUI::newDelegate(this, &MapWindow::onDragStart);

        const bool allowZooming = Settings::map().mAllowZooming;

        if (allowZooming)
            mEventBoxGlobal->eventMouseWheel += MyGUI::newDelegate(this, &MapWindow::onMapZoomed);
        mEventBoxGlobal->setDepth(Global_ExploreOverlayLayer);

        getWidget(mEventBoxLocal, "EventBoxLocal");
        mEventBoxLocal->eventMouseDrag += MyGUI::newDelegate(this, &MapWindow::onMouseDrag);
        mEventBoxLocal->eventMouseButtonPressed += MyGUI::newDelegate(this, &MapWindow::onDragStart);
        mEventBoxLocal->eventMouseButtonDoubleClick += MyGUI::newDelegate(this, &MapWindow::onMapDoubleClicked);
        if (allowZooming)
            mEventBoxLocal->eventMouseWheel += MyGUI::newDelegate(this, &MapWindow::onMapZoomed);

        LocalMapBase::init(mLocalMap, mPlayerArrowLocal, getLocalViewingDistance());

        mGlobalMap->setVisible(global);
        mLocalMap->setVisible(!global);

    }

    void MapWindow::updateWorldToggleCaptions()
    {
        const bool global = Settings::map().mGlobal;
        mButton->setCaptionWithReplacing(global ? "#{sLocal}" : "#{sWorld}");
        if (mControllerTabs != nullptr && mControllerTabs->getVisible())
            drawControllerTabs();
    }

    void MapWindow::onNoteEditOk()
    {
        if (mEditNoteDialog.getDeleteButtonShown())
            mCustomMarkers.updateMarker(mEditingMarker, mEditNoteDialog.getText());
        else
        {
            mEditingMarker.mNote = mEditNoteDialog.getText();
            mCustomMarkers.addMarker(mEditingMarker);
        }

        mEditNoteDialog.setVisible(false);
    }

    void MapWindow::onNoteEditDelete()
    {
        ConfirmationDialog* confirmation = MWBase::Environment::get().getWindowManager()->getConfirmationDialog();
        confirmation->askForConfirmation("#{sDeleteNote}");
        confirmation->eventCancelClicked.clear();
        confirmation->eventOkClicked.clear();
        confirmation->eventOkClicked += MyGUI::newDelegate(this, &MapWindow::onNoteEditDeleteConfirm);
    }

    void MapWindow::onNoteEditDeleteConfirm()
    {
        mCustomMarkers.deleteMarker(mEditingMarker);

        mEditNoteDialog.setVisible(false);
    }

    void MapWindow::onCustomMarkerDoubleClicked(MyGUI::Widget* sender)
    {
        mEditingMarker = *sender->getUserData<ESM::CustomMarker>();
        mEditNoteDialog.setText(mEditingMarker.mNote);
        mEditNoteDialog.showDeleteButton(true);
        mEditNoteDialog.setVisible(true);
    }

    void MapWindow::onMapDoubleClicked(MyGUI::Widget* /*sender*/)
    {
        mEditingMarker = getMarkerAt(MyGUI::InputManager::getInstance().getMousePosition());

        mEditNoteDialog.setVisible(true);
        mEditNoteDialog.showDeleteButton(false);
        mEditNoteDialog.setText({});
    }

    ESM::CustomMarker MapWindow::getMarkerAt(MyGUI::IntPoint point) const
    {
        MyGUI::IntPoint widgetPos = point - mEventBoxLocal->getAbsolutePosition();
        auto mapWidgetSize = getWidgetSize();
        int x = int(widgetPos.left / float(mapWidgetSize)) + mGrid.left;
        int y = mGrid.bottom - int(widgetPos.top / float(mapWidgetSize));
        float nX = widgetPos.left / float(mapWidgetSize) - int(widgetPos.left / float(mapWidgetSize));
        float nY = widgetPos.top / float(mapWidgetSize) - int(widgetPos.top / float(mapWidgetSize));

        osg::Vec2f worldPos;
        if (!mActiveCell->isExterior())
        {
            worldPos = mLocalMapRender->interiorMapToWorldPosition(nX, nY, x, y);
        }
        else
        {
            worldPos.x() = (x + nX) * cellSize;
            worldPos.y() = (y + (1.0f - nY)) * cellSize;
        }

        ESM::CustomMarker marker;
        marker.mWorldX = worldPos.x();
        marker.mWorldY = worldPos.y();
        marker.mCell = getCellIdInWorldSpace(*mActiveCell, x, y);
        return marker;
    }

    void MapWindow::updateCrosshair()
    {
        const bool active = mCrosshair != nullptr && mActiveCell != nullptr && mControllerGuide->getVisible()
            && !Settings::map().mGlobal && mMap3DStarted;
        mGuideMark->setVisible(active);
        mGuideZoom->setVisible(mControllerGuide->getVisible());
        if (mCrosshair != nullptr)
        {
            // The sticks work the 3D view here, rather than the mouse pointer.
            mDisableGamepadCursor = !Settings::map().mGlobal;
            // The crosshair stays in the middle of the 3D view, and the right stick pans the view under it.
            mCrosshair->setVisible(active);
        }

        MyGUI::Widget* pin = nullptr;
        if (active)
        {
            const MyGUI::IntPoint center = mMap3DImage->getAbsolutePosition()
                + MyGUI::IntPoint(mMap3DImage->getWidth() / 2, mMap3DImage->getHeight() / 2);
            const MyGUI::IntPoint inWindow = center - mCrosshair->getParent()->getAbsolutePosition();
            mCrosshair->setPosition(
                inWindow.left - mCrosshair->getWidth() / 2, inWindow.top - mCrosshair->getHeight() / 2);

            // The nearest pin within the crosshair's reach.
            const int reach = mCrosshair->getWidth() / 2;
            int nearest = reach * reach + 1;
            for (const Map3DMarker& marker : mMap3DPins)
            {
                MyGUI::Widget* widget = marker.mWidget;
                if (!widget->getVisible())
                    continue;
                const MyGUI::IntCoord coord = widget->getAbsoluteCoord();
                const int dx = coord.left + coord.width / 2 - center.left;
                const int dy = coord.top + coord.height / 2 - center.top;
                if (dx * dx + dy * dy < nearest)
                {
                    nearest = dx * dx + dy * dy;
                    pin = widget;
                }
            }
        }
        if (pin != mPinAtCrosshair)
        {
            // The pin under the crosshair shows at full brightness, as one under the mouse does.
            if (mPinAtCrosshair != nullptr)
                mPinAtCrosshair->castType<MarkerWidget>()->setNormalColour(MyGUI::Colour(0.6f, 0.6f, 0.6f));
            if (pin != nullptr)
                pin->castType<MarkerWidget>()->setNormalColour(MyGUI::Colour::White);
            mPinAtCrosshair = pin;
        }

        // The pins can be made again without the crosshair moving, as when one is removed, so the hint is checked on
        // its own.
        const bool canRemove = pin != nullptr;
        if (canRemove == mGuideMarkRemoves)
            return;
        mGuideMarkRemoves = canRemove;
        mGuideMarkText->setCaptionWithReplacing(canRemove ? "#{Interface:MapRemovePin}" : "#{Interface:MapPlacePin}");
        mGuideMark->castType<Gui::HBox>()->notifyChildrenSizeChanged();
    }

    void MapWindow::togglePinAtCrosshair()
    {
        if (mCrosshair == nullptr || !mGuideMark->getVisible())
            return;
        MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
        if (mPinAtCrosshair == nullptr)
        {
            mCustomMarkers.addMarker(getMap3DPinAt(mMap3DFocus.x(), mMap3DFocus.y()));
            return;
        }

        mEditingMarker = *mPinAtCrosshair->getUserData<ESM::CustomMarker>();
        // A pin with a note, which only a mouse can write, asks before it goes.
        if (!mEditingMarker.mNote.empty())
            onNoteEditDelete();
        else
            mCustomMarkers.deleteMarker(mEditingMarker);
    }

    void MapWindow::createGlobalMapPins()
    {
        for (const auto& [widget, position] : mGlobalMapPins)
            MyGUI::Gui::getInstance().destroyWidget(widget);
        mGlobalMapPins.clear();

        for (const auto& [cell, marker] : mCustomMarkers)
        {
            // The global map only shows the exterior, so pins inside buildings and caves stay on their local maps.
            if (!marker.mCell.is<ESM::ESM3ExteriorCellRefId>())
                continue;
            MyGUI::ImageBox* pin
                = mGlobalMap->createWidget<MyGUI::ImageBox>("ImageBox", MyGUI::IntCoord(), MyGUI::Align::Default);
            pin->setImageTexture("icons\\map_marker_red.dds");
            pin->setNeedMouseFocus(false);
            pin->setDepth(Global_CompassLayer);
            mGlobalMapPins.emplace_back(pin, osg::Vec2f(marker.mWorldX, marker.mWorldY));
        }
        placeGlobalMapPins();
    }

    void MapWindow::placeGlobalMapPins()
    {
        // The same size as on the local map, whatever the zoom.
        constexpr int size = 16;
        for (const auto& [widget, position] : mGlobalMapPins)
        {
            float x, y;
            worldPosToGlobalMapImageSpace(position.x(), position.y(), x, y);
            widget->setCoord(static_cast<int>(x) - size / 2, static_cast<int>(y) - size / 2, size, size);
        }
    }

    void MapWindow::onMapZoomed(MyGUI::Widget* /*sender*/, int rel)
    {
        const MyGUI::ScrollView* map = Settings::map().mGlobal ? mGlobalMap : mLocalMap;
        zoomMap(rel < 0 ? 1.f / speed : speed,
            MyGUI::InputManager::getInstance().getMousePosition() - map->getAbsolutePosition());
    }

    bool MapWindow::zoomMap(float factor, MyGUI::IntPoint focus)
    {
        const int localWidgetSize = Settings::map().mLocalMapWidgetSize;
        const bool zoomOut = factor < 1.f;

        // Each zoom stops at its limits: 4 times the normal size, and small enough to show the whole map.
        float speedDiff = factor;
        if (Settings::map().mGlobal)
        {
            const float currentGlobalZoom = mGlobalMapZoom;
            const float currentMinGlobalMapZoom
                = std::min(float(mGlobalMap->getWidth()) / float(mGlobalMapRender->getWidth()),
                    float(mGlobalMap->getHeight()) / float(mGlobalMapRender->getHeight()));

            mGlobalMapZoom = std::clamp(mGlobalMapZoom * factor, std::min(currentMinGlobalMapZoom, currentGlobalZoom),
                std::max(4.f, currentGlobalZoom));
            if (mGlobalMapZoom == currentGlobalZoom)
                return false;
            speedDiff = mGlobalMapZoom / currentGlobalZoom;
        }
        else
        {
            const float currentMinLocalMapZoom
                = std::max({ (float(Settings::map().mGlobalMapCellSize) * 4.f) / float(localWidgetSize),
                    float(mLocalMap->getWidth()) / (localWidgetSize * (mGrid.width() + 1)),
                    float(mLocalMap->getHeight()) / (localWidgetSize * (mGrid.height() + 1)) });

            const float currentLocalZoom = mLocalMapZoom;
            mLocalMapZoom = std::clamp(mLocalMapZoom * factor, std::min(currentMinLocalMapZoom, currentLocalZoom),
                std::max(4.f, currentLocalZoom));
            if (mLocalMapZoom == currentLocalZoom)
                return false;
            speedDiff = mLocalMapZoom / currentLocalZoom;

            if (zoomOut)
                mNeedDoorMarkersUpdate = true;
        }
        zoomAround(speedDiff, focus);
        return true;
    }

    void MapWindow::zoomWithController(bool zoomIn)
    {
        // Each press zooms by about as much as three turns of the mouse wheel.
        constexpr float step = 1.25f;
        // The zoom keeps the player still when in view, or else the middle of the view.
        MyGUI::IntPoint focus(mGlobalMap->getWidth() / 2, mGlobalMap->getHeight() / 2);
        const MyGUI::IntPoint player
            = mPlayerArrowGlobal->getPosition() + MyGUI::IntPoint{ 16, 16 } + mGlobalMap->getViewOffset();
        if (player.left >= 0 && player.top >= 0 && player.left <= mGlobalMap->getWidth()
            && player.top <= mGlobalMap->getHeight())
            focus = player;
        if (zoomMap(zoomIn ? step : 1.f / step, focus))
            MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
    }

    void MapWindow::zoomMap3D(float factor)
    {
        const bool exterior = mActiveCell == nullptr || mActiveCell->isExterior();
        float& distance = exterior ? mMap3DDistance : mMap3DInteriorDistance;
        const float zoomed = distance / factor;
        distance = exterior ? std::clamp(zoomed, 6000.f, 100000.f) : std::clamp(zoomed, 1000.f, 30000.f);
    }

    void MapWindow::zoomByPinch(float factor, MyGUI::IntPoint point)
    {
        if (!mMap3DStarted)
        {
            zoomMap(factor, point - mGlobalMap->getAbsolutePosition());
            return;
        }
        const bool exterior = mActiveCell == nullptr || mActiveCell->isExterior();
        const float before = exterior ? mMap3DDistance : mMap3DInteriorDistance;
        zoomMap3D(factor);
        const float after = exterior ? mMap3DDistance : mMap3DInteriorDistance;
        // The view looks down at its focus, so moving the focus towards the pinch by the zoom's share keeps the ground
        // under the pinch where it was.
        osg::Vec3f ground;
        if (after != before && map3DPointToGround(point, ground))
        {
            const float kept = after / before;
            mMap3DFocus.x() = ground.x() + (mMap3DFocus.x() - ground.x()) * kept;
            mMap3DFocus.y() = ground.y() + (mMap3DFocus.y() - ground.y()) * kept;
        }
    }

    void MapWindow::zoomAround(float speedDiff, MyGUI::IntPoint cursor)
    {
        auto map = Settings::map().mGlobal ? mGlobalMap : mLocalMap;
        auto centerView = map->getViewOffset() - cursor;

        Settings::map().mGlobal ? updateGlobalMap() : updateLocalMap();

        map->setViewOffset(MyGUI::IntPoint(static_cast<int>(std::round(centerView.left * speedDiff) + cursor.left),
            static_cast<int>(std::round(centerView.top * speedDiff) + cursor.top)));
    }

    void MapWindow::updateGlobalMap()
    {
        resizeGlobalMap();

        float x = mCurPos.x(), y = mCurPos.y();
        if (!mActiveCell->isExterior())
        {
            auto pos = MWBase::Environment::get().getWorld()->getPlayer().getLastKnownExteriorPosition();
            x = pos.x();
            y = pos.y();
        }
        setGlobalMapPlayerPosition(x, y);

        for (auto& [marker, col] : mGlobalMapMarkers)
        {
            marker.widget->setCoord(
                createMarkerCoords(marker.position.x(), marker.position.y(), static_cast<float>(col.size())));
            marker.widget->setVisible(marker.widget->getHeight() >= 6);
        }
        placeGlobalMapPins();
    }

    void MapWindow::onChangeScrollWindowCoord(MyGUI::Widget* sender)
    {
        MyGUI::IntCoord currentCoordinates = sender->getCoord();

        MyGUI::IntPoint currentViewPortCenter
            = MyGUI::IntPoint(currentCoordinates.width / 2, currentCoordinates.height / 2);
        MyGUI::IntPoint lastViewPortCenter
            = MyGUI::IntPoint(mLastScrollWindowCoordinates.width / 2, mLastScrollWindowCoordinates.height / 2);
        MyGUI::IntPoint viewPortCenterDiff = currentViewPortCenter - lastViewPortCenter;

        mLocalMap->setViewOffset(mLocalMap->getViewOffset() + viewPortCenterDiff);
        mGlobalMap->setViewOffset(mGlobalMap->getViewOffset() + viewPortCenterDiff);

        mLastScrollWindowCoordinates = currentCoordinates;
    }

    void MapWindow::setVisible(bool visible)
    {
        WindowBase::setVisible(visible);
        MWGui::GuiMode mode = MWBase::Environment::get().getWindowManager()->getMode();
        const bool inMenu = visible && mode != MWGui::GM_None;
        mButton->setVisible(inMenu && !Settings::gui().mControllerMenus);
        mControllerGuide->setVisible(inMenu && Settings::gui().mControllerMenus);
        if (!visible && mControllerTabs != nullptr)
            mControllerTabs->setVisible(false);

        if (Settings::gui().mControllerMenus && mode == MWGui::GM_None && pinned() && visible)
        {
            // Restore the window to pinned size.
            MyGUI::Window* window = mMainWidget->castType<MyGUI::Window>();
            MyGUI::IntSize viewSize = MyGUI::RenderManager::getInstance().getViewSize();
            const int x = static_cast<int>(Settings::windows().mMapX * viewSize.width);
            const int y = static_cast<int>(Settings::windows().mMapY * viewSize.height);
            const int w = static_cast<int>(Settings::windows().mMapW * viewSize.width);
            const int h = static_cast<int>(Settings::windows().mMapH * viewSize.height);
            window->setCoord(x, y, w, h);
        }
    }

    void MapWindow::renderGlobalMap()
    {
        mGlobalMapRender->render();
        resizeGlobalMap();
    }

    MapWindow::~MapWindow() = default;

    void MapWindow::setCellName(const std::string& cellName)
    {
        setTitle("#{sCell=" + cellName + "}");
        if (mControllerTabs != nullptr && mControllerTabs->getVisible())
            drawControllerTabs();
    }

    MyGUI::IntCoord MapWindow::createMarkerCoords(float x, float y, float agregatedWeight) const
    {
        float worldX, worldY;
        worldPosToGlobalMapImageSpace(
            (x + 0.5f) * Constants::CellSizeInUnits, (y + 0.5f) * Constants::CellSizeInUnits, worldX, worldY);

        const float markerSize = getMarkerSize(static_cast<size_t>(agregatedWeight));
        const float halfMarkerSize = markerSize / 2.0f;
        return MyGUI::IntCoord(static_cast<int>(worldX - halfMarkerSize), static_cast<int>(worldY - halfMarkerSize),
            static_cast<int>(markerSize), static_cast<int>(markerSize));
    }

    MyGUI::Widget* MapWindow::createMarker(const std::string& name, float x, float y, float agregatedWeight)
    {
        MyGUI::Widget* markerWidget = mGlobalMap->createWidget<MyGUI::Widget>(
            "MarkerButton", createMarkerCoords(x, y, agregatedWeight), MyGUI::Align::Default);
        markerWidget->setVisible(markerWidget->getHeight() >= 6.0);
        markerWidget->setUserString("Caption_TextOneLine", "#{sCell=" + name + "}");
        setGlobalMapMarkerTooltip(markerWidget, static_cast<int>(x), static_cast<int>(y));

        markerWidget->setUserString("ToolTipLayout", "TextToolTipOneLine");

        markerWidget->setNeedMouseFocus(true);
        markerWidget->setColour(
            MyGUI::Colour::parse(MyGUI::LanguageManager::getInstance().replaceTags("#{fontcolour=normal}")));
        markerWidget->setDepth(Global_MarkerLayer);
        markerWidget->eventMouseDrag += MyGUI::newDelegate(this, &MapWindow::onMouseDrag);
        if (Settings::map().mAllowZooming)
            markerWidget->eventMouseWheel += MyGUI::newDelegate(this, &MapWindow::onMapZoomed);
        markerWidget->eventMouseButtonPressed += MyGUI::newDelegate(this, &MapWindow::onDragStart);

        return markerWidget;
    }

    void MapWindow::addVisitedLocation(const std::string& name, int x, int y)
    {
        CellId cell;
        cell.first = x;
        cell.second = y;
        if (mMarkers.insert(cell).second)
        {
            const osg::Vec2f pos(static_cast<float>(x), static_cast<float>(y));
            MapMarkerType mapMarkerWidget = { pos, createMarker(name, pos.x(), pos.y(), 0) };
            mGlobalMapMarkers.emplace(mapMarkerWidget, std::vector<MapMarkerType>());

            const std::string markerName = name.substr(0, name.find(','));
            auto& entry = mGlobalMapMarkersByName[markerName];
            if (!entry.widget)
            {
                entry = { pos, entry.widget }; // update the coords

                entry.widget = createMarker(markerName, entry.position.x(), entry.position.y(), 1);
                mGlobalMapMarkers.emplace(entry, std::vector<MapMarkerType>{ entry });
            }
            else
            {
                auto it = mGlobalMapMarkers.find(entry);
                auto& marker = const_cast<MapMarkerType&>(it->first);
                auto& elements = it->second;
                elements.emplace_back(mapMarkerWidget);

                // we compute the barycenter of the entry elements => it will be the place on the world map for the
                // agregated widget
                marker.position = std::accumulate(elements.begin(), elements.end(), osg::Vec2f(0.f, 0.f),
                                      [](const auto& left, const auto& right) { return left + right.position; })
                    / float(elements.size());

                marker.widget->setCoord(
                    createMarkerCoords(marker.position.x(), marker.position.y(), static_cast<float>(elements.size())));
                marker.widget->setVisible(marker.widget->getHeight() >= 6);
            }
        }
    }

    void MapWindow::cellExplored(int x, int y)
    {
        mGlobalMapRender->cleanupCameras();
        mGlobalMapRender->exploreCell(x, y, *mLocalMapRender);
    }

    void MapWindow::onFrame(float dt)
    {
        LocalMapBase::onFrame(dt);
        NoDrop::onFrame(dt);

        if (Settings::gui().mControllerMenus && !Settings::map().mGlobal
            && MWBase::Environment::get().getWindowManager()->getActiveControllerWindow() == this)
        {
            if (mMap3DStarted)
                updateMap3D(dt);
        }
        updateCrosshair();
    }

    void MapWindow::setGlobalMapMarkerTooltip(MyGUI::Widget* markerWidget, int x, int y)
    {
        ESM::RefId cellRefId = ESM::RefId::esm3ExteriorCell(x, y);
        CustomMarkerCollection::RangeType markers = mCustomMarkers.getMarkers(cellRefId);
        std::vector<std::string> destNotes;
        for (CustomMarkerCollection::ContainerType::const_iterator it = markers.first; it != markers.second; ++it)
            destNotes.push_back(it->second.mNote);

        if (!destNotes.empty())
        {
            MarkerUserData data(nullptr);
            std::swap(data.notes, destNotes);
            data.caption = markerWidget->getUserString("Caption_TextOneLine");
            markerWidget->setUserData(data);
            markerWidget->setUserString("ToolTipType", "MapMarker");
        }
        else
        {
            markerWidget->setUserString("ToolTipType", "Layout");
        }
    }

    float MapWindow::getMarkerSize(size_t agregatedWeight) const
    {
        float markerSize = 12.f * mGlobalMapZoom;
        if (mGlobalMapZoom < 1)
            return static_cast<float>(markerSize * std::sqrt(agregatedWeight)); // we want to see agregated object
        return agregatedWeight ? 0 : markerSize; // we want to see only original markers (i.e. non agregated)
    }

    void MapWindow::resizeGlobalMap()
    {
        int width = static_cast<int>(mGlobalMapRender->getWidth() * mGlobalMapZoom);
        int height = static_cast<int>(mGlobalMapRender->getHeight() * mGlobalMapZoom);
        mGlobalMap->setCanvasSize(width, height);
        mGlobalMapImage->setSize(width, height);
    }

    void MapWindow::fitGlobalMapToView()
    {
        const int mapWidth = mGlobalMapRender->getWidth();
        const int mapHeight = mGlobalMapRender->getHeight();
        if (mapWidth <= 0 || mapHeight <= 0)
            return;
        mGlobalMapZoom = std::min(float(mGlobalMap->getWidth()) / float(mapWidth),
            float(mGlobalMap->getHeight()) / float(mapHeight));
        updateGlobalMap();
    }

    void MapWindow::worldPosToGlobalMapImageSpace(float x, float y, float& imageX, float& imageY) const
    {
        mGlobalMapRender->worldPosToImageSpace(x, y, imageX, imageY);
        imageX *= mGlobalMapZoom;
        imageY *= mGlobalMapZoom;
    }

    void MapWindow::updateCustomMarkers()
    {
        // The local map's pins are made again below, so the one under the crosshair is found again.
        mPinAtCrosshair = nullptr;
        LocalMapBase::updateCustomMarkers();
        createGlobalMapPins();
        createMap3DPins();

        for (auto& [widgetPair, ignore] : mGlobalMapMarkers)
            setGlobalMapMarkerTooltip(widgetPair.widget, static_cast<int>(widgetPair.position.x()),
                static_cast<int>(widgetPair.position.y()));
    }

    void MapWindow::onDragStart(MyGUI::Widget* /*sender*/, int left, int top, MyGUI::MouseButton id)
    {
        if (id != MyGUI::MouseButton::Left)
            return;
        mLastDragPos = MyGUI::IntPoint(left, top);
    }

    void MapWindow::onMouseDrag(MyGUI::Widget* /*sender*/, int left, int top, MyGUI::MouseButton id)
    {
        if (id != MyGUI::MouseButton::Left)
            return;

        MyGUI::IntPoint diff = MyGUI::IntPoint(left, top) - mLastDragPos;

        if (!Settings::map().mGlobal)
        {
            mNeedDoorMarkersUpdate = true;
            mLocalMap->setViewOffset(mLocalMap->getViewOffset() + diff);
        }
        else
            mGlobalMap->setViewOffset(mGlobalMap->getViewOffset() + diff);

        mLastDragPos = MyGUI::IntPoint(left, top);
    }

    void MapWindow::onWorldButtonClicked(MyGUI::Widget* /*sender*/)
    {
        const bool global = !Settings::map().mGlobal;

        Settings::map().mGlobal.set(global);

        mGlobalMap->setVisible(global);
        mLocalMap->setVisible(!global);

        updateWorldToggleCaptions();
    }

    void MapWindow::onPinToggled()
    {
        Settings::windows().mMapPin.set(mPinned);
    }

    void MapWindow::onTitleDoubleClicked()
    {
        if (Settings::gui().mControllerMenus)
            return;
        else if (MyGUI::InputManager::getInstance().isShiftPressed())
            MWBase::Environment::get().getWindowManager()->toggleMaximized(this);
        else if (!mPinned)
            MWBase::Environment::get().getWindowManager()->toggleVisible(GW_Map);
    }

    void MapWindow::onOpen()
    {
        ensureGlobalMapLoaded();

        globalMapUpdatePlayer();
    }

    void MapWindow::onClose()
    {
        stopMap3D();
    }

    void MapWindow::globalMapUpdatePlayer()
    {
        // For interiors, position is set by WindowManager via setGlobalMapPlayerPosition
        if (MWBase::Environment::get().getWorld()->isCellExterior())
        {
            osg::Vec3f pos = MWBase::Environment::get().getWorld()->getPlayerPtr().getRefData().getPosition().asVec3();
            setGlobalMapPlayerPosition(pos.x(), pos.y());
        }
    }

    void MapWindow::notifyPlayerUpdate()
    {
        globalMapUpdatePlayer();

        setGlobalMapPlayerDir(mLastDirectionX, mLastDirectionY);
    }

    void MapWindow::centerView()
    {
        LocalMapBase::centerView();
        // set the view offset so that player is in the center
        MyGUI::IntSize viewsize = mGlobalMap->getSize();
        MyGUI::IntPoint pos = mPlayerArrowGlobal->getPosition() + MyGUI::IntPoint{ 16, 16 };
        MyGUI::IntPoint viewoffs(
            static_cast<int>(viewsize.width * 0.5f - pos.left), static_cast<int>(viewsize.height * 0.5f - pos.top));
        mGlobalMap->setViewOffset(viewoffs);
    }

    void MapWindow::setGlobalMapPlayerPosition(float worldX, float worldY)
    {
        float x, y;
        worldPosToGlobalMapImageSpace(worldX, worldY, x, y);
        mPlayerArrowGlobal->setPosition(MyGUI::IntPoint(static_cast<int>(x - 16), static_cast<int>(y - 16)));
    }

    void MapWindow::setGlobalMapPlayerDir(const float x, const float y)
    {
        MyGUI::ISubWidget* main = mPlayerArrowGlobal->getSubWidgetMain();
        MyGUI::RotatingSkin* rotatingSubskin = main->castType<MyGUI::RotatingSkin>();
        rotatingSubskin->setCenter(MyGUI::IntPoint(16, 16));
        float angle = std::atan2(x, y);
        rotatingSubskin->setAngle(angle);
    }

    void MapWindow::ensureGlobalMapLoaded()
    {
        if (!mGlobalMapTexture.get())
        {
            // The generated unexplored map and explored map RTT images are Y-up so the UVs are inverted
            // The unexplored map isn't saved so we *could* consider generating it the "right" way
            // but mixing conventions for map images could make things confusing
            mGlobalMapTexture = mGlobalMapRender->makeBaseGuiTexture();
            mGlobalMapImage->setRenderItemTexture(mGlobalMapTexture.get());
            mGlobalMapImage->getSubWidgetMain()->_setUVSet(MyGUI::FloatRect(0.f, 1.f, 1.f, 0.f));

            mGlobalMapOverlayTexture = mGlobalMapRender->makeOverlayGuiTexture();
            mGlobalMapOverlay->setRenderItemTexture(mGlobalMapOverlayTexture.get());
            mGlobalMapOverlay->getSubWidgetMain()->_setUVSet(MyGUI::FloatRect(0.f, 1.f, 1.f, 0.f));

            // Redraw children in proper order
            mGlobalMap->getParent()->_updateChilds();
        }
    }

    void MapWindow::clear()
    {
        mMarkers.clear();

        mGlobalMapRender->clear();
        mActiveCell = nullptr;

        for (auto& widgetPair : mGlobalMapMarkers)
            MyGUI::Gui::getInstance().destroyWidget(widgetPair.first.widget);
        mGlobalMapMarkers.clear();
        mGlobalMapMarkersByName.clear();

        for (const auto& [widget, position] : mGlobalMapPins)
            MyGUI::Gui::getInstance().destroyWidget(widget);
        mGlobalMapPins.clear();

        // A new game or a loaded one has its own pins, doors and magic.
        for (std::vector<Map3DMarker>* markers : { &mMap3DPins, &mMap3DDoors, &mMap3DMagicMarkers })
        {
            for (const Map3DMarker& marker : *markers)
                MyGUI::Gui::getInstance().destroyWidget(marker.mWidget);
            markers->clear();
        }
        mMap3DDoorCells.clear();
        mMap3DDoorsSpace = ESM::RefId();
        mPinAtCrosshair = nullptr;
    }

    void MapWindow::write(ESM::ESMWriter& writer, Loading::Listener& progress)
    {
        ESM::GlobalMap map;
        mGlobalMapRender->write(map);

        map.mMarkers = mMarkers;

        writer.startRecord(ESM::REC_GMAP);
        map.save(writer);
        writer.endRecord(ESM::REC_GMAP);
    }

    void MapWindow::readRecord(ESM::ESMReader& reader, uint32_t type)
    {
        if (type == ESM::REC_GMAP)
        {
            ESM::GlobalMap map;
            map.load(reader);

            mGlobalMapRender->read(map);

            for (const ESM::GlobalMap::CellId& cellId : map.mMarkers)
            {
                const ESM::Cell* cell
                    = MWBase::Environment::get().getESMStore()->get<ESM::Cell>().search(cellId.first, cellId.second);
                if (cell && !cell->mName.empty())
                    addVisitedLocation(cell->mName, cellId.first, cellId.second);
            }
        }
    }

    void MapWindow::setAlpha(float alpha)
    {
        NoDrop::setAlpha(alpha);
        // can't allow showing map with partial transparency, as the fog of war will also go transparent
        // and reveal parts of the map you shouldn't be able to see
        mLocalMap->setVisible(!mGlobalMap->getVisible() && alpha == 1 && !mMap3DStarted);
    }

    void MapWindow::customMarkerCreated(MyGUI::Widget* marker)
    {
        marker->eventMouseDrag += MyGUI::newDelegate(this, &MapWindow::onMouseDrag);
        marker->eventMouseButtonPressed += MyGUI::newDelegate(this, &MapWindow::onDragStart);
        marker->eventMouseButtonDoubleClick += MyGUI::newDelegate(this, &MapWindow::onCustomMarkerDoubleClicked);
        if (Settings::map().mAllowZooming)
            marker->eventMouseWheel += MyGUI::newDelegate(this, &MapWindow::onMapZoomed);
    }

    void MapWindow::doorMarkerCreated(MyGUI::Widget* marker)
    {
        marker->eventMouseDrag += MyGUI::newDelegate(this, &MapWindow::onMouseDrag);
        marker->eventMouseButtonPressed += MyGUI::newDelegate(this, &MapWindow::onDragStart);
        if (Settings::map().mAllowZooming)
            marker->eventMouseWheel += MyGUI::newDelegate(this, &MapWindow::onMapZoomed);
    }

    void MapWindow::asyncPrepareSaveMap()
    {
        mGlobalMapRender->asyncWritePng();
    }

    bool MapWindow::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
    {
        if (arg.button == SDL_CONTROLLER_BUTTON_B)
            MWBase::Environment::get().getWindowManager()->exitCurrentGuiMode();
        else if (arg.button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER
            || arg.button == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)
        {
            // The shoulder buttons switch between the Local and World tabs. There are only two, so either one
            // switches to the other.
            // The 3D view stops on the World tab, and starts again on the Local tab.
            const bool toWorld = !Settings::map().mGlobal;
            if (toWorld)
                stopMap3D();
            onWorldButtonClicked(mButton);
            if (!toWorld)
                startMap3D();
            MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
        }
        else if (mMap3DStarted)
        {
            if (arg.button == SDL_CONTROLLER_BUTTON_X)
                togglePinAtCrosshair();
            else if (arg.button == SDL_CONTROLLER_BUTTON_Y)
            {
                centerMap3D();
                MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
            }
            else if (arg.button == SDL_CONTROLLER_BUTTON_DPAD_UP || arg.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN)
            {
                constexpr float step = 1.25f;
                zoomMap3D(arg.button == SDL_CONTROLLER_BUTTON_DPAD_UP ? step : 1.f / step);
                MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
            }
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_Y)
        {
            centerView();
            MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_DPAD_UP || arg.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN)
            zoomWithController(arg.button == SDL_CONTROLLER_BUTTON_DPAD_UP);

        return true;
    }

    bool MapWindow::onControllerThumbstickEvent(const SDL_ControllerAxisEvent& arg)
    {
        // On the local map, the right stick pans the 3D view, which onFrame does by reading the stick's position. The
        // left stick does nothing there, rather than move the mouse pointer.
        if (arg.axis == SDL_CONTROLLER_AXIS_LEFTX || arg.axis == SDL_CONTROLLER_AXIS_LEFTY)
            return !Settings::map().mGlobal;
        return arg.axis == SDL_CONTROLLER_AXIS_RIGHTX || arg.axis == SDL_CONTROLLER_AXIS_RIGHTY;
    }

    void MapWindow::setActiveControllerWindow(bool active)
    {
        MWBase::WindowManager* winMgr = MWBase::Environment::get().getWindowManager();
        if (winMgr->getMode() == MWGui::GM_Inventory)
        {
            // Fill the screen below the map's own tabs, as the inventory does. The window's frame and title bar sit
            // just outside that area, off the screen's edges and under the tabs, so only the map shows.
            const MyGUI::IntSize viewSize = MyGUI::RenderManager::getInstance().getViewSize();
            const MyGUI::IntCoord client = mMainWidget->getClientWidget()->getCoord();
            const MyGUI::IntSize borderSize = mMainWidget->getSize() - client.size();
            const int tabsBottom = static_cast<int>(std::round(sTabsBottom * viewSize.height / 720.f));
            const int mapHeight = viewSize.height - tabsBottom;

            const int width = viewSize.width + borderSize.width;
            const int height = mapHeight + borderSize.height;
            const int x = -client.left;
            const int y = viewSize.height - mapHeight - client.top;

            MyGUI::Window* window = mMainWidget->castType<MyGUI::Window>();
            window->setCoord(x, active ? y : viewSize.height + 1, width, height);
            if (active)
            {
                fitGlobalMapToView();
                layoutControllerGuide();
                drawControllerTabs();
            }
            else if (mControllerTabs != nullptr)
                mControllerTabs->setVisible(false);
        }
        // In controller menus mode, the local map is the 3D view. It draws only while the Local tab shows.
        if (!active)
            stopMap3D();
        else if (!Settings::map().mGlobal)
            startMap3D();

        WindowBase::setActiveControllerWindow(active);
    }

    void MapWindow::stopMap3D()
    {
        mMap3DStarted = false;
        mMap3DPointer.reset();
        if (mMap3D != nullptr)
            mMap3D->setShown(false);
        if (mMap3DImage != nullptr)
            mMap3DImage->setVisible(false);
    }

    void MapWindow::startMap3D()
    {
        if (mActiveCell == nullptr)
            return;
        MWBase::WindowManager* winMgr = MWBase::Environment::get().getWindowManager();

        // The picture has a pixel for each pixel of the screen that the map covers.
        const MyGUI::IntCoord coord = mLocalMap->getCoord();
        const float scale = winMgr->getScalingFactor();
        const int width = std::max(1, static_cast<int>(std::round(coord.width * scale)));
        const int height = std::max(1, static_cast<int>(std::round(coord.height * scale)));
        if (mMap3D == nullptr || mMap3D->getWidth() != width || mMap3D->getHeight() != height)
        {
            if (mMap3DImage != nullptr)
                mMap3DImage->setRenderItemTexture(nullptr);
            mMap3DTexture.reset();
            mMap3D.reset();
            mMap3D = MWBase::Environment::get().getWorld()->getRenderingManager()->createMap3D(width, height);
            mMap3DTexture = mMap3D->makeGuiTexture();
        }

        if (mMap3DImage == nullptr)
        {
            // The view covers the map's other widgets, except the button hints, which move in front of it.
            mMap3DImage
                = mLocalMap->getParent()->createWidget<MyGUI::ImageBox>("ImageBox", coord, MyGUI::Align::Stretch);
            // The mouse drags the view to pan it, and a click places a pin or removes one. The wheel zooms, as the
            // d-pad does.
            mMap3DImage->setNeedMouseFocus(true);
            mMap3DImage->eventMouseButtonPressed += MyGUI::newDelegate(this, &MapWindow::onMap3DMousePressed);
            mMap3DImage->eventMouseDrag += MyGUI::newDelegate(this, &MapWindow::onMap3DMouseDrag);
            mMap3DImage->eventMouseButtonReleased += MyGUI::newDelegate(this, &MapWindow::onMap3DMouseReleased);
            mMap3DImage->eventMouseMove += MyGUI::newDelegate(this, &MapWindow::onMap3DMouseMove);
            mMap3DImage->eventMouseLostFocus += MyGUI::newDelegate(this, &MapWindow::onMap3DMouseLostFocus);
            for (MyGUI::Widget* hint : { mControllerGuide, mGuideZoom, mGuideMark, mCrosshair })
                hint->setDepth(-1);
            // The player's arrow goes in front of the markers, and the pins in front of the doors.
            const int arrowSize = getMap3DMarkerSize(24);
            mMap3DPlayer = mMap3DImage->createWidget<MyGUI::ImageBox>(
                "RotatingSkin", MyGUI::IntCoord(0, 0, arrowSize, arrowSize), MyGUI::Align::Default);
            mMap3DPlayer->setImageTexture("textures\\compass.dds");
            mMap3DPlayer->setNeedMouseFocus(false);
            mMap3DPlayer->setDepth(-2);
            // The label goes in front of everything else in the view.
            mMap3DLabel = mMap3DImage->createWidget<MyGUI::Widget>("", MyGUI::IntCoord(), MyGUI::Align::Default);
            mMap3DLabel->setNeedMouseFocus(false);
            mMap3DLabel->setDepth(-3);
            mMap3DLabel->setVisible(false);
            ControllerUi::addRect(mMap3DLabel, 0, 0, 0, 0, MyGUI::Colour::Black, 0.7f)->setAlign(MyGUI::Align::Stretch);
            mMap3DLabelText = ControllerUi::addText(mMap3DLabel, 0, 0, 0, 0, {}, getMap3DMarkerSize(17),
                MyGUI::Colour::parse("#dfc99f"), MyGUI::Align::Center);
            mMap3DLabelText->setAlign(MyGUI::Align::Stretch);
        }
        mMap3DImage->setCoord(coord);
        mMap3DImage->setRenderItemTexture(mMap3DTexture.get());
        mMap3DImage->getSubWidgetMain()->_setUVSet(MyGUI::FloatRect(0.f, 1.f, 1.f, 0.f));
        mMap3DImage->setVisible(true);
        mLocalMap->setVisible(false);

        centerMap3D();
        // The fog of war may have changed since the view last showed.
        mMap3DFogCenter.reset();

        // The doors stay from one showing to the next, while the player stays in the same cell or worldspace.
        const ESM::RefId doorsSpace = mActiveCell->isExterior() ? mActiveCell->getWorldSpace() : mActiveCell->getId();
        if (doorsSpace != mMap3DDoorsSpace)
        {
            for (const Map3DMarker& marker : mMap3DDoors)
                MyGUI::Gui::getInstance().destroyWidget(marker.mWidget);
            mMap3DDoors.clear();
            mMap3DDoorCells.clear();
            mMap3DDoorsSpace = doorsSpace;
        }
        createMap3DPins();
        createMap3DMagicMarkers();

        mMap3D->setShown(true);
        mMap3DStarted = true;
        updateMap3D(0.f);
    }

    int MapWindow::getMap3DMarkerSize(int canvasSize) const
    {
        const float scale = MyGUI::RenderManager::getInstance().getViewSize().height / 720.f;
        return std::max(4, static_cast<int>(std::round(canvasSize * scale)));
    }

    float MapWindow::getMap3DGroundHeight(float x, float y) const
    {
        MWBase::World* world = MWBase::Environment::get().getWorld();
        if (mActiveCell->isExterior())
            return world->getTerrainHeightAt(osg::Vec3f(x, y, 0.f), mActiveCell->getWorldSpace());
        return world->getPlayerPtr().getRefData().getPosition().pos[2];
    }

    void MapWindow::createMap3DPins()
    {
        for (const Map3DMarker& marker : mMap3DPins)
        {
            if (marker.mWidget == mPinAtCrosshair)
                mPinAtCrosshair = nullptr;
            MyGUI::Gui::getInstance().destroyWidget(marker.mWidget);
        }
        mMap3DPins.clear();
        if (mMap3DImage == nullptr || mActiveCell == nullptr)
            return;

        const int size = getMap3DMarkerSize(16);
        for (const auto& [cellId, marker] : mCustomMarkers)
        {
            // Outdoors, the view shows the pins of every exterior cell, since it can look anywhere. Indoors, it shows
            // the pins of the player's cell.
            if (mActiveCell->isExterior() ? !cellId.is<ESM::ESM3ExteriorCellRefId>() : cellId != mActiveCell->getId())
                continue;
            MarkerWidget* widget = mMap3DImage->createWidget<MarkerWidget>(
                "CustomMarkerButton", MyGUI::IntCoord(0, 0, size, size), MyGUI::Align::Default);
            widget->setDepth(-1);
            widget->setNormalColour(MyGUI::Colour(0.6f, 0.6f, 0.6f));
            widget->setHoverColour(MyGUI::Colour::White);
            widget->setUserData(marker);
            widget->setNeedMouseFocus(false);
            mMap3DPins.push_back({ widget,
                osg::Vec3f(marker.mWorldX, marker.mWorldY, getMap3DGroundHeight(marker.mWorldX, marker.mWorldY)),
                marker.mNote });
        }
    }

    void MapWindow::createMap3DMagicMarkers()
    {
        for (const Map3DMarker& marker : mMap3DMagicMarkers)
            MyGUI::Gui::getInstance().destroyWidget(marker.mWidget);
        mMap3DMagicMarkers.clear();

        const int size = getMap3DMarkerSize(10);
        const auto addMarker = [&](const osg::Vec3f& position, std::string_view texture) {
            MyGUI::ImageBox* widget = mMap3DImage->createWidget<MyGUI::ImageBox>(
                "ImageBox", MyGUI::IntCoord(0, 0, size, size), MyGUI::Align::Default);
            widget->setImageTexture(texture);
            widget->setNeedMouseFocus(false);
            mMap3DMagicMarkers.push_back({ widget, position });
        };

        // The game is paused while the map shows, so these don't move until it opens again.
        MWBase::World* world = MWBase::Environment::get().getWorld();
        constexpr std::pair<MWBase::World::DetectionType, std::string_view> detections[] = {
            { MWBase::World::Detect_Creature, "textures\\detect_animal_icon.dds" },
            { MWBase::World::Detect_Key, "textures\\detect_key_icon.dds" },
            { MWBase::World::Detect_Enchantment, "textures\\detect_enchantment_icon.dds" },
        };
        for (const auto& [type, texture] : detections)
        {
            std::vector<MWWorld::Ptr> detected;
            world->listDetectedReferences(world->getPlayerPtr(), detected, type);
            for (const MWWorld::Ptr& ptr : detected)
                addMarker(ptr.getRefData().getPosition().asVec3(), texture);
        }

        MWWorld::CellStore* markedCell = nullptr;
        ESM::Position markedPosition;
        world->getPlayer().getMarkedPosition(markedCell, markedPosition);
        if (markedCell != nullptr && markedCell->getCell()->getWorldSpace() == mActiveCell->getWorldSpace())
            addMarker(markedPosition.asVec3(), "textures\\menu_map_smark.dds");
    }

    void MapWindow::addMap3DDoors()
    {
        MWBase::World* world = MWBase::Environment::get().getWorld();
        MWWorld::WorldModel* worldModel = MWBase::Environment::get().getWorldModel();
        const int size = getMap3DMarkerSize(9);
        const MyGUI::Colour colour
            = MyGUI::Colour::parse(MyGUI::LanguageManager::getInstance().replaceTags("#{fontcolour=normal}"));
        const auto addDoors = [&](MWWorld::CellStore& cell) {
            std::vector<MWBase::World::DoorMarker> doors;
            world->getDoorMarkers(cell, doors);
            for (const MWBase::World::DoorMarker& door : doors)
            {
                MarkerWidget* widget = mMap3DImage->createWidget<MarkerWidget>(
                    "MarkerButton", MyGUI::IntCoord(0, 0, size, size), MyGUI::Align::Default);
                widget->setNormalColour(colour);
                widget->setNeedMouseFocus(false);
                mMap3DDoors.push_back({ widget, osg::Vec3f(door.x, door.y, door.z),
                    MyGUI::LanguageManager::getInstance().replaceTags(door.name).asUTF8() });
            }
        };

        if (!mActiveCell->isExterior())
        {
            if (mMap3DDoorCells.empty())
            {
                mMap3DDoorCells.emplace(0, 0);
                addDoors(worldModel->getInterior(mActiveCell->getNameId()));
            }
            return;
        }

        // The cells in view, and a little past its edges.
        const float mapCellSize = static_cast<float>(cellSize);
        const int focusX = static_cast<int>(std::floor(mMap3DFocus.x() / mapCellSize));
        const int focusY = static_cast<int>(std::floor(mMap3DFocus.y() / mapCellSize));
        const int reach = std::clamp(static_cast<int>(mMap3DDistance * 2.f / mapCellSize) + 1, 1, 16);
        std::vector<std::pair<int, int>> cells;
        for (int x = focusX - reach; x <= focusX + reach; ++x)
            for (int y = focusY - reach; y <= focusY + reach; ++y)
                if (!mMap3DDoorCells.contains({ x, y }))
                    cells.emplace_back(x, y);
        std::sort(cells.begin(), cells.end(), [&](const auto& a, const auto& b) {
            const int distanceA = (a.first - focusX) * (a.first - focusX) + (a.second - focusY) * (a.second - focusY);
            const int distanceB = (b.first - focusX) * (b.first - focusX) + (b.second - focusY) * (b.second - focusY);
            return distanceA < distanceB;
        });

        constexpr int loadsPerFrame = 2;
        int loads = 0;
        for (const auto& [x, y] : cells)
        {
            const ESM::ExteriorCellLocation location(x, y, mActiveCell->getWorldSpace());
            MWWorld::CellStore& cell = worldModel->getExterior(location, false);
            // As on the flat map, the doors show in the cells the player has been to.
            const bool loaded = cell.getState() == MWWorld::CellStore::State_Loaded;
            if (!loaded && cell.getFog() == nullptr)
                continue;
            if (!loaded && loads++ == loadsPerFrame)
                break;
            mMap3DDoorCells.emplace(x, y);
            addDoors(worldModel->getExterior(location));
        }
    }

    void MapWindow::placeMap3DMarkers()
    {
        const MyGUI::IntSize size = mMap3DImage->getSize();
        for (const std::vector<Map3DMarker>* markers : { &mMap3DPins, &mMap3DDoors, &mMap3DMagicMarkers })
        {
            // The doors hide under the fog of war, as the places around them do, and indoors above the cut, as the
            // upper floors they lead from do. The player's own pins, and what magic shows, stay in sight.
            const bool isDoor = markers == &mMap3DDoors;
            for (const Map3DMarker& marker : *markers)
            {
                osg::Vec2f picture;
                const bool inView = mMap3D->worldToPicture(marker.mPosition, picture) && picture.x() >= 0.f
                    && picture.x() <= 1.f && picture.y() >= 0.f && picture.y() <= 1.f
                    && (!isDoor
                        || (isMap3DExplored(marker.mPosition)
                            && (!mMap3DCeiling.has_value() || marker.mPosition.z() < *mMap3DCeiling)));
                marker.mWidget->setVisible(inView);
                if (inView)
                    marker.mWidget->setPosition(
                        static_cast<int>(picture.x() * size.width) - marker.mWidget->getWidth() / 2,
                        static_cast<int>(picture.y() * size.height) - marker.mWidget->getHeight() / 2);
            }
        }
    }

    bool MapWindow::isMap3DExplored(const osg::Vec3f& position) const
    {
        if (!mMap3DFogShown || mMap3DFogImage == nullptr)
            return true;
        const osg::Vec3f place(position.x(), position.y(), 1.f);
        const int s = static_cast<int>(std::floor((mMap3DFogToU * place) * mMap3DFogImage->s()));
        const int t = static_cast<int>(std::floor((mMap3DFogToV * place) * mMap3DFogImage->t()));
        if (s < 0 || t < 0 || s >= mMap3DFogImage->s() || t >= mMap3DFogImage->t())
            return false;
        // Where the fog becomes solid in the view, a little way into the soft edge of what was explored.
        return *mMap3DFogImage->data(s, t) < 128;
    }

    ESM::CustomMarker MapWindow::getMap3DPinAt(float x, float y) const
    {
        ESM::CustomMarker marker;
        marker.mWorldX = x;
        marker.mWorldY = y;
        const float mapCellSize = static_cast<float>(cellSize);
        marker.mCell = getCellIdInWorldSpace(
            *mActiveCell, static_cast<int>(std::floor(x / mapCellSize)), static_cast<int>(std::floor(y / mapCellSize)));
        return marker;
    }

    bool MapWindow::map3DPointToGround(MyGUI::IntPoint point, osg::Vec3f& ground) const
    {
        if (mMap3D == nullptr || mMap3DImage == nullptr)
            return false;
        const MyGUI::IntCoord image = mMap3DImage->getAbsoluteCoord();
        if (image.width <= 0 || image.height <= 0)
            return false;
        const osg::Vec2f picture(static_cast<float>(point.left - image.left) / image.width,
            static_cast<float>(point.top - image.top) / image.height);
        // The ground is taken as level at the height the view looks at, which is close enough near the middle.
        return mMap3D->pictureToGround(picture, mMap3DFocus.z(), ground);
    }

    void MapWindow::togglePinAtMap3DPoint(MyGUI::IntPoint point)
    {
        if (mActiveCell == nullptr)
            return;
        MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));

        // A pin under the point goes, asking first when it has a note, as with X.
        const int reach = getMap3DMarkerSize(16);
        for (const Map3DMarker& pin : mMap3DPins)
        {
            if (!pin.mWidget->getVisible())
                continue;
            const MyGUI::IntCoord coord = pin.mWidget->getAbsoluteCoord();
            const int dx = coord.left + coord.width / 2 - point.left;
            const int dy = coord.top + coord.height / 2 - point.top;
            if (dx * dx + dy * dy > reach * reach)
                continue;
            mEditingMarker = *pin.mWidget->getUserData<ESM::CustomMarker>();
            if (!mEditingMarker.mNote.empty())
                onNoteEditDelete();
            else
                mCustomMarkers.deleteMarker(mEditingMarker);
            return;
        }

        osg::Vec3f ground;
        if (map3DPointToGround(point, ground))
            mCustomMarkers.addMarker(getMap3DPinAt(ground.x(), ground.y()));
    }

    void MapWindow::onMap3DMousePressed(MyGUI::Widget* /*sender*/, int left, int top, MyGUI::MouseButton id)
    {
        if (id != MyGUI::MouseButton::Left)
            return;
        mMap3DPressedAt = MyGUI::IntPoint(left, top);
        mMap3DDraggedTo = mMap3DPressedAt;
        mMap3DDragging = false;
    }

    void MapWindow::onMap3DMouseDrag(MyGUI::Widget* /*sender*/, int left, int top, MyGUI::MouseButton id)
    {
        if (id != MyGUI::MouseButton::Left)
            return;
        const MyGUI::IntPoint point(left, top);
        // A few pixels of movement still count as a click.
        constexpr int dragThreshold = 6;
        const MyGUI::IntPoint moved = point - mMap3DPressedAt;
        if (!mMap3DDragging && moved.left * moved.left + moved.top * moved.top < dragThreshold * dragThreshold)
            return;
        mMap3DDragging = true;

        // The ground that was under the pointer stays under it.
        osg::Vec3f from;
        osg::Vec3f to;
        if (map3DPointToGround(mMap3DDraggedTo, from) && map3DPointToGround(point, to))
        {
            mMap3DFocus.x() += from.x() - to.x();
            mMap3DFocus.y() += from.y() - to.y();
        }
        mMap3DDraggedTo = point;
        mMap3DPointer = point;
    }

    void MapWindow::onMap3DMouseMove(MyGUI::Widget* /*sender*/, int left, int top)
    {
        mMap3DPointer = MyGUI::IntPoint(left, top);
    }

    void MapWindow::onMap3DMouseLostFocus(MyGUI::Widget* /*sender*/, MyGUI::Widget* /*newFocus*/)
    {
        mMap3DPointer.reset();
    }

    void MapWindow::onMap3DMouseReleased(MyGUI::Widget* /*sender*/, int left, int top, MyGUI::MouseButton id)
    {
        if (id != MyGUI::MouseButton::Left)
            return;
        if (!mMap3DDragging)
            togglePinAtMap3DPoint(MyGUI::IntPoint(left, top));
        mMap3DDragging = false;
    }

    void MapWindow::centerMap3D()
    {
        mMap3DFocus = MWBase::Environment::get().getWorld()->getPlayerPtr().getRefData().getPosition().asVec3();
    }

    void MapWindow::updateMap3DFog(const MWWorld::CellStore& cell)
    {
        // The console's ToggleFogOfWar turns the fog off, as on the flat map.
        const bool fogShown = mFogOfWarToggled && mFogOfWarEnabled;
        if (!fogShown)
        {
            if (mMap3DFogShown)
                mMap3D->setFog(nullptr, osg::Vec3f(), osg::Vec3f());
            mMap3DFogShown = false;
            mMap3DFogCenter.reset();
            return;
        }

        const auto showFog = [&](const MWRender::LocalMap::FogImage& fog) {
            mMap3DFogImage = fog.mImage;
            mMap3DFogToU = fog.mToU;
            mMap3DFogToV = fog.mToV;
            mMap3DFogShown = fog.mImage != nullptr;
            mMap3D->setFog(fog.mImage, fog.mToU, fog.mToV);
        };

        // Indoors, the fog covers the whole cell, and the player can't move while the map shows.
        if (!cell.isExterior())
        {
            if (!mMap3DFogCenter.has_value())
            {
                mMap3DFogCenter = std::pair<int, int>(0, 0);
                showFog(mLocalMapRender->makeInteriorFogImage());
            }
            return;
        }

        // The fog of war covers a square of cells around the point, wider than the view when zoomed all the way out.
        // It is made again once the point moves far enough that the view could reach the square's edge.
        constexpr int fogCells = 64;
        constexpr int fogMargin = 16;
        const float fogCellSize = static_cast<float>(Constants::CellSizeInUnits);
        const std::pair<int, int> focusCell(static_cast<int>(std::floor(mMap3DFocus.x() / fogCellSize)),
            static_cast<int>(std::floor(mMap3DFocus.y() / fogCellSize)));
        if (mMap3DFogCenter.has_value() && std::abs(focusCell.first - mMap3DFogCenter->first) <= fogMargin
            && std::abs(focusCell.second - mMap3DFogCenter->second) <= fogMargin)
            return;

        mMap3DFogCenter = focusCell;
        const int westX = focusCell.first - fogCells / 2;
        const int southY = focusCell.second - fogCells / 2;
        showFog(mLocalMapRender->makeExteriorFogImage(westX, southY, fogCells, cell.getCell()->getWorldSpace()));
    }

    void MapWindow::updateMap3D(float dt)
    {
        if (mMap3D == nullptr)
            return;
        MWBase::World* world = MWBase::Environment::get().getWorld();
        const MWWorld::Ptr player = world->getPlayerPtr();

        // The right stick moves the point the camera looks at, faster when the camera is farther away. Pushing the
        // stick up moves north, which is the top of the view.
        const MWBase::InputManager* input = MWBase::Environment::get().getInputManager();
        const float x = stickResponse(input->getControllerAxisValue(SDL_CONTROLLER_AXIS_RIGHTX));
        const float y = stickResponse(input->getControllerAxisValue(SDL_CONTROLLER_AXIS_RIGHTY));
        mMap3DFocus.x() += x * mMap3DDistance * dt;
        mMap3DFocus.y() -= y * mMap3DDistance * dt;
        if (x != 0.f || y != 0.f)
            mMap3DPointer.reset();

        // Outdoors, the camera looks at the ground under the point. Indoors, it stays at the height of the player's
        // feet, and everything a little above the player's head is cut away, ceilings and upper floors included.
        const MWWorld::CellStore& cell = *player.getCell();
        if (cell.isExterior())
        {
            mMap3DFocus.z() = world->getTerrainHeightAt(mMap3DFocus, cell.getCell()->getWorldSpace());
            mMap3DCeiling.reset();
        }
        else
        {
            constexpr float headroom = 200.f;
            mMap3DFocus.z() = player.getRefData().getPosition().pos[2];
            mMap3DCeiling = mMap3DFocus.z() + headroom;
        }
        mMap3D->setIndoors(mMap3DCeiling);
        updateMap3DFog(cell);
        mMap3D->setView(mMap3DFocus, cell.isExterior() ? mMap3DDistance : mMap3DInteriorDistance);
        addMap3DDoors();
        placeMap3DMarkers();

        // The player's arrow points the way the player faces. The view looks north, as the flat map does.
        const ESM::Position& position = player.getRefData().getPosition();
        osg::Vec2f picture;
        const bool inView = mMap3D->worldToPicture(position.asVec3(), picture) && picture.x() >= 0.f
            && picture.x() <= 1.f && picture.y() >= 0.f && picture.y() <= 1.f;
        mMap3DPlayer->setVisible(inView);
        if (inView)
        {
            const MyGUI::IntSize size = mMap3DImage->getSize();
            const int half = mMap3DPlayer->getWidth() / 2;
            mMap3DPlayer->setPosition(
                static_cast<int>(picture.x() * size.width) - half, static_cast<int>(picture.y() * size.height) - half);
            MyGUI::RotatingSkin* arrow = mMap3DPlayer->getSubWidgetMain()->castType<MyGUI::RotatingSkin>();
            arrow->setCenter(MyGUI::IntPoint(half, half));
            arrow->setAngle(position.rot[2]);
        }
        updateMap3DLabel();
    }

    void MapWindow::updateMap3DLabel()
    {
        if (mMap3DLabel == nullptr)
            return;

        // The marker nearest the mouse pointer, or else the one nearest the crosshair, within a short reach of it.
        // A finger on a touch screen moves the pointer too, but it doesn't hover, and it covers what's under it, so
        // with touch controls the crosshair always picks the marker.
        std::optional<MyGUI::IntPoint> point;
        if (!Settings::input().mTouchControls)
            point = mMap3DPointer;
        int reach = getMap3DMarkerSize(12);
        if (!point.has_value() && mCrosshair != nullptr && mCrosshair->getVisible())
        {
            point = mMap3DImage->getAbsolutePosition()
                + MyGUI::IntPoint(mMap3DImage->getWidth() / 2, mMap3DImage->getHeight() / 2);
            reach = mCrosshair->getWidth() / 2;
        }
        // A door within reach is named before a pin, since most pins have no note. Doors under the fog of war are
        // hidden, so they aren't named.
        const Map3DMarker* found = nullptr;
        bool isDoor = false;
        if (point.has_value())
        {
            for (const std::vector<Map3DMarker>* markers : { &mMap3DDoors, &mMap3DPins })
            {
                int nearest = reach * reach + 1;
                for (const Map3DMarker& marker : *markers)
                {
                    if (marker.mName.empty() || !marker.mWidget->getVisible())
                        continue;
                    const MyGUI::IntCoord coord = marker.mWidget->getAbsoluteCoord();
                    const int dx = coord.left + coord.width / 2 - point->left;
                    const int dy = coord.top + coord.height / 2 - point->top;
                    if (dx * dx + dy * dy < nearest)
                    {
                        nearest = dx * dx + dy * dy;
                        found = &marker;
                    }
                }
                if (found != nullptr)
                {
                    isDoor = markers == &mMap3DDoors;
                    break;
                }
            }
        }
        if (found == nullptr)
        {
            mMap3DLabel->setVisible(false);
            mMap3DLabelled = nullptr;
            return;
        }

        if (found->mWidget != mMap3DLabelled)
        {
            mMap3DLabelled = found->mWidget;
            // Morrowind names an interior after its town first, such as "Balmora, Guild of Mages", so a door's
            // name leaves out everything up to the first comma. The rest says which building it is.
            std::string_view name = found->mName;
            constexpr std::string_view separator = ", ";
            if (const std::size_t comma = name.find(separator);
                isDoor && comma != std::string_view::npos && comma + separator.size() < name.size())
                name.remove_prefix(comma + separator.size());
            mMap3DLabelText->setCaption(MyGUI::TextIterator::toTagsString(MyGUI::UString(std::string(name))));
            const MyGUI::IntSize textSize = mMap3DLabelText->getTextSize();
            const int padding = getMap3DMarkerSize(6);
            mMap3DLabel->setSize(textSize.width + 2 * padding, textSize.height + padding);
        }
        // Below the marker, or above it at the bottom of the view, and always inside the view.
        const MyGUI::IntCoord marker = found->mWidget->getCoord();
        const MyGUI::IntSize view = mMap3DImage->getSize();
        const MyGUI::IntSize label = mMap3DLabel->getSize();
        const int left
            = std::clamp(marker.left + marker.width / 2 - label.width / 2, 0, std::max(0, view.width - label.width));
        int top = marker.top + marker.height + getMap3DMarkerSize(4);
        if (top + label.height > view.height)
            top = marker.top - getMap3DMarkerSize(4) - label.height;
        mMap3DLabel->setPosition(left, top);
        mMap3DLabel->setVisible(true);
    }

    void MapWindow::drawControllerTabs()
    {
        // The Local and World tabs sit on the same layer as the inventory screen's tabs, so they look the same. The
        // current cell's name goes where the inventory shows the armour rating and gold.
        MyGUI::ILayer* layer = MyGUI::LayerManager::getInstance().getByName("ControllerMenus", false);
        if (layer == nullptr)
            return;
        const MyGUI::IntSize size = layer->getSize();
        if (mControllerTabs == nullptr)
            mControllerTabs = MyGUI::Gui::getInstance().createWidget<MyGUI::Widget>(
                "", MyGUI::IntCoord(0, 0, size.width, sTabsBottom), MyGUI::Align::Default, "ControllerMenus");
        mControllerTabs->setNeedMouseFocus(false);
        mControllerTabs->setCoord(0, 0, size.width, sTabsBottom);
        while (mControllerTabs->getChildCount() > 0)
            MyGUI::Gui::getInstance().destroyWidget(mControllerTabs->getChildAt(0));

        ControllerUi::addRect(mControllerTabs, 0, ControllerUi::sTabBarHeight, size.width,
            sTabsBottom - ControllerUi::sTabBarHeight, MyGUI::Colour::Black);
        const std::string cellName = mMainWidget->castType<MyGUI::Window>()->getCaption().asUTF8();
        ControllerUi::drawTabRow(mControllerTabs, size.width, { "#{sLocal}", "#{sWorld}" },
            Settings::map().mGlobal ? 1 : 0, ControllerUi::escaped(cellName));
        mControllerTabs->setVisible(true);
    }

    void MapWindow::layoutControllerGuide()
    {
        // The button hints match the inventory screen's, which are sized for a canvas 720 units tall whatever the
        // GUI scaling factor.
        const float scale = MyGUI::RenderManager::getInstance().getViewSize().height / 720.f;
        const auto scaled = [&](int units) { return static_cast<int>(std::round(units * scale)); };
        const int glyphSize = scaled(26);
        const int rowHeight = scaled(34);

        MyGUI::Widget* box;
        MyGUI::ImageBox* image;
        MyGUI::TextBox* text;
        getWidget(box, "GuideCenter");
        getWidget(image, "GuideCenterImage");
        getWidget(text, "GuideCenterText");
        box->setProperty("Spacing", std::to_string(scaled(8)));
        image->setSize(glyphSize, glyphSize);
        text->setFontHeight(scaled(18));
        text->setTextColour(MyGUI::Colour::parse("#dfc99f"));
        box->setSize(box->getWidth(), rowHeight);

        MyGUI::Widget* client = mMainWidget->getClientWidget();
        mControllerGuide->setCoord(scaled(32), client->getHeight() - scaled(24) - rowHeight, scaled(400), rowHeight);
        mControllerGuide->castType<Gui::VBox>()->notifyChildrenSizeChanged();

        // The zoom hint goes above the center hint, and the pin hint above that, styled the same.
        const auto layoutRow = [&](MyGUI::Widget* row, const std::string& imageName, const std::string& textName,
                                   int index) {
            MyGUI::ImageBox* rowImage;
            MyGUI::TextBox* rowText;
            getWidget(rowImage, imageName);
            getWidget(rowText, textName);
            row->setProperty("Spacing", std::to_string(scaled(8)));
            rowImage->setSize(glyphSize, glyphSize);
            rowText->setFontHeight(scaled(18));
            rowText->setTextColour(MyGUI::Colour::parse("#dfc99f"));
            row->setCoord(scaled(32), client->getHeight() - scaled(24) - (index + 1) * rowHeight - index * scaled(4),
                scaled(400), rowHeight);
            row->castType<Gui::HBox>()->notifyChildrenSizeChanged();
        };
        layoutRow(mGuideZoom, "GuideZoomImage", "GuideZoomText", 1);
        layoutRow(mGuideMark, "GuideMarkImage", "GuideMarkText", 2);

        // The crosshair is four short arms around a gap, so a pin in the middle of it still shows.
        if (mCrosshair != nullptr)
        {
            while (mCrosshair->getChildCount() > 0)
                MyGUI::Gui::getInstance().destroyWidget(mCrosshair->getChildAt(0));
            const int size = scaled(28);
            const int arm = scaled(9);
            const int thickness = std::max(2, scaled(2));
            const int middle = (size - thickness) / 2;
            mCrosshair->setSize(size, size);
            const MyGUI::Colour colour = MyGUI::Colour::parse("#dfc99f");
            ControllerUi::addRect(mCrosshair, 0, middle, arm, thickness, colour);
            ControllerUi::addRect(mCrosshair, size - arm, middle, arm, thickness, colour);
            ControllerUi::addRect(mCrosshair, middle, 0, thickness, arm, colour);
            ControllerUi::addRect(mCrosshair, middle, size - arm, thickness, arm, colour);
        }
    }

    // -------------------------------------------------------------------

    EditNoteDialog::EditNoteDialog()
        : WindowModal("openmw_edit_note.layout")
    {
        getWidget(mOkButton, "OkButton");
        getWidget(mCancelButton, "CancelButton");
        getWidget(mDeleteButton, "DeleteButton");
        getWidget(mTextEdit, "TextEdit");

        mCancelButton->eventMouseButtonClick += MyGUI::newDelegate(this, &EditNoteDialog::onCancelButtonClicked);
        mOkButton->eventMouseButtonClick += MyGUI::newDelegate(this, &EditNoteDialog::onOkButtonClicked);
        mDeleteButton->eventMouseButtonClick += MyGUI::newDelegate(this, &EditNoteDialog::onDeleteButtonClicked);

        if (Settings::gui().mControllerMenus)
        {
            mControllerButtons.mA = "#{Interface:OK}";
            mControllerButtons.mB = "#{Interface:Cancel}";
        }
    }

    void EditNoteDialog::showDeleteButton(bool show)
    {
        mDeleteButton->setVisible(show);
    }

    bool EditNoteDialog::getDeleteButtonShown()
    {
        return mDeleteButton->getVisible();
    }

    void EditNoteDialog::setText(const std::string& text)
    {
        mTextEdit->setCaption(MyGUI::TextIterator::toTagsString(text));
    }

    std::string EditNoteDialog::getText()
    {
        return MyGUI::TextIterator::getOnlyText(mTextEdit->getCaption());
    }

    void EditNoteDialog::onOpen()
    {
        WindowModal::onOpen();
        center();
        MWBase::Environment::get().getWindowManager()->setKeyFocusWidget(mTextEdit);

        if (Settings::gui().mControllerMenus)
        {
            mControllerFocus = getDeleteButtonShown() ? 1 : 0;
            mOkButton->setStateSelected(true);
            mCancelButton->setStateSelected(false);
        }
    }

    void EditNoteDialog::onCancelButtonClicked(MyGUI::Widget* /*sender*/)
    {
        setVisible(false);
    }

    void EditNoteDialog::onOkButtonClicked(MyGUI::Widget* /*sender*/)
    {
        eventOkClicked();
    }

    void EditNoteDialog::onDeleteButtonClicked(MyGUI::Widget* /*sender*/)
    {
        eventDeleteClicked();
    }

    ControllerButtons* EditNoteDialog::getControllerButtons()
    {
        if (getDeleteButtonShown())
            mControllerButtons.mX = "#{Interface:Delete}";
        else
            mControllerButtons.mX.clear();
        return &mControllerButtons;
    }

    bool EditNoteDialog::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
    {
        if (arg.button == SDL_CONTROLLER_BUTTON_A)
        {
            if (getDeleteButtonShown())
            {
                if (mControllerFocus == 0)
                    onDeleteButtonClicked(mDeleteButton);
                else if (mControllerFocus == 1)
                    onOkButtonClicked(mOkButton);
                else
                    onCancelButtonClicked(mCancelButton);
            }
            else
            {
                if (mControllerFocus == 0)
                    onOkButtonClicked(mOkButton);
                else
                    onCancelButtonClicked(mCancelButton);
            }
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_B)
        {
            onCancelButtonClicked(mCancelButton);
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_X)
        {
            if (getDeleteButtonShown())
                onDeleteButtonClicked(mDeleteButton);
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT)
        {
            if (getDeleteButtonShown())
            {
                mControllerFocus = wrap(mControllerFocus, 3, -1);
                mDeleteButton->setStateSelected(mControllerFocus == 0);
                mOkButton->setStateSelected(mControllerFocus == 1);
                mCancelButton->setStateSelected(mControllerFocus == 2);
            }
            else
            {
                mControllerFocus = 0;
                mOkButton->setStateSelected(mControllerFocus == 0);
                mCancelButton->setStateSelected(mControllerFocus == 1);
            }
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT)
        {
            if (getDeleteButtonShown())
            {
                mControllerFocus = wrap(mControllerFocus, 3, 1);
                mDeleteButton->setStateSelected(mControllerFocus == 0);
                mOkButton->setStateSelected(mControllerFocus == 1);
                mCancelButton->setStateSelected(mControllerFocus == 2);
            }
            else
            {
                mControllerFocus = 1;
                mOkButton->setStateSelected(mControllerFocus == 0);
                mCancelButton->setStateSelected(mControllerFocus == 1);
            }
        }

        return true;
    }

    bool LocalMapBase::MarkerUserData::isPositionExplored() const
    {
        if (!mLocalMapRender)
            return true;
        return mLocalMapRender->isPositionExplored(nX, nY, cellX, cellY);
    }

}
