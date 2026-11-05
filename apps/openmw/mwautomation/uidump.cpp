#include "uidump.hpp"

#include <cctype>
#include <cmath>
#include <string_view>

#include <MyGUI_Gui.h>
#include <MyGUI_ILayer.h>
#include <MyGUI_InputManager.h>
#include <MyGUI_TextBox.h>
#include <MyGUI_Widget.h>

namespace MWAutomation
{
    namespace
    {
        // OpenMW's layouts prefix every widget name with the address of the layout object and an underscore, such as
        // "000001F52093D720_HealthFrame". The address changes on every run, so leave it out.
        std::string_view stripLayoutPrefix(std::string_view name)
        {
            const std::size_t underscore = name.find('_');
            if (underscore != 8 && underscore != 16)
                return name;
            for (std::size_t i = 0; i < underscore; ++i)
                if (!std::isxdigit(static_cast<unsigned char>(name[i])))
                    return name;
            return name.substr(underscore + 1);
        }

        struct Context
        {
            const UiDumpOptions& mOptions;
            MyGUI::Widget* mKeyFocus;
            MyGUI::Widget* mMouseFocus;
        };

        Json describe(MyGUI::Widget* widget, const Context& context, int depth)
        {
            Json node = Json::object({ { "type", widget->getTypeName() } });
            if (!widget->getName().empty())
                node.set("name", stripLayoutPrefix(widget->getName()));
            const MyGUI::IntCoord coord = widget->getAbsoluteCoord();
            node.set("rect", Json::array({ coord.left, coord.top, coord.width, coord.height }));

            if (MyGUI::TextBox* text = widget->castType<MyGUI::TextBox>(false))
            {
                const std::string caption = text->getCaption().asUTF8();
                if (!caption.empty())
                {
                    node.set("text", caption);
                    node.set("font", text->getFontName());
                    node.set("fontHeight", text->getFontHeight());
                }
            }
            if (!widget->getVisible())
                node.set("hidden", true);
            if (widget->getAlpha() < 1.f)
                node.set("alpha", std::round(widget->getAlpha() * 100) / 100);
            if (!widget->getInheritedEnabled())
                node.set("disabled", true);
            if (widget == context.mKeyFocus)
                node.set("keyFocus", true);
            if (widget == context.mMouseFocus)
                node.set("mouseFocus", true);

            if (context.mOptions.mMaxDepth > 0 && depth >= context.mOptions.mMaxDepth)
            {
                if (widget->getChildCount() > 0)
                    node.set("childCount", widget->getChildCount());
                return node;
            }

            Json children = Json::array();
            MyGUI::EnumeratorWidgetPtr enumerator = widget->getEnumerator();
            while (enumerator.next())
            {
                MyGUI::Widget* child = enumerator.current();
                if (!context.mOptions.mHidden && !child->getVisible())
                    continue;
                children.push(describe(child, context, depth + 1));
            }
            if (!children.asArray().empty())
                node.set("children", std::move(children));
            return node;
        }

        void findByName(MyGUI::Widget* widget, const std::string& name, std::vector<MyGUI::Widget*>& results)
        {
            if (stripLayoutPrefix(widget->getName()) == name)
            {
                results.push_back(widget);
                return;
            }
            MyGUI::EnumeratorWidgetPtr enumerator = widget->getEnumerator();
            while (enumerator.next())
                findByName(enumerator.current(), name, results);
        }

        std::string describeFocus(MyGUI::Widget* widget)
        {
            if (widget == nullptr)
                return {};
            if (!widget->getName().empty())
                return std::string(stripLayoutPrefix(widget->getName()));
            return std::string(widget->getTypeName());
        }
    }

    Json dumpUi(const UiDumpOptions& options)
    {
        MyGUI::InputManager& input = MyGUI::InputManager::getInstance();
        const Context context{ options, input.getKeyFocusWidget(), input.getMouseFocusWidget() };

        std::vector<MyGUI::Widget*> roots;
        MyGUI::EnumeratorWidgetPtr enumerator = MyGUI::Gui::getInstance().getEnumerator();
        while (enumerator.next())
        {
            MyGUI::Widget* widget = enumerator.current();
            if (!options.mRoot.empty())
                findByName(widget, options.mRoot, roots);
            else if (options.mHidden || widget->getVisible())
                roots.push_back(widget);
        }

        Json widgets = Json::array();
        for (MyGUI::Widget* root : roots)
        {
            Json node = describe(root, context, 0);
            if (MyGUI::ILayer* layer = root->getLayer())
                node.set("layer", layer->getName());
            widgets.push(std::move(node));
        }

        Json result = Json::object({ { "widgets", std::move(widgets) } });
        if (context.mKeyFocus != nullptr)
            result.set("keyFocus", describeFocus(context.mKeyFocus));
        if (context.mMouseFocus != nullptr)
            result.set("mouseFocus", describeFocus(context.mMouseFocus));
        return result;
    }
}
