#include "localmap3dview.hpp"

#include <algorithm>
#include <cmath>

#include <MyGUI_Gui.h>
#include <MyGUI_ImageBox.h>
#include <MyGUI_InputManager.h>
#include <MyGUI_RotatingSkin.h>

#include <osg/Image>

#include <components/esm/util.hpp>
#include <components/misc/constants.hpp>
#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/cellstore.hpp"
#include "../mwworld/worldmodel.hpp"

#include "../mwrender/linemeetsland.hpp"
#include "../mwrender/localmap.hpp"
#include "../mwrender/map3d.hpp"
#include "../mwrender/renderingmanager.hpp"

#include "mapwindow.hpp"

namespace MWGui
{
    namespace
    {
        constexpr float sCellSize = static_cast<float>(Constants::CellSizeInUnits);

        // The size of the flat map's arrow in its layout.
        constexpr int sArrowSize = 32;
        // How far the view zooms out at most. The flat map's limit depends on its grid of cells, which the 3D view
        // doesn't have.
        constexpr float sMinZoom = 0.125f;

        // A smaller depth draws in front.
        constexpr int sArrowDepth = 0;
        constexpr int sCustomMarkerDepth = 1;
        constexpr int sDoorMarkerDepth = 2;

        // Indoors, everything this far above the player's feet is cut away, ceilings and upper floors included.
        constexpr float sHeadroom = 200.f;

        // Outdoors, the fog of war image covers a square of this many cells around the view. It is made again once
        // the view moves far enough from its middle that it could reach the image's edge.
        constexpr int sFogCells = 64;
        constexpr int sFogMargin = 16;

        std::pair<int, int> getCellAt(float x, float y)
        {
            const ESM::ExteriorCellLocation cell = ESM::positionToExteriorCellLocation(x, y);
            return { cell.mX, cell.mY };
        }

        bool isInPicture(const osg::Vec2f& picture)
        {
            return picture.x() >= 0.f && picture.x() <= 1.f && picture.y() >= 0.f && picture.y() <= 1.f;
        }
    }

    LocalMap3DView::LocalMap3DView(MyGUI::Widget* parent, const MyGUI::IntCoord& coord,
        MWRender::LocalMap* localMapRender, CustomMarkerCollection& customMarkers)
        : mLocalMapRender(localMapRender)
        , mCustomMarkers(customMarkers)
    {
        mImage = parent->createWidget<MyGUI::ImageBox>("ImageBox", coord, MyGUI::Align::Stretch);
        // Behind the window's buttons.
        mImage->setDepth(1);
        mImage->setVisible(false);
        mImage->setNeedMouseFocus(true);
        mImage->eventMouseButtonPressed += MyGUI::newDelegate(this, &LocalMap3DView::onMousePressed);
        mImage->eventMouseDrag += MyGUI::newDelegate(this, &LocalMap3DView::onMouseDrag);
        mImage->eventMouseButtonDoubleClick += MyGUI::newDelegate(this, &LocalMap3DView::onDoubleClicked);
        if (Settings::map().mAllowZooming)
            mImage->eventMouseWheel += MyGUI::newDelegate(this, &LocalMap3DView::onMouseWheel);

        mPlayerArrow = mImage->createWidget<MyGUI::ImageBox>(
            "RotatingSkin", MyGUI::IntCoord(0, 0, sArrowSize, sArrowSize), MyGUI::Align::Default);
        mPlayerArrow->setImageTexture("textures\\compass.dds");
        mPlayerArrow->setNeedMouseFocus(false);
        mPlayerArrow->setDepth(sArrowDepth);
    }

    LocalMap3DView::~LocalMap3DView()
    {
        MyGUI::Gui::getInstance().destroyWidget(mImage);
    }

    void LocalMap3DView::setVisible(bool visible)
    {
        mImage->setVisible(visible);
        updateDrawing();
    }

    void LocalMap3DView::updateDrawing()
    {
        const bool drawing
            = mImage->getInheritedVisible() && MWBase::Environment::get().getWorld()->getPlayerPtr().isInCell();
        if (drawing == mDrawing)
            return;
        mDrawing = drawing;
        if (!drawing)
        {
            if (mMap3D != nullptr)
                mMap3D->setShown(false);
            return;
        }

        updateRenderer();
        centerOnPlayer();
        // The fog of war may have changed since the view last showed.
        mFogMiddle.reset();
        updateCustomMarkers();
        updateMagicMarkers();
        mMap3D->setShown(true);
        update();
    }

