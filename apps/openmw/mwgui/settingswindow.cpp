#include "settingswindow.hpp"

#include <array>
#include <cmath>

#include <unicode/locid.h>

#include <MyGUI_ComboBox.h>
#include <MyGUI_Gui.h>
#include <MyGUI_ImageBox.h>
#include <MyGUI_InputManager.h>
#include <MyGUI_LanguageManager.h>
#include <MyGUI_LayerManager.h>
#include <MyGUI_ListBox.h>
#include <MyGUI_ScrollBar.h>
#include <MyGUI_ScrollView.h>
#include <MyGUI_TabControl.h>
#include <MyGUI_TabItem.h>
#include <MyGUI_UString.h>
#include <MyGUI_Window.h>

#include <SDL_video.h>

#include <components/debug/debuglog.hpp>
#include <components/files/configurationmanager.hpp>
#include <components/l10n/manager.hpp>
#include <components/lua_ui/scriptsettings.hpp>
#include <components/misc/constants.hpp>
#include <components/misc/display.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/resource/scenemanager.hpp>
#include <components/sceneutil/lightmanager.hpp>
#include <components/settings/values.hpp>
#include <components/vfs/manager.hpp>
#include <components/vfs/recursivedirectoryiterator.hpp>
#include <components/widgets/sharedstatebutton.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwlua/luamanagerimp.hpp"

#include "confirmationdialog.hpp"
#include "weightedsearch.hpp"

namespace
{
    std::string_view textureFilteringToStr(const std::string& mipFilter, const std::string& magFilter)
    {
        if (mipFilter == "none")
            return "#{OMWEngine:TextureFilteringDisabled}";

        if (magFilter == "linear")
        {
            if (mipFilter == "linear")
                return "#{OMWEngine:TextureFilteringTrilinear}";
            if (mipFilter == "nearest")
                return "#{OMWEngine:TextureFilteringBilinear}";
        }
        else if (magFilter == "nearest")
            return "#{OMWEngine:TextureFilteringNearest}";

        Log(Debug::Warning) << "Warning: Invalid texture filtering options: " << mipFilter << ", " << magFilter;
        return "#{OMWEngine:TextureFilteringOther}";
    }

    bool sortResolutions(std::pair<int, int> left, std::pair<int, int> right)
    {
        if (left.first == right.first)
            return left.second > right.second;
        return left.first > right.first;
    }

    const std::string_view checkButtonType = "CheckButton";
    const std::string_view sliderType = "Slider";

    std::string_view getSettingType(MyGUI::Widget* widget)
    {
        return widget->getUserString("SettingType");
    }

    std::string_view getSettingName(MyGUI::Widget* widget)
    {
        return widget->getUserString("SettingName");
    }

    std::string_view getSettingCategory(MyGUI::Widget* widget)
    {
        return widget->getUserString("SettingCategory");
    }

    std::string_view getSettingValueType(MyGUI::Widget* widget)
    {
        return widget->getUserString("SettingValueType");
    }

    template <class T>
    void getSettingMinMax(MyGUI::Widget* widget, T& min, T& max)
    {
        const char* settingMin = "SettingMin";
        const char* settingMax = "SettingMax";
        min = 0;
        max = 1;
        if (!widget->getUserString(settingMin).empty())
            min = MyGUI::utility::parseValue<T>(widget->getUserString(settingMin));
        if (!widget->getUserString(settingMax).empty())
            max = MyGUI::utility::parseValue<T>(widget->getUserString(settingMax));
    }

    void updateMaxLightsComboBox(MyGUI::ComboBox* box)
    {
        constexpr int min = 8;
        constexpr int max = 64;
        constexpr int increment = 8;
        const int maxLights = Settings::shaders().mMaxLights;
        // show increments of 8 in dropdown
        if (maxLights >= min && maxLights <= max && !(maxLights % increment))
            box->setIndexSelected((maxLights / increment) - 1);
        else
            box->setIndexSelected(MyGUI::ITEM_NONE);
    }

    void updateAnisotropyComboBox(MyGUI::ComboBox* box)
    {
        const int anisotropy = Settings::general().mAnisotropy;
        if (anisotropy <= 1)
            box->setIndexSelected(0);
        else if (anisotropy <= 2)
            box->setIndexSelected(1);
        else if (anisotropy <= 4)
            box->setIndexSelected(2);
        else if (anisotropy <= 8)
            box->setIndexSelected(3);
        else
            box->setIndexSelected(4);
    }

    void updateShadowMapResolutionComboBox(MyGUI::ComboBox* box)
    {
        const int resolution = Settings::shadows().mShadowMapResolution;
        if (resolution == 512)
            box->setIndexSelected(0);
        else if (resolution == 1024)
            box->setIndexSelected(1);
        else if (resolution == 2048)
            box->setIndexSelected(2);
        else if (resolution == 4096)
            box->setIndexSelected(3);
        else
        {
            box->addItem(std::to_string(resolution), resolution);
            box->setIndexSelected(4);
        }
    }

    void updateSliderLabel(MyGUI::ScrollBar* scroller, MyGUI::TextBox* textBox,
        const std::vector<icu::UnicodeString>& argNames, const std::vector<icu::Formattable>& args)
    {
        if (textBox != nullptr)
        {
            auto l10n = MWBase::Environment::get().getL10nManager()->getContext("OMWEngine");
            std::string labelCaption
                = l10n->formatMessage(scroller->getUserString("SettingLabelCaption"), argNames, args);
            textBox->setCaption(labelCaption);
        }
    }
}

namespace MWGui
{
    void SettingsWindow::configureWidgets(MyGUI::Widget* widget, bool init)
    {
        MyGUI::EnumeratorWidgetPtr widgets = widget->getEnumerator();
        while (widgets.next())
        {
            MyGUI::Widget* current = widgets.current();

            std::string_view type = getSettingType(current);
            if (type == checkButtonType)
            {
                std::string_view initialValue
                    = Settings::get<bool>(getSettingCategory(current), getSettingName(current)) ? "#{Interface:On}"
                                                                                                : "#{Interface:Off}";
                current->castType<MyGUI::Button>()->setCaptionWithReplacing(initialValue);
                if (init)
                    current->eventMouseButtonClick += MyGUI::newDelegate(this, &SettingsWindow::onButtonToggled);
            }
            if (type == sliderType)
            {
                MyGUI::ScrollBar* scroll = current->castType<MyGUI::ScrollBar>();
                std::string_view valueType = getSettingValueType(current);
                std::vector<icu::UnicodeString> argNames;
                std::vector<icu::Formattable> args;
                if (valueType == "Float" || valueType == "Cell")
                {
                    // TODO: ScrollBar isn't meant for this. should probably use a dedicated FloatSlider widget
                    float min, max;
                    getSettingMinMax(scroll, min, max);
                    float value;

                    if (valueType == "Cell")
                    {
                        value = Settings::get<float>(getSettingCategory(current), getSettingName(current));
                        argNames.emplace_back("cells");
                        args.emplace_back(value / Constants::CellSizeInUnits);
                    }
                    else
                    {
                        value = Settings::get<float>(getSettingCategory(current), getSettingName(current));
                        argNames.emplace_back("value");
                        args.emplace_back(value);
                    }

                    value = std::clamp(value, min, max);
                    value = (value - min) / (max - min);

                    scroll->setScrollPosition(static_cast<size_t>(std::round(value * (scroll->getScrollRange() - 1))));
                }
                else if (valueType == "Integer")
                {
                    int min, max;
                    getSettingMinMax(scroll, min, max);
                    const int value = Settings::get<int>(getSettingCategory(current), getSettingName(current));
                    argNames.emplace_back("value");
                    args.emplace_back(value);
                    scroll->setScrollPosition(static_cast<size_t>(std::clamp(value, min, max) - min));
                }
                else
                {
                    const int value = Settings::get<int>(getSettingCategory(current), getSettingName(current));
                    argNames.emplace_back("value");
                    args.emplace_back(value);
                    scroll->setScrollPosition(value);
                }
                if (init)
                    scroll->eventScrollChangePosition
                        += MyGUI::newDelegate(this, &SettingsWindow::onSliderChangePosition);
                if (scroll->getVisible())
                    updateSliderLabel(scroll, getSliderLabel(scroll), argNames, args);
            }

            configureWidgets(current, init);
        }
    }

