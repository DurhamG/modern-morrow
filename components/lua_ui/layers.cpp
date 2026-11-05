#include "layers.hpp"

#include <components/debug/debuglog.hpp>
#include <components/myguiplatform/scalinglayer.hpp>

namespace LuaUi
{
    size_t Layer::indexOf(std::string_view name)
    {
        for (size_t i = 0; i < count(); i++)
            if (at(i)->getName() == name)
                return i;
        return count();
    }

    void Layer::insert(size_t index, std::string_view name, Options options)
    {
        if (index > count())
            throw std::logic_error("Invalid layer index");
        if (indexOf(name) < count())
            Log(Debug::Error) << "Layer \"" << name << "\" already exists";
        else
        {
            const std::string type = options.mHeight > 0 ? "ScalingLayer" : "OverlappedLayer";
            auto layer = MyGUI::LayerManager::getInstance().createLayerAt(std::string(name), type, index);
            auto overlappedLayer = dynamic_cast<MyGUI::OverlappedLayer*>(layer);
            overlappedLayer->setPick(options.mInteractive);
            if (options.mHeight > 0)
                static_cast<MyGUIPlatform::ScalingLayer*>(overlappedLayer)->setFixedHeight(options.mHeight);
        }
    }
}