    void LocalMap3DView::updateRenderer()
    {
        // The picture has a pixel for each pixel of the screen that the view covers.
        const float scale = MWBase::Environment::get().getWindowManager()->getScalingFactor();
        const int width = std::max(1, static_cast<int>(std::round(mImage->getWidth() * scale)));
        const int height = std::max(1, static_cast<int>(std::round(mImage->getHeight() * scale)));
        if (mMap3D != nullptr && mMap3D->getWidth() == width && mMap3D->getHeight() == height)
            return;

        mImage->setRenderItemTexture(nullptr);
        mTexture.reset();
        mMap3D.reset();
        mMap3D = MWBase::Environment::get().getWorld()->getRenderingManager()->createMap3D(width, height);
        mTexture = mMap3D->makeGuiTexture();
        mImage->setRenderItemTexture(mTexture.get());
        // The widget is Y-down, the picture is Y-up.
        mImage->getSubWidgetMain()->_setUVSet(MyGUI::FloatRect(0.f, 1.f, 1.f, 0.f));
        mMap3D->setShown(mDrawing);
        // The new renderer has no fog of war yet.
        mFogMiddle.reset();
    }

    void LocalMap3DView::onFrame(float dt, bool fogOfWar)
    {
        mFogOfWar = fogOfWar;
        // The window can show before the player is in a cell, such as while a game loads.
        updateDrawing();
        if (!mDrawing)
            return;
        updateRenderer();

        // The markers that move, and the fog of war of the cells the player explores, are updated as often as on the
        // flat map.
        mUpdateTimer += dt;
        if (mUpdateTimer >= LocalMapBase::sMarkerUpdateInterval)
        {
            mUpdateTimer = 0.f;
            updateMagicMarkers();
            refreshFog();
        }
        update();
    }

    void LocalMap3DView::centerOnPlayer()
    {
        mFocus = MWBase::Environment::get().getWorld()->getPlayerPtr().getRefData().getPosition().asVec3();
    }

    void LocalMap3DView::pan(int dx, int dy)
    {
        if (!mDrawing)
            return;
        const MyGUI::IntCoord image = mImage->getAbsoluteCoord();
        const MyGUI::IntPoint middle(image.left + image.width / 2, image.top + image.height / 2);
        osg::Vec3f from;
        osg::Vec3f to;
        if (pointToGround(middle, mFocus.z(), from) && pointToGround(middle + MyGUI::IntPoint(dx, dy), mFocus.z(), to))
        {
            mFocus.x() += from.x() - to.x();
            mFocus.y() += from.y() - to.y();
        }
    }

    float LocalMap3DView::getUnitsPerWidgetUnit() const
    {
        return sCellSize / Settings::map().mLocalMapWidgetSize / mZoom;
    }

    float LocalMap3DView::getCameraDistance() const
    {
        // Close to the point the view looks at, the picture shows the world at the scale of the flat map.
        return MWRender::Map3D::getDistanceToShow(getUnitsPerWidgetUnit() * static_cast<float>(mImage->getHeight()));
    }

    float LocalMap3DView::getGroundHeight(float x, float y) const
    {
        MWBase::World* world = MWBase::Environment::get().getWorld();
        if (mCell == nullptr || !mCell->isExterior())
            return world->getPlayerPtr().getRefData().getPosition().pos[2];
        return world->getTerrainHeightAt(osg::Vec3f(x, y, 0.f), mCell->getCell()->getWorldSpace());
    }