    void SettingsWindow::onFrame(float duration)
    {
        if (mScriptView->getVisible())
        {
            const auto scriptsSize = mScriptAdapter->getSize();
            if (mScriptView->getCanvasSize() != scriptsSize)
                mScriptView->setCanvasSize(scriptsSize);
        }

        // Up and down move between rows, and left and right change the highlighted setting. Both repeat while held.
        int dx = 0;
        int dy = 0;
        if (mPanel != nullptr && mPanel->getVisible() && mHeldDirection.update(duration, dx, dy))
        {
            if (dy != 0 && panelItemCount() > 0)
            {
                // The list's focus counts settings only, so it skips the headings.
                mList.setFocus(std::clamp(mList.getFocus() + dy, 0, panelItemCount() - 1));
                drawPanel();
            }
            else if (dx != 0)
                changePanelRow(dx);
        }
    }

    void SettingsWindow::collectPanelRows(MyGUI::Widget* widget)
    {
        // The settings in the order the tab shows them. The parts of lists and sliders are not settings of their own.
        for (std::size_t i = 0; i < widget->getChildCount(); ++i)
        {
            MyGUI::Widget* child = widget->getChildAt(i);
            // A tab with tabs of its own, such as Video, lists all of them, each under a heading.
            if (MyGUI::TabControl* tabs = child->castType<MyGUI::TabControl>(false))
            {
                for (std::size_t item = 0; item < tabs->getItemCount(); ++item)
                {
                    mPanelRows.push_back(tabs->getItemAt(item));
                    collectPanelRows(tabs->getItemAt(item));
                }
                continue;
            }
            if (!child->getVisible())
                continue;
            if (child->isType<MyGUI::ScrollBar>())
            {
                if (getSettingType(child) == "Slider")
                    mPanelRows.push_back(child);
            }
            else if (child->isType<MyGUI::ComboBox>() || child->isType<MyGUI::ListBox>()
                || child->isType<MyGUI::Button>())
                mPanelRows.push_back(child);
            else
                collectPanelRows(child);
        }
    }

    int SettingsWindow::panelItemCount() const
    {
        int count = 0;
        for (const MyGUI::Widget* row : mPanelRows)
            count += row->isType<MyGUI::TabItem>() ? 0 : 1;
        return count;
    }

    MyGUI::Widget* SettingsWindow::panelRow(int item) const
    {
        int index = 0;
        for (MyGUI::Widget* row : mPanelRows)
        {
            if (row->isType<MyGUI::TabItem>())
                continue;
            if (index++ == item)
                return row;
        }
        return nullptr;
    }

    bool SettingsWindow::panelShowsTab()
    {
        mPanelRows.clear();
        MyGUI::Widget* page = mSettingsTab->getItemAt(mSettingsTab->getIndexSelected());
        // The key bindings and the scripts' pages are built from code, and need this window.
        const auto isOnPage = [&](MyGUI::Widget* widget) {
            for (; widget != nullptr; widget = widget->getParent())
                if (widget == page)
                    return true;
            return false;
        };
        if (page == nullptr || isOnPage(mControlsBox) || isOnPage(mScriptView))
            return false;
        collectPanelRows(page);
        return !mPanelRows.empty();
    }

    void SettingsWindow::showPanelOrWindow()
    {
        if (mPanel == nullptr)
            return;
        const bool panel = panelShowsTab();
        mMainWidget->setAlpha(panel ? 0.f : 1.f);
        if (MyGUI::Widget* client = mMainWidget->getClientWidget())
            client->setVisible(!panel);
        mPanel->setVisible(panel);
        mList.reset();
        mHeldDirection.reset();
        if (panel)
        {
            MyGUI::LayerManager::getInstance().upLayerItem(mPanel);
            drawPanel();
        }
    }

