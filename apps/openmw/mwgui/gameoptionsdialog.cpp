#include "gameoptionsdialog.hpp"

#include <algorithm>
#include <array>
#include <string>

#include <MyGUI_ImageBox.h>
#include <MyGUI_LayerManager.h>

#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "runfatiguechoices.hpp"

namespace MWGui
{
    namespace
    {
        enum class Option
        {
            RetroactiveHealth,
            KeepLevelUpProgress,
            GlancingBlows,
            MinimumRunSpeed,
            RunFatigue,
            ExclusiveChoices,
            QuestMarkers,
            QuestCompass,
        };

        struct OptionText
        {
            Option mOption;
            const char* mName;
            const char* mDescription;
        };

        constexpr std::array<OptionText, 8> sOptions = { {
            { Option::RetroactiveHealth, "#{OMWEngine:RetroactiveHealth}",
                "#{Interface:GameOptionsRetroactiveHealthText}" },
            { Option::KeepLevelUpProgress, "#{OMWEngine:KeepLevelUpProgress}",
                "#{Interface:GameOptionsKeepLevelUpProgressText}" },
            { Option::GlancingBlows, "#{OMWEngine:GlancingBlows}", "#{Interface:GameOptionsGlancingBlowsText}" },
            { Option::MinimumRunSpeed, "#{OMWEngine:MinimumRunSpeed}", "#{Interface:GameOptionsMinimumRunSpeedText}" },
            { Option::RunFatigue, "#{OMWEngine:RunFatigue}", "#{Interface:GameOptionsRunFatigueText}" },
            { Option::ExclusiveChoices, "#{OMWEngine:WarnBeforeExclusiveChoices}",
                "#{Interface:GameOptionsExclusiveChoicesText}" },
            { Option::QuestMarkers, "#{OMWEngine:QuestMarkers}", "#{Interface:GameOptionsQuestMarkersText}" },
            { Option::QuestCompass, "#{OMWEngine:QuestCompass}", "#{Interface:GameOptionsQuestCompassText}" },
        } };

        Settings::SettingValue<bool>* boolSetting(Option option)
        {
            switch (option)
            {
                case Option::RetroactiveHealth:
                    return &Settings::game().mRetroactiveHealth;
                case Option::KeepLevelUpProgress:
                    return &Settings::game().mKeepLevelUpProgress;
                case Option::GlancingBlows:
                    return &Settings::game().mGlancingBlows;
                case Option::MinimumRunSpeed:
                    return &Settings::game().mMinimumRunSpeed;
                case Option::ExclusiveChoices:
                    return &Settings::game().mWarnBeforeExclusiveChoices;
                case Option::QuestMarkers:
                    return &Settings::gui().mQuestMarkers;
                case Option::QuestCompass:
                    return &Settings::gui().mQuestCompass;
                case Option::RunFatigue:
                    break;
            }
            return nullptr;
        }

        std::string valueText(Option option)
        {
            if (const Settings::SettingValue<bool>* setting = boolSetting(option))
                return setting->get() ? "#{Interface:On}" : "#{Interface:Off}";
            const float fatigue = sRunFatigueChoices[nearestRunFatigueChoice(Settings::game().mRunFatigue)];
            return std::to_string(static_cast<int>(fatigue * 100.f + 0.5f)) + "%";
        }

        void applyChangedSettings()
        {
            const Settings::CategorySettingVector changed = Settings::Manager::getPendingChanges();
            MWBase::Environment::get().getWorld()->processChangedSettings(changed);
            MWBase::Environment::get().getSoundManager()->processChangedSettings(changed);
            MWBase::Environment::get().getWindowManager()->processChangedSettings(changed);
            MWBase::Environment::get().getInputManager()->processChangedSettings(changed);
            MWBase::Environment::get().getMechanicsManager()->processChangedSettings(changed);
            Settings::Manager::resetPendingChanges();
        }
    }

    GameOptionsDialog::GameOptionsDialog()
        : WindowModal("openmw_game_options_dialog.layout")
    {
        // A changes the focused option, even right after the mouse moved, rather than clicking where the mouse is.
        mDisableGamepadCursor = true;
    }

    GameOptionsDialog::~GameOptionsDialog() = default;

    void GameOptionsDialog::onOpen()
    {
        WindowModal::onOpen();
        mList.reset();
        if (mPanel == nullptr)
            mPanel = ControllerUi::createPanelRoot("ControllerModal");
        if (mPanel == nullptr)
            return;
        ControllerUi::hideWindow(mMainWidget);
        mPanel->setVisible(true);
        MyGUI::LayerManager::getInstance().upLayerItem(mPanel);
        draw();
    }

    void GameOptionsDialog::onClose()
    {
        if (mPanel != nullptr)
            mPanel->setVisible(false);
        mList.resetHeld();
        WindowModal::onClose();
    }

    void GameOptionsDialog::onFrame(float duration)
    {
        if (mPanel == nullptr || !mPanel->getVisible())
            return;
        if (MWBase::Environment::get().getWindowManager()->getActiveControllerWindow() != this)
        {
            mList.resetHeld();
            return;
        }
        if (mList.updateHeld(duration, static_cast<int>(sOptions.size())))
            draw();
    }