    void LocalMap3DView::update()
    {
        MWBase::World* world = MWBase::Environment::get().getWorld();
        const MWWorld::Ptr player = world->getPlayerPtr();
        if (!player.isInCell())
            return;
        const MWWorld::CellStore& cell = *player.getCell();
        if (&cell != mCell)
        {
            mCell = &cell;
            centerOnPlayer();
            mFogMiddle.reset();
            resetDoorMarkers();
            updateCustomMarkers();
            updateMagicMarkers();
        }

        // Outdoors, the view looks at the ground. Indoors, it looks at the player's feet, and cuts away what is above
        // their head.
        if (cell.isExterior())
        {
            mFocus.z() = world->getTerrainHeightAt(mFocus, cell.getCell()->getWorldSpace());
            mCeiling.reset();
        }
        else
        {
            mFocus.z() = player.getRefData().getPosition().pos[2];
            mCeiling = mFocus.z() + sHeadroom;
        }
        mMap3D->setIndoors(mCeiling);
        updateFog(cell);
        mMap3D->setView(mFocus, getCameraDistance());
        addDoorMarkers();
        placeMarkers();
        placePlayerArrow();
    }

    void LocalMap3DView::showFog(const MWRender::LocalMap::FogImage& fog)
    {
        mFogImage = fog.mImage;
        mFogToU = fog.mToU;
        mFogToV = fog.mToV;
        mMap3D->setFog(fog.mImage, fog.mToU, fog.mToV);
    }

    void LocalMap3DView::updateFog(const MWWorld::CellStore& cell)
    {
        // The console's ToggleFogOfWar turns the fog off, as on the flat map.
        if (!mFogOfWar)
        {
            if (mFogImage != nullptr)
                showFog({});
            mFogMiddle.reset();
            return;
        }

        if (!cell.isExterior())
        {
            if (!mFogMiddle.has_value())
            {
                mFogMiddle = std::pair<int, int>(0, 0);
                showFog(mLocalMapRender->makeInteriorFogImage());
            }
            return;
        }

        const std::pair<int, int> middle = getCellAt(mFocus.x(), mFocus.y());
        if (mFogMiddle.has_value() && std::abs(middle.first - mFogMiddle->first) <= sFogMargin
            && std::abs(middle.second - mFogMiddle->second) <= sFogMargin)
            return;
        mFogMiddle = middle;
        showFog(mLocalMapRender->makeExteriorFogImage(
            middle.first - sFogCells / 2, middle.second - sFogCells / 2, sFogCells, cell.getCell()->getWorldSpace()));
    }

    void LocalMap3DView::refreshFog()
    {
        // The player explores the cells around them as they move, while the map is pinned.
        if (mFogImage == nullptr || !mFogMiddle.has_value() || mCell == nullptr)
            return;
        if (mCell->isExterior())
            mLocalMapRender->updateExteriorFogImage(
                *mFogImage, mFogMiddle->first - sFogCells / 2, mFogMiddle->second - sFogCells / 2);
        else
            showFog(mLocalMapRender->makeInteriorFogImage());
    }

    bool LocalMap3DView::isExplored(const osg::Vec3f& position) const
    {
        if (mFogImage == nullptr)
            return true;
        const osg::Vec3f place(position.x(), position.y(), 1.f);
        const int s = static_cast<int>(std::floor((mFogToU * place) * mFogImage->s()));
        const int t = static_cast<int>(std::floor((mFogToV * place) * mFogImage->t()));
        if (s < 0 || t < 0 || s >= mFogImage->s() || t >= mFogImage->t())
            return false;
        return *mFogImage->data(s, t) < MWRender::LocalMap::sExploredFogLimit;
    }

    void LocalMap3DView::destroyMarkers(std::vector<Marker>& markers)
    {
        for (const Marker& marker : markers)
            MyGUI::Gui::getInstance().destroyWidget(marker.mWidget);
        markers.clear();
    }

    void LocalMap3DView::forwardMouseEvents(MyGUI::Widget* widget)
    {
        // A marker takes the mouse for its tooltip, so the view still pans and zooms when the mouse is on it.
        widget->eventMouseButtonPressed += MyGUI::newDelegate(this, &LocalMap3DView::onMousePressed);
        widget->eventMouseDrag += MyGUI::newDelegate(this, &LocalMap3DView::onMouseDrag);
        if (Settings::map().mAllowZooming)
            widget->eventMouseWheel += MyGUI::newDelegate(this, &LocalMap3DView::onMouseWheel);
    }