    void SettingsWindow::drawPanel()
    {
        using namespace ControllerUi;
        if (mPanel == nullptr || !mPanel->getVisible())
            return;
        clearPanel(mPanel);
        const MyGUI::IntSize size = mPanel->getSize();
        addRect(mPanel, 0, 0, size.width, size.height, MyGUI::Colour::Black, 0.97f)->setNeedMouseFocus(true);

        // The tabs, switched with the shoulder buttons.
        std::vector<std::string> captions;
        for (std::size_t i = 0; i < mSettingsTab->getItemCount(); ++i)
            captions.push_back(mSettingsTab->getItemNameAt(i).asUTF8());
        addText(
            mPanel, sMargin, 6, size.width - 2 * sMargin, 40, "#{Interface:OptionsTitle}", sFontTitle, sBrightColour);
        drawTabRow(mPanel, size.width, captions, static_cast<int>(mSettingsTab->getIndexSelected()), {});

        // A row for each setting, with its label on the left and its value on the right. The headings of a tab's own
        // tabs are rows too, but the focus skips them.
        constexpr int rowH = 46;
        const int top = sContentTop;
        const int bottom = size.height - 64;
        const int x = sMargin;
        const int w = size.width - 2 * sMargin;
        const int valueW = std::min(460, w / 2);
        const int vx = x + w - valueW - 16;
        std::vector<ControllerList::Row> rows;
        // Each row's setting, counting settings only, or -1 for a heading.
        std::vector<int> rowItems;
        int itemCount = 0;
        for (const MyGUI::Widget* row : mPanelRows)
        {
            const bool heading = row->isType<MyGUI::TabItem>();
            rows.push_back(ControllerList::Row{ rowH, !heading });
            rowItems.push_back(heading ? -1 : itemCount++);
        }
        // Each setting drawn, and the top of its row. Its value's mouse areas are added once the list is drawn.
        std::vector<std::pair<int, int>> drawnItems;
        const auto drawRow = [&](std::size_t i, int ry, bool focused) {
            MyGUI::Widget* row = mPanelRows[i];
            if (MyGUI::TabItem* heading = row->castType<MyGUI::TabItem>(false))
            {
                addText(mPanel, x + 4, ry + 8, w - 8, rowH - 14, escaped(localised(heading->getCaption().asUTF8())),
                    sFontSmall, sDimColour);
                addRect(mPanel, x + 4, ry + rowH - 8, w - 8, 1, sAccentColour, 0.25f);
                return;
            }
            drawnItems.emplace_back(rowItems[i], ry);
            addRect(mPanel, x, ry, w, rowH - 6, sPanelColour, 0.9f);
            if (focused)
            {
                addRect(mPanel, x, ry, w, rowH - 6, sAccentColour, 0.16f);
                addRect(mPanel, x, ry, 3, rowH - 6, sAccentColour);
            }

            // The label is the text beside the setting: before it for sliders and lists, after it for buttons.
            std::string label;
            MyGUI::Widget* parent = row->getParent();
            std::size_t index = 0;
            while (index < parent->getChildCount() && parent->getChildAt(index) != row)
                ++index;
            const auto isLabel = [](MyGUI::Widget* widget) {
                return widget->isType<MyGUI::TextBox>() && !widget->isType<MyGUI::Button>()
                    && !widget->isType<MyGUI::EditBox>() && widget->getVisible()
                    && !widget->castType<MyGUI::TextBox>()->getCaption().empty();
            };
            const auto labelBefore = [&](MyGUI::Widget* container, std::size_t position) {
                for (std::size_t j = position; j-- > 0;)
                    if (isLabel(container->getChildAt(j)))
                        return container->getChildAt(j)->castType<MyGUI::TextBox>()->getCaption().asUTF8();
                return std::string();
            };
            label = labelBefore(parent, index);
            if (label.empty() && row->isType<MyGUI::Button>())
                for (std::size_t j = index + 1; j < parent->getChildCount(); ++j)
                    if (isLabel(parent->getChildAt(j)))
                    {
                        label = parent->getChildAt(j)->castType<MyGUI::TextBox>()->getCaption().asUTF8();
                        break;
                    }
            // A drop-down list in a box of its own has its label before the box.
            if (label.empty() && parent->getParent() != nullptr)
            {
                MyGUI::Widget* grandparent = parent->getParent();
                std::size_t parentIndex = 0;
                while (parentIndex < grandparent->getChildCount() && grandparent->getChildAt(parentIndex) != parent)
                    ++parentIndex;
                label = labelBefore(grandparent, parentIndex);
            }
            if (row == mResolutionList)
                label = localised("#{Interface:OptionsResolution}");
            addText(mPanel, x + 16, ry, w - valueW - 32, rowH - 6, escaped(label), sFontNormal,
                focused ? sActiveColour : sBrightColour);

            if (MyGUI::ScrollBar* slider = row->castType<MyGUI::ScrollBar>(false))
            {
                const std::size_t range = slider->getScrollRange();
                const float fraction = range > 1 ? static_cast<float>(slider->getScrollPosition()) / (range - 1) : 0.f;
                addRect(mPanel, vx, ry + 16, valueW - 70, 8, MyGUI::Colour::Black, 0.6f);
                addRect(mPanel, vx, ry + 16, static_cast<int>((valueW - 70) * fraction), 8, sAccentColour);
                addText(mPanel, vx + valueW - 64, ry, 64, rowH - 6,
                    std::to_string(static_cast<int>(fraction * 100)) + "%", sFontSmall, sTextColour,
                    MyGUI::Align::Right | MyGUI::Align::VCenter);
            }
            else
            {
                std::string value;
                if (MyGUI::ComboBox* combo = row->castType<MyGUI::ComboBox>(false))
                    value = combo->getCaption().asUTF8();
                else if (MyGUI::ListBox* list = row->castType<MyGUI::ListBox>(false))
                {
                    if (list->getIndexSelected() != MyGUI::ITEM_NONE)
                        value = list->getItemNameAt(list->getIndexSelected()).asUTF8();
                }
                else if (MyGUI::Button* button = row->castType<MyGUI::Button>(false))
                    value = button->getCaption().asUTF8();
                const bool choice = !row->isType<MyGUI::Button>();
                if (choice && focused)
                {
                    addGlyph(mPanel, vx, ry + 9, 22, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
                    addGlyph(mPanel, vx + valueW - 22, ry + 9, 22, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
                }
                addText(mPanel, vx + 28, ry, valueW - 56, rowH - 6, escaped(value), sFontNormal, sTextColour,
                    MyGUI::Align::Center);
            }
        };
        // Pointing at a setting focuses it, and clicking it does what A does.
        mList.draw(
            mPanel, x, top, w, bottom - top, rows, drawRow, [this] { drawPanel(); },
            [] { pressButton(SDL_CONTROLLER_BUTTON_A); });

        // Some values have mouse areas of their own, on top of their rows. Clicking a slider's bar sets the slider to
        // that point. Clicking an arrow beside the focused setting's value changes it, as left and right on the d-pad
        // do.
        const int barW = valueW - 70;
        for (const std::pair<int, int>& drawn : drawnItems)
        {
            const int item = drawn.first;
            const int ry = drawn.second;
            const MyGUI::Widget* row = panelRow(item);
            if (row == nullptr)
                continue;
            if (row->isType<MyGUI::ScrollBar>())
            {
                if (barW > 0)
                    addPointerTarget(
                        mPanel, vx, ry, barW, rowH - 6,
                        [this, item] {
                            if (mList.getFocus() == item)
                                return;
                            mList.setFocus(item);
                            drawPanel();
                        },
                        [this, item, vx, barW] { clickPanelSlider(item, vx, barW); });
            }
            else if (!row->isType<MyGUI::Button>() && item == mList.getFocus())
            {
                addPointerTarget(mPanel, vx, ry, 28, rowH - 6, nullptr, [this, item] {
                    mList.setFocus(item);
                    changePanelRow(-1);
                });
                addPointerTarget(mPanel, vx + valueW - 28, ry, 28, rowH - 6, nullptr, [this, item] {
                    mList.setFocus(item);
                    changePanelRow(1);
                });
            }
        }

        // The button hints along the bottom.
        int hx = sMargin;
        const int hintY = size.height - 48;
        const auto hint = [&](SDL_GameControllerButton button, const std::string& caption) {
            addGlyph(mPanel, hx, hintY + 4, 22, button);
            MyGUI::TextBox* text = addText(mPanel, hx + 30, hintY, 260, 30, caption, sFontNormal, sBrightColour);
            const int hintW = 30 + text->getTextSize().width;
            // Clicking a button's hint presses it. The d-pad's hint stands for two directions, so it isn't one.
            if (button != SDL_CONTROLLER_BUTTON_DPAD_LEFT)
                addPointerTarget(mPanel, hx, hintY, hintW, 30, nullptr, [button] { pressButton(button); });
            hx += hintW + 22;
        };
        hint(SDL_CONTROLLER_BUTTON_DPAD_LEFT, "#{Interface:OptionsChange}");
        hint(SDL_CONTROLLER_BUTTON_A, "#{Interface:Select}");
    }

    void SettingsWindow::changePanelRow(int delta)
    {
        MyGUI::Widget* row = panelRow(mList.getFocus());
        if (row == nullptr)
            return;
        // Each change goes through the same event as a click, so the window applies it as before.
        if (MyGUI::ScrollBar* slider = row->castType<MyGUI::ScrollBar>(false))
        {
            const int range = static_cast<int>(slider->getScrollRange());
            const int step = std::max(1, range / 50);
            const int position = std::clamp(static_cast<int>(slider->getScrollPosition()) + delta * step, 0, range - 1);
            slider->setScrollPosition(position);
            slider->eventScrollChangePosition(slider, position);
        }
        else if (MyGUI::ComboBox* combo = row->castType<MyGUI::ComboBox>(false))
        {
            const std::size_t count = combo->getItemCount();
            if (count == 0)
                return;
            const std::size_t current = combo->getIndexSelected() == MyGUI::ITEM_NONE ? 0 : combo->getIndexSelected();
            const std::size_t next = wrap(current, count, delta);
            combo->setIndexSelected(next);
            combo->eventComboChangePosition(combo, next);
        }
        else if (MyGUI::ListBox* list = row->castType<MyGUI::ListBox>(false))
        {
            const std::size_t count = list->getItemCount();
            if (count == 0)
                return;
            const std::size_t current = list->getIndexSelected() == MyGUI::ITEM_NONE ? 0 : list->getIndexSelected();
            const std::size_t next = wrap(current, count, delta);
            list->setIndexSelected(next);
            list->eventListChangePosition(list, next);
        }
        else if (getSettingType(row) == "CheckButton")
            activatePanelRow();
        else
            return;
        MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
        drawPanel();
    }

    void SettingsWindow::clickPanelSlider(int item, int barX, int barWidth)
    {
        mList.setFocus(item);
        MyGUI::Widget* row = panelRow(item);
        MyGUI::ScrollBar* slider = row != nullptr ? row->castType<MyGUI::ScrollBar>(false) : nullptr;
        if (slider == nullptr || barWidth <= 0)
            return;
        // The point clicked along the bar, from 0 at its left end to 1 at its right end.
        const int mouseX = MyGUI::InputManager::getInstance().getMousePosition().left - mPanel->getAbsoluteLeft();
        const float fraction = std::clamp(static_cast<float>(mouseX - barX) / barWidth, 0.f, 1.f);
        const int range = static_cast<int>(slider->getScrollRange());
        const int position
            = std::clamp(static_cast<int>(std::lround(fraction * (range - 1))), 0, std::max(0, range - 1));
        // The change goes through the same event as dragging the slider, so the window applies it as before.
        slider->setScrollPosition(position);
        slider->eventScrollChangePosition(slider, position);
        MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
        drawPanel();
    }

    void SettingsWindow::activatePanelRow()
    {
        MyGUI::Widget* row = panelRow(mList.getFocus());
        if (row == nullptr)
            return;
        if (row->isType<MyGUI::Button>())
            row->eventMouseButtonClick(row);
        else if (MyGUI::ListBox* list = row->castType<MyGUI::ListBox>(false))
        {
            if (list->getIndexSelected() != MyGUI::ITEM_NONE)
                list->eventListSelectAccept(list, list->getIndexSelected());
        }
        MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
        // A setting can show or hide others, such as the shadow settings.
        const int focus = mList.getFocus();
        showPanelOrWindow();
        mList.setFocus(focus);
        drawPanel();
    }

    MyGUI::TextBox* SettingsWindow::getSliderLabel(MyGUI::ScrollBar* scroller) const
    {
        auto labelWidgetName = scroller->getUserString("SettingLabelWidget");
        if (!labelWidgetName.empty())
        {
            MyGUI::TextBox* textBox;
            getWidget(textBox, labelWidgetName);
            return textBox;
        }
        return nullptr;
    }

    SettingsWindow::SettingsWindow(Files::ConfigurationManager& cfgMgr)
        : WindowBase("openmw_settings_window.layout")
        , mKeyboardMode(true)
        , mCurrentPage(static_cast<size_t>(-1))
        , mCfgMgr(cfgMgr)
    {
        const bool terrain = Settings::terrain().mDistantTerrain;
        const std::string_view widgetName = terrain ? "RenderingDistanceSlider" : "LargeRenderingDistanceSlider";
        MyGUI::Widget* unusedSlider;
        getWidget(unusedSlider, widgetName);
        unusedSlider->setVisible(false);

        configureWidgets(mMainWidget, true);

        setTitle("#{OMWEngine:SettingsWindow}");

        getWidget(mSettingsTab, "SettingsTab");
        getWidget(mOkButton, "OkButton");
        getWidget(mResolutionList, "ResolutionList");
        getWidget(mWindowModeList, "WindowModeList");
        getWidget(mVSyncModeList, "VSyncModeList");
        getWidget(mWindowBorderButton, "WindowBorderButton");
        getWidget(mTextureFilteringButton, "TextureFilteringButton");
        getWidget(mAnisotropy, "Anisotropy");
        getWidget(mControlsBox, "ControlsBox");
        getWidget(mResetControlsButton, "ResetControlsButton");
        getWidget(mKeyboardSwitch, "KeyboardButton");
        getWidget(mControllerSwitch, "ControllerButton");
        getWidget(mWaterRefractionButton, "WaterRefractionButton");
        getWidget(mWaterTextureSize, "WaterTextureSize");
        getWidget(mWaterReflectionDetail, "WaterReflectionDetail");
        getWidget(mWaterRainRippleDetail, "WaterRainRippleDetail");
        getWidget(mPrimaryLanguage, "PrimaryLanguage");
        getWidget(mSecondaryLanguage, "SecondaryLanguage");
        getWidget(mGmstOverridesL10n, "GmstOverridesL10nButton");
        getWidget(mWindowModeHint, "WindowModeHint");
        getWidget(mClusteredLightingButton, "ClusteredLightingButton");
        getWidget(mLightsResetButton, "LightsResetButton");
        getWidget(mMaxLights, "MaxLights");
        getWidget(mScriptFilter, "ScriptFilter");
        getWidget(mScriptList, "ScriptList");
        getWidget(mScriptBox, "ScriptBox");
        getWidget(mScriptView, "ScriptView");
        getWidget(mScriptAdapter, "ScriptAdapter");
        getWidget(mScriptDisabled, "ScriptDisabled");
        getWidget(mClassicFalloffWidget, "ClassicFalloffWidget");
        getWidget(mMinimumBrightnessText, "MinimumBrightnessText");
        getWidget(mMinimumBrightnessScroll, "MinimumBrightnessScroll");
        getWidget(mActorShadowsButton, "ActorShadowsButton");
        getWidget(mPlayerShadowsButton, "PlayerShadowsButton");
        getWidget(mTerrainShadowsButton, "TerrainShadowsButton");
        getWidget(mObjectShadowsButton, "ObjectShadowsButton");
        getWidget(mShadowPlanesComputationMethod, "ShadowPlanesComputationMethod");
        getWidget(mShadowMapResolution, "ShadowMapResolution");

#ifndef WIN32
        // hide gamma controls since it currently does not work under Linux
        MyGUI::ScrollBar* gammaSlider;
        getWidget(gammaSlider, "GammaSlider");
        gammaSlider->setVisible(false);
        MyGUI::TextBox* textBox;
        getWidget(textBox, "GammaText");
        textBox->setVisible(false);
        getWidget(textBox, "GammaTextDark");
        textBox->setVisible(false);
        getWidget(textBox, "GammaTextLight");
        textBox->setVisible(false);
#endif

        mMainWidget->castType<MyGUI::Window>()->eventWindowChangeCoord
            += MyGUI::newDelegate(this, &SettingsWindow::onWindowResize);

        mSettingsTab->eventTabChangeSelect += MyGUI::newDelegate(this, &SettingsWindow::onTabChanged);
        mOkButton->eventMouseButtonClick += MyGUI::newDelegate(this, &SettingsWindow::onOkButtonClicked);
        mTextureFilteringButton->eventComboChangePosition
            += MyGUI::newDelegate(this, &SettingsWindow::onTextureFilteringChanged);
        mAnisotropy->eventComboChangePosition += MyGUI::newDelegate(this, &SettingsWindow::onAnisotropyChanged);
        mResolutionList->eventListChangePosition += MyGUI::newDelegate(this, &SettingsWindow::onResolutionSelected);

        mWaterTextureSize->eventComboChangePosition
            += MyGUI::newDelegate(this, &SettingsWindow::onWaterTextureSizeChanged);
        mWaterReflectionDetail->eventComboChangePosition
            += MyGUI::newDelegate(this, &SettingsWindow::onWaterReflectionDetailChanged);
        mWaterRainRippleDetail->eventComboChangePosition
            += MyGUI::newDelegate(this, &SettingsWindow::onWaterRainRippleDetailChanged);

        mLightsResetButton->eventMouseButtonClick
            += MyGUI::newDelegate(this, &SettingsWindow::onLightsResetButtonClicked);
        mMaxLights->eventComboChangePosition += MyGUI::newDelegate(this, &SettingsWindow::onMaxLightsChanged);

        mActorShadowsButton->eventMouseButtonClick += MyGUI::newDelegate(this, &SettingsWindow::onShadowsButtonClicked);
        mPlayerShadowsButton->eventMouseButtonClick
            += MyGUI::newDelegate(this, &SettingsWindow::onShadowsButtonClicked);
        mTerrainShadowsButton->eventMouseButtonClick
            += MyGUI::newDelegate(this, &SettingsWindow::onShadowsButtonClicked);
        mObjectShadowsButton->eventMouseButtonClick
            += MyGUI::newDelegate(this, &SettingsWindow::onShadowsButtonClicked);
        mShadowPlanesComputationMethod->eventComboChangePosition
            += MyGUI::newDelegate(this, &SettingsWindow::onComputeSceneBoundsChanged);
        mShadowMapResolution->eventComboChangePosition
            += MyGUI::newDelegate(this, &SettingsWindow::onShadowMapResolutionChanged);

        mWindowModeList->eventComboChangePosition += MyGUI::newDelegate(this, &SettingsWindow::onWindowModeChanged);
        mVSyncModeList->eventComboChangePosition += MyGUI::newDelegate(this, &SettingsWindow::onVSyncModeChanged);

        mKeyboardSwitch->eventMouseButtonClick += MyGUI::newDelegate(this, &SettingsWindow::onKeyboardSwitchClicked);
        mControllerSwitch->eventMouseButtonClick
            += MyGUI::newDelegate(this, &SettingsWindow::onControllerSwitchClicked);

        mPrimaryLanguage->eventComboChangePosition
            += MyGUI::newDelegate(this, &SettingsWindow::onPrimaryLanguageChanged);
        mSecondaryLanguage->eventComboChangePosition
            += MyGUI::newDelegate(this, &SettingsWindow::onSecondaryLanguageChanged);
        mGmstOverridesL10n->eventMouseButtonClick
            += MyGUI::newDelegate(this, &SettingsWindow::onGmstOverridesL10nChanged);

        computeMinimumWindowSize();

        center();

        mResetControlsButton->eventMouseButtonClick
            += MyGUI::newDelegate(this, &SettingsWindow::onResetDefaultBindings);

        // fill resolution list
        const int screen = Settings::video().mScreen;
        int numDisplayModes = SDL_GetNumDisplayModes(screen);
        std::vector<std::pair<int, int>> resolutions;
        for (int i = 0; i < numDisplayModes; i++)
        {
            SDL_DisplayMode mode;
            SDL_GetDisplayMode(screen, i, &mode);
            resolutions.emplace_back(mode.w, mode.h);
        }
        std::sort(resolutions.begin(), resolutions.end(), sortResolutions);
        for (std::pair<int, int>& resolution : resolutions)
        {
            std::string str = Misc::getResolutionText(resolution.first, resolution.second);

            if (mResolutionList->findItemIndexWith(str) == MyGUI::ITEM_NONE)
                mResolutionList->addItem(str, resolution);
        }
        highlightCurrentResolution();

        mTextureFilteringButton->setCaptionWithReplacing(
            textureFilteringToStr(Settings::general().mTextureMipmap, Settings::general().mTextureMinFilter));
        updateAnisotropyComboBox(mAnisotropy);

        int waterTextureSize = Settings::water().mRttSize;
        if (waterTextureSize >= 512)
            mWaterTextureSize->setIndexSelected(0);
        if (waterTextureSize >= 1024)
            mWaterTextureSize->setIndexSelected(1);
        if (waterTextureSize >= 2048)
            mWaterTextureSize->setIndexSelected(2);

        const int waterReflectionDetail = Settings::water().mReflectionDetail;
        mWaterReflectionDetail->setIndexSelected(waterReflectionDetail);

        const int waterRainRippleDetail = Settings::water().mRainRippleDetail;
        mWaterRainRippleDetail->setIndexSelected(waterRainRippleDetail);

        updateMaxLightsComboBox(mMaxLights);

        const std::string& shadowMethod = Settings::shadows().mComputeSceneBounds;
        if (shadowMethod == "bounds")
            mShadowPlanesComputationMethod->setIndexSelected(0);
        else if (shadowMethod == "primitives")
            mShadowPlanesComputationMethod->setIndexSelected(1);
        else
            mShadowPlanesComputationMethod->setIndexSelected(2);

        updateShadowMapResolutionComboBox(mShadowMapResolution);

        const Settings::WindowMode windowMode = Settings::video().mWindowMode;
        mWindowBorderButton->setEnabled(
            windowMode != Settings::WindowMode::Fullscreen && windowMode != Settings::WindowMode::WindowedFullscreen);

        mWindowModeHint->setVisible(windowMode == Settings::WindowMode::WindowedFullscreen);

        mKeyboardSwitch->setStateSelected(true);
        mControllerSwitch->setStateSelected(false);

        mScriptFilter->eventEditTextChange += MyGUI::newDelegate(this, &SettingsWindow::onScriptFilterChange);
        mScriptList->eventListMouseItemActivate += MyGUI::newDelegate(this, &SettingsWindow::onScriptListSelection);

        std::vector<std::string> availableLanguages;
        const VFS::Manager* vfs = MWBase::Environment::get().getResourceSystem()->getVFS();
        constexpr VFS::Path::NormalizedView l10n("l10n/");
        for (const auto& path : vfs->getRecursiveDirectoryIterator(l10n))
        {
            if (path.extension() == "yaml")
            {
                std::string_view localeName(path.stem());
                if (localeName == "gmst")
                    continue; // fake locale to get gmst strings from content files
                if (std::find(availableLanguages.begin(), availableLanguages.end(), localeName)
                    == availableLanguages.end())
                    availableLanguages.emplace_back(localeName);
            }
        }

        std::sort(availableLanguages.begin(), availableLanguages.end());

        std::vector<std::string> currentLocales = Settings::general().mPreferredLocales;
        if (currentLocales.empty())
            currentLocales.push_back("en");

        icu::Locale primaryLocale(currentLocales[0].c_str());

        mPrimaryLanguage->removeAllItems();
        mPrimaryLanguage->setIndexSelected(MyGUI::ITEM_NONE);

        mSecondaryLanguage->removeAllItems();
        mSecondaryLanguage->addItem(
            MyGUI::LanguageManager::getInstance().replaceTags("#{Interface:None}"), std::string());
        mSecondaryLanguage->setIndexSelected(0);

        size_t i = 0;
        for (const auto& language : availableLanguages)
        {
            icu::Locale locale(language.c_str());

            icu::UnicodeString str(language.c_str());
            locale.getDisplayName(primaryLocale, str);
            std::string localeString;
            str.toUTF8String(localeString);

            mPrimaryLanguage->addItem(localeString, language);
            mSecondaryLanguage->addItem(localeString, language);

            if (language == currentLocales[0])
                mPrimaryLanguage->setIndexSelected(i);
            if (currentLocales.size() > 1 && language == currentLocales[1])
                mSecondaryLanguage->setIndexSelected(i + 1);

            i++;
        }

        mControllerButtons.mA = "#{Interface:Select}";
        mControllerButtons.mB = "#{Interface:OK}";
        mControllerButtons.mLStick = "#{Interface:Mouse}";
    }

    void SettingsWindow::onTabChanged(MyGUI::TabControl* /*sender*/, size_t /*index*/)
    {
        resetScrollbars();
    }

    void SettingsWindow::onOkButtonClicked(MyGUI::Widget* /*sender*/)
    {
        MWBase::Environment::get().getWindowManager()->toggleSettingsWindow();
    }

    void SettingsWindow::onResolutionSelected(MyGUI::ListBox* /*sender*/, size_t index)
    {
        if (index == MyGUI::ITEM_NONE)
            return;

        ConfirmationDialog* dialog = MWBase::Environment::get().getWindowManager()->getConfirmationDialog();
        dialog->askForConfirmation("#{OMWEngine:ConfirmResolution}");
        dialog->eventOkClicked.clear();
        dialog->eventOkClicked += MyGUI::newDelegate(this, &SettingsWindow::onResolutionAccept);
        dialog->eventCancelClicked.clear();
        dialog->eventCancelClicked += MyGUI::newDelegate(this, &SettingsWindow::onResolutionCancel);
    }

    void SettingsWindow::onResolutionAccept()
    {
        auto resolution = mResolutionList->getItemDataAt<std::pair<int, int>>(mResolutionList->getIndexSelected());
        if (resolution)
        {
            Settings::video().mResolutionX.set(resolution->first);
            Settings::video().mResolutionY.set(resolution->second);

            apply();
        }
    }

    void SettingsWindow::onResolutionCancel()
    {
        highlightCurrentResolution();
    }

    void SettingsWindow::highlightCurrentResolution()
    {
        mResolutionList->setIndexSelected(MyGUI::ITEM_NONE);

        const int currentX = Settings::video().mResolutionX;
        const int currentY = Settings::video().mResolutionY;

        for (size_t i = 0; i < mResolutionList->getItemCount(); ++i)
        {
            auto resolution = mResolutionList->getItemDataAt<std::pair<int, int>>(i);
            if (resolution && resolution->first == currentX && resolution->second == currentY)
            {
                mResolutionList->setIndexSelected(i);
                break;
            }
        }
    }

    void SettingsWindow::onWaterTextureSizeChanged(MyGUI::ComboBox* /*sender*/, size_t pos)
    {
        int size = 0;
        if (pos == 0)
            size = 512;
        else if (pos == 1)
            size = 1024;
        else if (pos == 2)
            size = 2048;
        Settings::water().mRttSize.set(size);
        apply();
    }

    void SettingsWindow::onWaterReflectionDetailChanged(MyGUI::ComboBox* /*sender*/, size_t pos)
    {
        Settings::water().mReflectionDetail.set(static_cast<int>(pos));
        apply();
    }

    void SettingsWindow::onWaterRainRippleDetailChanged(MyGUI::ComboBox* /*sender*/, size_t pos)
    {
        Settings::water().mRainRippleDetail.set(static_cast<int>(pos));
        apply();
    }

    void SettingsWindow::onLanguageChanged(size_t langPriority, MyGUI::ComboBox* sender, size_t pos)
    {
        if (pos == MyGUI::ITEM_NONE)
            return;

        sender->setCaptionWithReplacing(sender->getItemNameAt(sender->getIndexSelected()));

        MWBase::Environment::get().getWindowManager()->interactiveMessageBox(
            "#{OMWEngine:ChangeRequiresRestart}", { "#{Interface:OK}" }, true);

        std::vector<std::string> currentLocales = Settings::general().mPreferredLocales;
        if (currentLocales.size() <= langPriority)
            currentLocales.resize(langPriority + 1, "en");

        const auto& languageCode = *sender->getItemDataAt<std::string>(pos);
        if (!languageCode.empty())
            currentLocales[langPriority] = languageCode;
        else
            currentLocales.resize(1);

        Settings::general().mPreferredLocales.set(currentLocales);
    }

    void SettingsWindow::onGmstOverridesL10nChanged(MyGUI::Widget*)
    {
        MWBase::Environment::get().getWindowManager()->interactiveMessageBox(
            "#{OMWEngine:ChangeRequiresRestart}", { "#{Interface:OK}" }, true);
    }

    void SettingsWindow::onVSyncModeChanged(MyGUI::ComboBox* sender, size_t pos)
    {
        if (pos == MyGUI::ITEM_NONE)
            return;

        Settings::video().mVsyncMode.set(static_cast<SDLUtil::VSyncMode>(sender->getIndexSelected()));
        apply();
    }

    void SettingsWindow::onWindowModeChanged(MyGUI::ComboBox* sender, size_t pos)
    {
        if (pos == MyGUI::ITEM_NONE)
            return;

        const Settings::WindowMode windowMode = static_cast<Settings::WindowMode>(sender->getIndexSelected());
        if (windowMode == Settings::WindowMode::WindowedFullscreen)
        {
            mResolutionList->setEnabled(false);
            mWindowModeHint->setVisible(true);
        }
        else
        {
            mResolutionList->setEnabled(true);
            mWindowModeHint->setVisible(false);
        }

        if (windowMode == Settings::WindowMode::Windowed)
            mWindowBorderButton->setEnabled(true);
        else
            mWindowBorderButton->setEnabled(false);

        Settings::video().mWindowMode.set(windowMode);
        apply();
    }

    void SettingsWindow::onMaxLightsChanged(MyGUI::ComboBox* /*sender*/, size_t pos)
    {
        Settings::shaders().mMaxLights.set(8 * static_cast<int>(pos + 1));
        apply();
        configureWidgets(mMainWidget, false);
    }

    void SettingsWindow::onLightsResetButtonClicked(MyGUI::Widget* /*sender*/)
    {
        std::vector<std::string> buttons = { "#{Interface:Yes}", "#{Interface:No}" };
        MWBase::Environment::get().getWindowManager()->interactiveMessageBox(
            "#{OMWEngine:LightingResetToDefaults}", buttons, true);
        int selectedButton = MWBase::Environment::get().getWindowManager()->readPressedButton();
        if (selectedButton == 1 || selectedButton == -1)
            return;

        Settings::shaders().mForcePerPixelLighting.reset();
        Settings::shaders().mClassicFalloff.reset();
        Settings::shaders().mParticlePointLighting.reset();
        Settings::shaders().mClampLighting.reset();
        Settings::shaders().mMatchSunlightToSun.reset();
        Settings::shaders().mLightRadiusMultiplier.reset();
        Settings::shaders().mMaximumLightDistance.reset();
        Settings::shaders().mLightFadeStart.reset();
        Settings::shaders().mMinimumInteriorBrightness.reset();
        Settings::shaders().mMaxLights.reset();
        Settings::shaders().mClusteredLighting.reset();

        updateMaxLightsComboBox(mMaxLights);

        apply();
        configureWidgets(mMainWidget, false);
    }

    void SettingsWindow::onButtonToggled(MyGUI::Widget* sender)
    {
        const std::string on = MWBase::Environment::get().getL10nManager()->getMessage("Interface", "On");
        const std::string off = MWBase::Environment::get().getL10nManager()->getMessage("Interface", "Off");
        bool newState;
        if (sender->castType<MyGUI::Button>()->getCaption() == on)
        {
            sender->castType<MyGUI::Button>()->setCaption(MyGUI::UString(off));
            newState = false;
        }
        else
        {
            sender->castType<MyGUI::Button>()->setCaption(MyGUI::UString(on));
            newState = true;
        }

        if (getSettingType(sender) == checkButtonType)
        {
            Settings::get<bool>(getSettingCategory(sender), getSettingName(sender)).set(newState);
            apply();
            return;
        }
    }

    void SettingsWindow::onTextureFilteringChanged(MyGUI::ComboBox* /*sender*/, size_t pos)
    {
        auto& generalSettings = Settings::general();
        switch (pos)
        {
            case 0: // Bilinear with mips
                generalSettings.mTextureMipmap.set("nearest");
                generalSettings.mTextureMagFilter.set("linear");
                generalSettings.mTextureMinFilter.set("linear");
                break;
            case 1: // Trilinear with mips
                generalSettings.mTextureMipmap.set("linear");
                generalSettings.mTextureMagFilter.set("linear");
                generalSettings.mTextureMinFilter.set("linear");
                break;
            default:
                Log(Debug::Warning) << "Unexpected texture filtering option pos " << pos;
                break;
        }

        apply();
    }

    void SettingsWindow::onAnisotropyChanged(MyGUI::ComboBox* /*sender*/, size_t pos)
    {
        if (pos == MyGUI::ITEM_NONE)
            return;

        auto& generalSettings = Settings::general();
        switch (pos)
        {
            case 0:
                generalSettings.mAnisotropy.set(0);
                break;
            case 1:
                generalSettings.mAnisotropy.set(2);
                break;
            case 2:
                generalSettings.mAnisotropy.set(4);
                break;
            case 3:
                generalSettings.mAnisotropy.set(8);
                break;
            case 4:
                generalSettings.mAnisotropy.set(16);
                break;
            default:
                Log(Debug::Warning) << "Unexpected anisotropy option pos " << pos;
                break;
        }

        apply();
    }

    void SettingsWindow::onShadowsButtonClicked(MyGUI::Widget* /*sender*/)
    {
        auto& shadowSettings = Settings::shadows();
        shadowSettings.mEnableShadows.set(shadowSettings.mActorShadows || shadowSettings.mPlayerShadows
            || shadowSettings.mTerrainShadows || shadowSettings.mObjectShadows);
        apply();
    }

    void SettingsWindow::onComputeSceneBoundsChanged(MyGUI::ComboBox* sender, size_t pos)
    {
        if (pos == MyGUI::ITEM_NONE)
            return;

        if (pos == 1 && mShowShadowPlanesMethodWarning)
        {
            MWBase::Environment::get().getWindowManager()->interactiveMessageBox(
                "#{OMWEngine:ShadowPlanesComputationMethodWarning}", { "#{Interface:OK}" }, true);
            mShowShadowPlanesMethodWarning = false;
        }

        auto& shadowSettings = Settings::shadows();
        switch (pos)
        {
            case 0:
                shadowSettings.mComputeSceneBounds.set("bounds");
                break;
            case 1:
                shadowSettings.mComputeSceneBounds.set("primitives");
                break;
            case 2:
                shadowSettings.mComputeSceneBounds.set("none");
                break;
            default:
                Log(Debug::Warning) << "Unexpected shadow planes computation method pos " << pos;
                break;
        }

        apply();
    }

    void SettingsWindow::onShadowMapResolutionChanged(MyGUI::ComboBox* sender, size_t pos)
    {
        if (pos == MyGUI::ITEM_NONE)
            return;

        auto& shadowSettings = Settings::shadows();
        switch (pos)
        {
            case 0:
                shadowSettings.mShadowMapResolution.set(512);
                break;
            case 1:
                shadowSettings.mShadowMapResolution.set(1024);
                break;
            case 2:
                shadowSettings.mShadowMapResolution.set(2048);
                break;
            case 3:
                shadowSettings.mShadowMapResolution.set(4096);
                break;
            case 4:
            {
                int* resolution = sender->getItemDataAt<int>(4);
                if (resolution)
                    shadowSettings.mShadowMapResolution.set(*resolution);
                break;
            }
            default:
                Log(Debug::Warning) << "Unexpected shadow map resolution pos " << pos;
                break;
        }

        apply();
    }

    void SettingsWindow::onResChange(int /*width*/, int /*height*/)
    {
        center();
        highlightCurrentResolution();
    }

    void SettingsWindow::onSliderChangePosition(MyGUI::ScrollBar* scroller, size_t pos)
    {
        if (getSettingType(scroller) == "Slider")
        {
            std::vector<icu::UnicodeString> argNames;
            std::vector<icu::Formattable> args;
            std::string_view valueType = getSettingValueType(scroller);
            if (valueType == "Float" || valueType == "Cell")
            {
                float value = pos / float(scroller->getScrollRange() - 1);

                float min, max;
                getSettingMinMax(scroller, min, max);
                value = min + (max - min) * value;

                if (valueType == "Cell")
                {
                    Settings::get<float>(getSettingCategory(scroller), getSettingName(scroller)).set(value);
                    argNames.emplace_back("cells");
                    args.emplace_back(value / Constants::CellSizeInUnits);
                }
                else
                {
                    Settings::get<float>(getSettingCategory(scroller), getSettingName(scroller)).set(value);
                    argNames.emplace_back("value");
                    args.emplace_back(value);
                }
            }
            else if (valueType == "Integer")
            {
                int min, max;
                getSettingMinMax(scroller, min, max);
                const int value = min + static_cast<int>(pos);
                Settings::get<int>(getSettingCategory(scroller), getSettingName(scroller)).set(value);
                argNames.emplace_back("value");
                args.emplace_back(value);
            }
            else
            {
                int intValue = static_cast<int>(pos);
                Settings::get<int>(getSettingCategory(scroller), getSettingName(scroller)).set(intValue);
                argNames.emplace_back("value");
                args.emplace_back(intValue);
            }
            updateSliderLabel(scroller, getSliderLabel(scroller), argNames, args);

            apply();
        }
    }

    void SettingsWindow::apply()
    {
        const Settings::CategorySettingVector changed = Settings::Manager::getPendingChanges();
        MWBase::Environment::get().getWorld()->processChangedSettings(changed);
        MWBase::Environment::get().getSoundManager()->processChangedSettings(changed);
        MWBase::Environment::get().getWindowManager()->processChangedSettings(changed);
        MWBase::Environment::get().getInputManager()->processChangedSettings(changed);
        MWBase::Environment::get().getMechanicsManager()->processChangedSettings(changed);
        Settings::Manager::resetPendingChanges();

        updateLightSettings();
    }

    void SettingsWindow::onKeyboardSwitchClicked(MyGUI::Widget* /*sender*/)
    {
        if (mKeyboardMode)
            return;
        mKeyboardMode = true;
        mKeyboardSwitch->setStateSelected(true);
        mControllerSwitch->setStateSelected(false);
        updateControlsBox();
        resetScrollbars();
    }

    void SettingsWindow::onControllerSwitchClicked(MyGUI::Widget* /*sender*/)
    {
        if (!mKeyboardMode)
            return;
        mKeyboardMode = false;
        mKeyboardSwitch->setStateSelected(false);
        mControllerSwitch->setStateSelected(true);
        updateControlsBox();
        resetScrollbars();
    }

    void SettingsWindow::updateControlsBox()
    {
        while (mControlsBox->getChildCount())
            MyGUI::Gui::getInstance().destroyWidget(mControlsBox->getChildAt(0));

        MWBase::Environment::get().getWindowManager()->removeStaticMessageBox();
        const auto inputManager = MWBase::Environment::get().getInputManager();
        const auto& actions
            = mKeyboardMode ? inputManager->getActionKeySorting() : inputManager->getActionControllerSorting();

        for (const int& action : actions)
        {
            std::string desc{ inputManager->getActionDescription(action) };
            if (desc.empty())
                continue;

            std::string binding;
            if (mKeyboardMode)
                binding = inputManager->getActionKeyBindingName(action);
            else
                binding = inputManager->getActionControllerBindingName(action);

            Gui::SharedStateButton* leftText = mControlsBox->createWidget<Gui::SharedStateButton>(
                "SandTextButton", MyGUI::IntCoord(), MyGUI::Align::Default);
            leftText->setCaptionWithReplacing(desc);

            Gui::SharedStateButton* rightText = mControlsBox->createWidget<Gui::SharedStateButton>(
                "SandTextButton", MyGUI::IntCoord(), MyGUI::Align::Default);
            rightText->setCaptionWithReplacing(binding);
            rightText->setTextAlign(MyGUI::Align::Right);
            rightText->setUserData(action); // save the action id for callbacks
            rightText->eventMouseButtonClick += MyGUI::newDelegate(this, &SettingsWindow::onRebindAction);
            rightText->eventMouseWheel += MyGUI::newDelegate(this, &SettingsWindow::onInputTabMouseWheel);

            Gui::ButtonGroup group;
            group.push_back(leftText);
            group.push_back(rightText);
            Gui::SharedStateButton::createButtonGroup(group);
        }

        layoutControlsBox();
    }

    void SettingsWindow::updateLightSettings()
    {
        mClusteredLightingButton->setEnabled(
            MWBase::Environment::get().getResourceSystem()->getSceneManager()->isClusteredLightingSupported());

        const bool isClustered = Settings::shaders().mClusteredLighting;
        const bool isClassic = !isClustered && Settings::shaders().mClassicFalloff;

        mClassicFalloffWidget->setVisible(!isClustered);
        mMinimumBrightnessText->setVisible(!isClassic);
        mMinimumBrightnessScroll->setVisible(!isClassic);
    }

    void SettingsWindow::updateWindowModeSettings()
    {
        const Settings::WindowMode windowMode = Settings::video().mWindowMode;
        const std::size_t windowModeIndex = static_cast<std::size_t>(windowMode);

        mWindowModeList->setIndexSelected(windowModeIndex);

        mWindowBorderButton->setEnabled(windowMode == Settings::WindowMode::Windowed);

        // Windowed fullscreen uses the desktop size, which need not match an exclusive fullscreen mode.
        if (windowMode == Settings::WindowMode::Fullscreen)
        {
            // check if this resolution is supported in fullscreen
            if (mResolutionList->getIndexSelected() != MyGUI::ITEM_NONE)
            {
                auto resolution
                    = mResolutionList->getItemDataAt<std::pair<int, int>>(mResolutionList->getIndexSelected());
                if (resolution)
                {
                    Settings::video().mResolutionX.set(resolution->first);
                    Settings::video().mResolutionY.set(resolution->second);
                }
            }

            bool supported = false;
            int fallbackX = 0, fallbackY = 0;
            for (size_t i = 0; i < mResolutionList->getItemCount(); ++i)
            {
                auto resolution = mResolutionList->getItemDataAt<std::pair<int, int>>(i);
                if (!resolution)
                    continue;

                if (i == 0)
                {
                    fallbackX = resolution->first;
                    fallbackY = resolution->second;
                }

                if (resolution->first == Settings::video().mResolutionX
                    && resolution->second == Settings::video().mResolutionY)
                    supported = true;
            }

            if (!supported && mResolutionList->getItemCount())
            {
                if (fallbackX != 0 && fallbackY != 0)
                {
                    Settings::video().mResolutionX.set(fallbackX);
                    Settings::video().mResolutionY.set(fallbackY);
                }
            }
        }

        if (windowMode == Settings::WindowMode::WindowedFullscreen)
            mResolutionList->setEnabled(false);
    }

    void SettingsWindow::updateVSyncModeSettings()
    {
        mVSyncModeList->setIndexSelected(static_cast<size_t>(Settings::video().mVsyncMode));
    }

    void SettingsWindow::layoutControlsBox()
    {
        const int h = Settings::gui().mFontSize + 2;
        const int w = mControlsBox->getWidth() - 28;
        const int noWidgetsInRow = 2;
        const int totalH = static_cast<int>(mControlsBox->getChildCount() / noWidgetsInRow) * h;

        for (size_t i = 0; i < mControlsBox->getChildCount(); i++)
        {
            MyGUI::Widget* widget = mControlsBox->getChildAt(i);
            widget->setCoord(0, static_cast<int>(i / noWidgetsInRow * h), w, h);
        }

        // Canvas size must be expressed with VScroll disabled, otherwise MyGUI would expand the scroll area when the
        // scrollbar is hidden
        mControlsBox->setVisibleVScroll(false);
        mControlsBox->setCanvasSize(mControlsBox->getWidth(), std::max(totalH, mControlsBox->getHeight()));
        mControlsBox->setVisibleVScroll(true);
    }

    void SettingsWindow::renderScriptSettings()
    {
        mScriptAdapter->detach();

        mScriptList->removeAllItems();
        mScriptView->setCanvasSize({ 0, 0 });

        struct WeightedPage
        {
            size_t mIndex;
            std::string mName;
            size_t mNameWeight;
            size_t mHintWeight;

            constexpr bool operator<(const WeightedPage& rhs) const
            {
                if (mNameWeight != rhs.mNameWeight)
                    return mNameWeight > rhs.mNameWeight;
                if (mHintWeight != rhs.mHintWeight)
                    return mHintWeight > rhs.mHintWeight;
                return mName < rhs.mName;
            }
        };

        const std::vector<std::string> patternArray = generatePatternArray(mScriptFilter->getCaption());
        std::vector<WeightedPage> weightedPages;
        weightedPages.reserve(LuaUi::scriptSettingsPageCount());
        for (size_t i = 0; i < LuaUi::scriptSettingsPageCount(); ++i)
        {
            LuaUi::ScriptSettingsPage page = LuaUi::scriptSettingsPageAt(i);
            size_t nameWeight = weightedSearch(page.mName, patternArray);
            size_t hintWeight = weightedSearch(page.mSearchHints, patternArray);
            if ((nameWeight + hintWeight) > 0)
                weightedPages.push_back({ i, page.mName, nameWeight, hintWeight });
        }
        std::sort(weightedPages.begin(), weightedPages.end());
        for (const WeightedPage& weightedPage : weightedPages)
            mScriptList->addItem(weightedPage.mName, weightedPage.mIndex);

        // Hide script settings when the game world isn't loaded
        bool disabled = LuaUi::scriptSettingsPageCount() == 0;
        mScriptFilter->setVisible(!disabled);
        mScriptList->setVisible(!disabled);
        mScriptBox->setVisible(!disabled);
        mScriptDisabled->setVisible(disabled);

        LuaUi::attachPageAt(mCurrentPage, mScriptAdapter);
    }

    void SettingsWindow::onScriptFilterChange(MyGUI::EditBox*)
    {
        renderScriptSettings();
    }

    void SettingsWindow::onScriptListSelection(MyGUI::ListBox*, size_t index)
    {
        mScriptAdapter->detach();
        mCurrentPage = static_cast<size_t>(-1);
        if (index < mScriptList->getItemCount())
        {
            mCurrentPage = *mScriptList->getItemDataAt<size_t>(index);
            LuaUi::attachPageAt(mCurrentPage, mScriptAdapter);
        }
    }

    void SettingsWindow::onRebindAction(MyGUI::Widget* sender)
    {
        int actionId = *sender->getUserData<int>();

        sender->castType<MyGUI::Button>()->setCaptionWithReplacing("#{Interface:None}");

        MWBase::Environment::get().getWindowManager()->staticMessageBox("#{OMWEngine:RebindAction}");
        MWBase::Environment::get().getWindowManager()->disallowMouse();

        MWBase::Environment::get().getInputManager()->enableDetectingBindingMode(actionId, mKeyboardMode);
    }

    void SettingsWindow::onInputTabMouseWheel(MyGUI::Widget* /*sender*/, int rel)
    {
        if (mControlsBox->getViewOffset().top + rel * 0.3f > 0)
            mControlsBox->setViewOffset(MyGUI::IntPoint(0, 0));
        else
            mControlsBox->setViewOffset(
                MyGUI::IntPoint(0, static_cast<int>(mControlsBox->getViewOffset().top + rel * 0.3f)));
    }

    void SettingsWindow::onResetDefaultBindings(MyGUI::Widget* /*sender*/)
    {
        ConfirmationDialog* dialog = MWBase::Environment::get().getWindowManager()->getConfirmationDialog();
        dialog->askForConfirmation("#{OMWEngine:ConfirmResetBindings}");
        dialog->eventOkClicked.clear();
        dialog->eventOkClicked += MyGUI::newDelegate(this, &SettingsWindow::onResetDefaultBindingsAccept);
        dialog->eventCancelClicked.clear();
    }

    void SettingsWindow::onResetDefaultBindingsAccept()
    {
        if (mKeyboardMode)
            MWBase::Environment::get().getInputManager()->resetToDefaultKeyBindings();
        else
            MWBase::Environment::get().getInputManager()->resetToDefaultControllerBindings();
        updateControlsBox();
    }

    void SettingsWindow::onOpen()
    {
        highlightCurrentResolution();
        updateControlsBox();
        updateLightSettings();
        updateWindowModeSettings();
        updateVSyncModeSettings();
        resetScrollbars();
        renderScriptSettings();
        MWBase::Environment::get().getWindowManager()->setKeyFocusWidget(mOkButton);

        if (Settings::gui().mControllerMenus && mPanel == nullptr)
            mPanel = ControllerUi::createPanelRoot("ControllerModal");
        showPanelOrWindow();
    }

    void SettingsWindow::onClose()
    {
        if (mPanel != nullptr)
            mPanel->setVisible(false);
        // Save user settings
        Settings::Manager::saveUser(mCfgMgr.getUserConfigPath() / "settings.cfg");
        MWBase::Environment::get().getLuaManager()->savePermanentStorage(mCfgMgr.getUserConfigPath());
        MWBase::Environment::get().getInputManager()->saveBindings();
    }

    void SettingsWindow::onWindowResize(MyGUI::Window* /*sender*/)
    {
        layoutControlsBox();
    }

    void SettingsWindow::computeMinimumWindowSize()
    {
        auto* window = mMainWidget->castType<MyGUI::Window>();
        auto minSize = window->getMinSize();

        // Window should be at minimum wide enough to show all tabs.
        int tabBarWidth = 0;
        for (uint32_t i = 0; i < mSettingsTab->getItemCount(); i++)
        {
            tabBarWidth += mSettingsTab->getButtonWidthAt(i);
        }

        // Need to include window margins
        int margins = mMainWidget->getWidth() - mSettingsTab->getWidth();
        int minimumWindowWidth = tabBarWidth + margins;

        if (minimumWindowWidth > minSize.width)
        {
            minSize.width = minimumWindowWidth;
            window->setMinSize(minSize);

            // Make a dummy call to setSize so MyGUI can apply any resize resulting from the change in MinSize
            mMainWidget->setSize(mMainWidget->getSize());
        }
    }

    void SettingsWindow::resetScrollbars()
    {
        mResolutionList->setScrollPosition(0);
        mControlsBox->setViewOffset(MyGUI::IntPoint(0, 0));
    }

    bool SettingsWindow::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
    {
        if (arg.button == SDL_CONTROLLER_BUTTON_B)
        {
            onOkButtonClicked(mOkButton);
            return true;
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER)
        {
            size_t index = mSettingsTab->getIndexSelected();
            index = wrap(index, mSettingsTab->getItemCount(), -1);
            mSettingsTab->setIndexSelected(index);
            MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
            showPanelOrWindow();
            return true;
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)
        {
            size_t index = mSettingsTab->getIndexSelected();
            index = wrap(index, mSettingsTab->getItemCount(), 1);
            mSettingsTab->setIndexSelected(index);
            MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
            showPanelOrWindow();
            return true;
        }
        else if (mPanel != nullptr && mPanel->getVisible())
        {
            if (arg.button == SDL_CONTROLLER_BUTTON_A)
                activatePanelRow();
            // The d-pad is read every frame in onFrame, so it can repeat while held.
            return true;
        }

        return false;
    }

}