    void GameOptionsDialog::change(int delta, bool wrap)
    {
        const int focus = mList.getFocus();
        if (focus < 0 || focus >= static_cast<int>(sOptions.size()))
            return;
        const Option option = sOptions[static_cast<std::size_t>(focus)].mOption;
        if (Settings::SettingValue<bool>* setting = boolSetting(option))
        {
            // A turns the option on or off. Right turns it on and left turns it off, as their order is Off, On.
            const bool value = wrap ? !setting->get() : delta > 0;
            if (value == setting->get())
                return;
            setting->set(value);
        }
        else
        {
            const float value = stepRunFatigueChoice(Settings::game().mRunFatigue, delta, wrap);
            if (value == Settings::game().mRunFatigue)
                return;
            Settings::game().mRunFatigue.set(value);
        }
        applyChangedSettings();
        MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
    }

    void GameOptionsDialog::draw()
    {
        using namespace ControllerUi;
        if (mPanel == nullptr || !mPanel->getVisible())
            return;
        clearPanel(mPanel);
        const MyGUI::IntSize size = mPanel->getSize();
        addRect(mPanel, 0, 0, size.width, size.height, MyGUI::Colour::Black)->setNeedMouseFocus(true);

        addText(mPanel, sMargin, 20, size.width - 2 * sMargin, 40, "#{Interface:GameOptionsTitle}", sFontTitle,
            sBrightColour);
        addRect(mPanel, sMargin, 66, size.width - 2 * sMargin, 1, sAccentColour, 0.2f);
        int top = 82;
        for (const std::string& line :
            wrapText(mPanel, localised("#{Interface:GameOptionsIntro}"), size.width - 2 * sMargin, sFontNormal, 3))
        {
            addText(mPanel, sMargin, top, size.width - 2 * sMargin, 26, escaped(line), sFontNormal, sTextColour);
            top += 26;
        }
        top += 16;
        const int bottom = size.height - 64;

        // The options on the left, each with its value at the right of its row.
        constexpr int entryH = 44;
        const int listW = std::min(620, (size.width - 2 * sMargin) * 11 / 20);
        // The box fits the rows, unless the screen is too short for them all, and then the list scrolls.
        const int listH = std::min(bottom - top, static_cast<int>(sOptions.size()) * entryH + 12);
        addRect(mPanel, sMargin, top, listW, listH, sPanelColour, 0.9f);
        const std::vector<ControllerList::Row> rows(sOptions.size(), ControllerList::Row{ entryH, true });
        const auto drawRow = [&](std::size_t i, int ry, bool focused) {
            if (focused)
            {
                addRect(mPanel, sMargin + 4, ry, listW - 8, entryH - 4, sAccentColour, 0.16f);
                addRect(mPanel, sMargin + 4, ry, 3, entryH - 4, sAccentColour);
            }
            const MyGUI::Colour colour = focused ? sActiveColour : sBrightColour;
            addText(mPanel, sMargin + 18, ry, listW - 130, entryH - 4, sOptions[i].mName, sFontNormal, colour);
            addText(mPanel, sMargin + listW - 110, ry, 92, entryH - 4, valueText(sOptions[i].mOption), sFontNormal,
                colour, MyGUI::Align::Right | MyGUI::Align::VCenter);
        };
        // Pointing at an option shows its description, and clicking it changes it, as A does.
        mList.draw(
            mPanel, sMargin + 4, top + 6, listW - 8, listH - 12, rows, drawRow, [this] { draw(); },
            [this] {
                change(1, true);
                draw();
            });

        // The focused option's name and description on the right.
        const OptionText& focused = sOptions[static_cast<std::size_t>(mList.getFocus())];
        const int dx = sMargin + listW + 28;
        const int dw = size.width - sMargin - dx;
        // The name lines up with the first row of the list.
        int ty = top + 10;
        for (const std::string& line : wrapText(mPanel, localised(focused.mName), dw, sFontLarge, 2))
        {
            addText(mPanel, dx, ty, dw, 32, escaped(line), sFontLarge, sBrightColour);
            ty += 32;
        }
        ty += 10;
        for (const std::string& line : wrapText(mPanel, localised(focused.mDescription), dw, sFontNormal, 16))
        {
            if (ty + 26 > bottom)
                break;
            addText(mPanel, dx, ty, dw, 26, escaped(line), sFontNormal, sTextColour);
            ty += 26;
        }

        // The button hints along the bottom. Clicking a hint presses its button.
        int hx = sMargin;
        const int hintY = size.height - 48;
        for (const auto& [button, label] : { std::pair(SDL_CONTROLLER_BUTTON_A, "#{Interface:OptionsChange}"),
                 std::pair(SDL_CONTROLLER_BUTTON_X, "#{Interface:GameOptionsBegin}") })
        {
            const SDL_GameControllerButton pressed = button;
            addGlyph(mPanel, hx, hintY + 4, 22, pressed);
            MyGUI::TextBox* text = addText(mPanel, hx + 30, hintY, 220, 30, label, sFontNormal, sBrightColour);
            addPointerTarget(
                mPanel, hx, hintY, 30 + text->getTextSize().width, 30, nullptr, [pressed] { pressButton(pressed); });
            hx += 30 + text->getTextSize().width + 22;
        }
    }

    bool GameOptionsDialog::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
    {
        switch (arg.button)
        {
            case SDL_CONTROLLER_BUTTON_A:
                change(1, true);
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                change(-1, false);
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                change(1, false);
                break;
            case SDL_CONTROLLER_BUTTON_X:
                eventBegin();
                return true;
            case SDL_CONTROLLER_BUTTON_B:
                setVisible(false);
                return true;
            default:
                break;
        }
        draw();
        return true;
    }
}