    void LocalMap3DView::resetDoorMarkers()
    {
        // Outdoors, the doors of the cells near the player are found again whenever the player moves to another
        // cell, since scripts can enable and disable doors. Morrowind.exe does the same when a cell joins the active
        // grid.
        const ESM::RefId space = mCell->isExterior() ? mCell->getCell()->getWorldSpace() : mCell->getCell()->getId();
        if (space != mDoorSpace)
        {
            for (auto& [cell, markers] : mDoorMarkers)
                destroyMarkers(markers);
            mDoorMarkers.clear();
            mDoorSpace = space;
            return;
        }
        if (!mCell->isExterior())
            return;
        const int cellX = mCell->getCell()->getGridX();
        const int cellY = mCell->getCell()->getGridY();
        for (int x = cellX - Constants::CellGridRadius; x <= cellX + Constants::CellGridRadius; ++x)
        {
            for (int y = cellY - Constants::CellGridRadius; y <= cellY + Constants::CellGridRadius; ++y)
            {
                const auto found = mDoorMarkers.find({ x, y });
                if (found == mDoorMarkers.end())
                    continue;
                destroyMarkers(found->second);
                mDoorMarkers.erase(found);
            }
        }
    }

    void LocalMap3DView::addDoorMarkers()
    {
        MWBase::World* world = MWBase::Environment::get().getWorld();
        MWWorld::WorldModel* worldModel = MWBase::Environment::get().getWorldModel();
        const auto addDoors = [&](MWWorld::CellStore& cell, std::vector<Marker>& markers) {
            std::vector<MWBase::World::DoorMarker> doors;
            world->getDoorMarkers(cell, doors);
            for (MWBase::World::DoorMarker& door : doors)
            {
                // Without a local map in the data, the tooltip doesn't check the flat map's fog of war. The view
                // hides the doors in the fog itself.
                LocalMapBase::MarkerUserData data(nullptr);
                data.caption = door.name;
                data.notes = LocalMapBase::getNotes(mCustomMarkers, door.dest);
                MarkerWidget* widget = LocalMapBase::createDoorMarkerWidget(mImage,
                    MyGUI::IntCoord(0, 0, LocalMapBase::sMarkerSize, LocalMapBase::sMarkerSize), std::move(data));
                widget->setDepth(sDoorMarkerDepth);
                forwardMouseEvents(widget);
                markers.push_back({ widget, osg::Vec3f(door.x, door.y, door.z) });
            }
        };

        if (!mCell->isExterior())
        {
            if (mDoorMarkers.empty())
                addDoors(worldModel->getInterior(mCell->getCell()->getNameId()), mDoorMarkers[{ 0, 0 }]);
            return;
        }

        // The cells in view, and a little past its edges, nearest first.
        const std::pair<int, int> middle = getCellAt(mFocus.x(), mFocus.y());
        const int reach = std::clamp(static_cast<int>(getCameraDistance() * 2.f / sCellSize) + 1, 1, sFogMargin);
        std::vector<std::pair<int, int>> cells;
        for (int x = middle.first - reach; x <= middle.first + reach; ++x)
            for (int y = middle.second - reach; y <= middle.second + reach; ++y)
                if (!mDoorMarkers.contains({ x, y }))
                    cells.emplace_back(x, y);
        const auto distance = [&](const std::pair<int, int>& cell) {
            const int dx = cell.first - middle.first;
            const int dy = cell.second - middle.second;
            return dx * dx + dy * dy;
        };
        std::sort(cells.begin(), cells.end(), [&](const auto& a, const auto& b) { return distance(a) < distance(b); });

        // Finding the doors of a cell that isn't loaded reads the cell from the content files, so only a few are read
        // each frame.
        constexpr int readsPerFrame = 2;
        int reads = 0;
        for (const auto& [x, y] : cells)
        {
            const ESM::ExteriorCellLocation location(x, y, mCell->getCell()->getWorldSpace());
            MWWorld::CellStore& cell = worldModel->getExterior(location, false);
            // As on the flat map, the doors show in the cells the player has explored.
            const bool loaded = cell.getState() == MWWorld::CellStore::State_Loaded;
            if (!loaded && cell.getFog() == nullptr)
                continue;
            if (!loaded && reads++ == readsPerFrame)
                break;
            addDoors(worldModel->getExterior(location), mDoorMarkers[{ x, y }]);
        }
    }

