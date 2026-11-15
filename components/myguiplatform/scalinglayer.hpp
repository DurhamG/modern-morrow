#ifndef OPENMW_COMPONENTS_MYGUIPLATFORM_SCALINGLAYER
#define OPENMW_COMPONENTS_MYGUIPLATFORM_SCALINGLAYER

#include <MyGUI_OverlappedLayer.h>

namespace MyGUIPlatform
{

    ///@brief A Layer that lays out and renders widgets in screen-relative coordinates. The "Size" property determines
    /// the size of the virtual screen,
    /// which is then upscaled to the real screen size during rendering. The aspect ratio is kept intact, adding blanks
    /// to the sides when necessary.
    ///
    /// Alternatively, the "Height" property fixes only the virtual screen's height. Its width then follows the real
    /// screen's aspect ratio, so the layer fills the whole screen and keeps a constant size relative to its height.
    class ScalingLayer final : public MyGUI::OverlappedLayer
    {
    public:
        MYGUI_RTTI_DERIVED(ScalingLayer)

        void deserialization(MyGUI::xml::ElementPtr node, MyGUI::Version version) override;

        MyGUI::ILayerItem* getLayerItemByPoint(int left, int top) const override;
        MyGUI::IntPoint getPosition(int left, int top) const override;
        void renderToTarget(MyGUI::IRenderTarget* target, bool update) override;

        void resizeView(const MyGUI::IntSize& viewSize) override;

        static float getScaleFactor(const MyGUI::IntSize& layerViewSize);

        /// Switches to a fixed virtual height. 0 goes back to the fixed size from the "Size" property.
        void setFixedHeight(int height);

        /// Converts a rectangle in this layer's coordinates to screen coordinates.
        MyGUI::IntCoord layerToScreenCoords(const MyGUI::IntCoord& coord) const;

    private:
        int mFixedHeight = 0;

        void screenToLayerCoords(int& left, int& top) const;
        void updateFixedHeightViewSize();
    };

    /// The size of a layer that is a fixed number of units high, as wide as the screen's shape makes it.
    MyGUI::IntSize fixedHeightViewSize(const MyGUI::IntSize& screen, int height);

    /// Where a box in a layer's units is on the screen, for a layer of that size drawn at a scale in the middle of the
    /// screen.
    MyGUI::IntCoord layerToScreen(
        const MyGUI::IntCoord& coord, const MyGUI::IntSize& layer, const MyGUI::IntSize& screen, float scale);

}

#endif