    void LocalMap3DView::updateCustomMarkers()
    {
        destroyMarkers(mCustomMarkerWidgets);
        if (mCell == nullptr)
            return;
        for (const auto& [cellId, marker] : mCustomMarkers)
        {
            // Outdoors, the view can look anywhere, so it shows the markers of every exterior cell.
            if (mCell->isExterior() ? !cellId.is<ESM::ESM3ExteriorCellRefId>() : cellId != mCell->getCell()->getId())
                continue;
            constexpr int size = LocalMapBase::sCustomMarkerSize;
            MarkerWidget* widget
                = LocalMapBase::createCustomMarkerWidget(mImage, MyGUI::IntCoord(0, 0, size, size), marker);
            widget->setDepth(sCustomMarkerDepth);
            forwardMouseEvents(widget);
            widget->eventMouseButtonDoubleClick
                += MyGUI::newDelegate(this, &LocalMap3DView::onCustomMarkerDoubleClicked);
            mCustomMarkerWidgets.push_back({ widget,
                osg::Vec3f(marker.mWorldX, marker.mWorldY, getGroundHeight(marker.mWorldX, marker.mWorldY)) });
        }
    }

    void LocalMap3DView::updateMagicMarkers()
    {
        destroyMarkers(mMagicMarkers);
        if (mCell == nullptr)
            return;

        constexpr int size = LocalMapBase::sMarkerSize;
        for (const LocalMapBase::MagicMarker& marker : LocalMapBase::getMagicMarkers(mCell->getCell()->getWorldSpace()))
        {
            MyGUI::ImageBox* widget
                = LocalMapBase::createMagicMarkerWidget(mImage, MyGUI::IntCoord(0, 0, size, size), marker);
            widget->setDepth(sCustomMarkerDepth);
            mMagicMarkers.push_back({ widget, marker.mPosition });
        }
    }

    void LocalMap3DView::placeMarkers()
    {
        const MyGUI::IntSize size = mImage->getSize();
        const auto place = [&](const Marker& marker, bool shown) {
            osg::Vec2f picture;
            shown = shown && mMap3D->worldToPicture(marker.mPosition, picture) && isInPicture(picture);
            marker.mWidget->setVisible(shown);
            if (shown)
                marker.mWidget->setPosition(static_cast<int>(picture.x() * size.width) - marker.mWidget->getWidth() / 2,
                    static_cast<int>(picture.y() * size.height) - marker.mWidget->getHeight() / 2);
        };

        // As on the flat map, the fog of war hides the doors, but not the custom markers or what magic detects.
        // Indoors, the doors above the cut hide too, since they lead from the floors that are cut away.
        for (const auto& [cell, markers] : mDoorMarkers)
            for (const Marker& marker : markers)
                place(marker,
                    isExplored(marker.mPosition) && (!mCeiling.has_value() || marker.mPosition.z() < *mCeiling));
        for (const Marker& marker : mCustomMarkerWidgets)
            place(marker, true);
        for (const Marker& marker : mMagicMarkers)
            place(marker, true);
    }

    void LocalMap3DView::placePlayerArrow()
    {
        // The arrow points the way the player faces. The view looks north, as the flat map does.
        const ESM::Position& position
            = MWBase::Environment::get().getWorld()->getPlayerPtr().getRefData().getPosition();
        osg::Vec2f picture;
        const bool shown = mMap3D->worldToPicture(position.asVec3(), picture) && isInPicture(picture);
        mPlayerArrow->setVisible(shown);
        if (!shown)
            return;
        const MyGUI::IntSize size = mImage->getSize();
        const int half = sArrowSize / 2;
        mPlayerArrow->setPosition(
            static_cast<int>(picture.x() * size.width) - half, static_cast<int>(picture.y() * size.height) - half);
        MyGUI::RotatingSkin* arrow = mPlayerArrow->getSubWidgetMain()->castType<MyGUI::RotatingSkin>();
        arrow->setCenter(MyGUI::IntPoint(half, half));
        arrow->setAngle(position.rot[2]);
    }

    bool LocalMap3DView::pointToPicture(MyGUI::IntPoint point, osg::Vec2f& picture) const
    {
        if (mMap3D == nullptr)
            return false;
        const MyGUI::IntCoord image = mImage->getAbsoluteCoord();
        if (image.width <= 0 || image.height <= 0)
            return false;
        picture = osg::Vec2f(static_cast<float>(point.left - image.left) / image.width,
            static_cast<float>(point.top - image.top) / image.height);
        return true;
    }

    bool LocalMap3DView::pointToGround(MyGUI::IntPoint point, float height, osg::Vec3f& ground) const
    {
        osg::Vec2f picture;
        return pointToPicture(point, picture) && mMap3D->pictureToGround(picture, height, ground);
    }

    bool LocalMap3DView::pointToLand(MyGUI::IntPoint point, osg::Vec3f& land) const
    {
        osg::Vec2f picture;
        if (!pointToPicture(point, picture))
            return false;
        osg::Vec3f nearPoint;
        osg::Vec3f farPoint;
        mMap3D->pictureToLine(picture, nearPoint, farPoint);
        // Checking the land once for each widget unit finds it to within a pixel or so.
        const std::optional<osg::Vec3f> found = MWRender::findWhereLineMeetsLand(
            nearPoint, farPoint, getUnitsPerWidgetUnit(), [this](float x, float y) { return getGroundHeight(x, y); });
        if (!found)
            return false;
        land = *found;
        return true;
    }

    void LocalMap3DView::onMousePressed(MyGUI::Widget* /*sender*/, int left, int top, MyGUI::MouseButton id)
    {
        if (id != MyGUI::MouseButton::Left)
            return;
        mLastDragPosition = MyGUI::IntPoint(left, top);
        // The drag moves the view on level ground at the height of the land that was under the mouse, so that land
        // stays under it.
        osg::Vec3f land;
        mDragHeight = pointToLand(mLastDragPosition, land) ? land.z() : mFocus.z();
    }

    void LocalMap3DView::onMouseDrag(MyGUI::Widget* /*sender*/, int left, int top, MyGUI::MouseButton id)
    {
        if (id != MyGUI::MouseButton::Left)
            return;
        // The ground that was under the mouse stays under it.
        const MyGUI::IntPoint point(left, top);
        osg::Vec3f from;
        osg::Vec3f to;
        if (pointToGround(mLastDragPosition, mDragHeight, from) && pointToGround(point, mDragHeight, to))
        {
            mFocus.x() += from.x() - to.x();
            mFocus.y() += from.y() - to.y();
        }
        mLastDragPosition = point;
    }

    void LocalMap3DView::onDoubleClicked(MyGUI::Widget* /*sender*/)
    {
        osg::Vec3f land;
        if (pointToLand(MyGUI::InputManager::getInstance().getMousePosition(), land))
            eventGroundDoubleClicked(land.x(), land.y());
    }

    void LocalMap3DView::onMouseWheel(MyGUI::Widget* /*sender*/, int rel)
    {
        if (mMap3D == nullptr)
            return;
        const float step = LocalMapBase::sZoomStep;
        const float zoom = std::clamp(rel < 0 ? mZoom / step : mZoom * step, sMinZoom, LocalMapBase::sMaxZoom);
        if (zoom == mZoom)
            return;

        // The ground under the mouse stays under it.
        const MyGUI::IntPoint mouse = MyGUI::InputManager::getInstance().getMousePosition();
        osg::Vec3f before;
        const bool found = pointToLand(mouse, before);
        mZoom = zoom;
        mMap3D->setView(mFocus, getCameraDistance());
        osg::Vec3f after;
        if (found && pointToGround(mouse, before.z(), after))
        {
            mFocus.x() += before.x() - after.x();
            mFocus.y() += before.y() - after.y();
        }
    }

    void LocalMap3DView::onCustomMarkerDoubleClicked(MyGUI::Widget* sender)
    {
        eventCustomMarkerDoubleClicked(sender);
    }

    void LocalMap3DView::clear()
    {
        for (auto& [cell, markers] : mDoorMarkers)
            destroyMarkers(markers);
        mDoorMarkers.clear();
        mDoorSpace = ESM::RefId();
        destroyMarkers(mCustomMarkerWidgets);
        destroyMarkers(mMagicMarkers);
        mCell = nullptr;
        mFogImage = nullptr;
        mFogMiddle.reset();
    }
}
