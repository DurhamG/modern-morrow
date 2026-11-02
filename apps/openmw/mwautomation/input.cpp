#include "input.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>

#include <SDL_events.h>
#include <SDL_mouse.h>
#include <SDL_timer.h>
#include <SDL_video.h>

#include <components/misc/strings/lower.hpp>

namespace MWAutomation
{
    namespace
    {
        void push(SDL_Event& event)
        {
            if (SDL_PushEvent(&event) < 0)
                throw std::runtime_error(std::string("SDL_PushEvent failed: ") + SDL_GetError());
        }
    }

    InputInjector::InputInjector(SDL_Window* window)
        : mWindow(window)
    {
    }

    SDL_Scancode InputInjector::parseKey(std::string_view name)
    {
        const std::string text(name);
        const SDL_Scancode scancode = SDL_GetScancodeFromName(text.c_str());
        if (scancode == SDL_SCANCODE_UNKNOWN)
            throw std::runtime_error("unknown key '" + text + "'. Use SDL key names such as Escape, Return or A.");
        return scancode;
    }

    std::uint8_t InputInjector::parseMouseButton(std::string_view name)
    {
        const std::string lower = Misc::StringUtils::lowerCase(name);
        if (lower.empty() || lower == "left")
            return SDL_BUTTON_LEFT;
        if (lower == "middle")
            return SDL_BUTTON_MIDDLE;
        if (lower == "right")
            return SDL_BUTTON_RIGHT;
        if (lower == "x1")
            return SDL_BUTTON_X1;
        if (lower == "x2")
            return SDL_BUTTON_X2;
        throw std::runtime_error("unknown mouse button '" + std::string(name) + "'");
    }

    void InputInjector::key(SDL_Scancode scancode, bool pressed, SDL_Keymod modifiers)
    {
        SDL_Event event{};
        event.key.type = pressed ? SDL_KEYDOWN : SDL_KEYUP;
        event.key.timestamp = SDL_GetTicks();
        event.key.windowID = SDL_GetWindowID(mWindow);
        event.key.state = pressed ? SDL_PRESSED : SDL_RELEASED;
        event.key.repeat = 0;
        event.key.keysym.scancode = scancode;
        event.key.keysym.sym = SDL_GetKeyFromScancode(scancode);
        event.key.keysym.mod = static_cast<Uint16>(modifiers);
        push(event);
    }

    void InputInjector::text(std::string_view utf8)
    {
        // A text event holds up to 31 bytes. Split on character boundaries.
        constexpr std::size_t maxBytes = sizeof(SDL_TextInputEvent::text) - 1;
        std::size_t start = 0;
        while (start < utf8.size())
        {
            std::size_t end = std::min(utf8.size(), start + maxBytes);
            while (end < utf8.size() && end > start && (static_cast<unsigned char>(utf8[end]) & 0xC0) == 0x80)
                --end;
            SDL_Event event{};
            event.text.type = SDL_TEXTINPUT;
            event.text.timestamp = SDL_GetTicks();
            event.text.windowID = SDL_GetWindowID(mWindow);
            std::memcpy(event.text.text, utf8.data() + start, end - start);
            push(event);
            start = end;
        }
    }

    void InputInjector::mouseMove(int x, int y)
    {
        SDL_Event event{};
        event.motion.type = SDL_MOUSEMOTION;
        event.motion.timestamp = SDL_GetTicks();
        event.motion.windowID = SDL_GetWindowID(mWindow);
        event.motion.which = 0;
        event.motion.state = mMouseButtons;
        event.motion.x = x;
        event.motion.y = y;
        event.motion.xrel = x - mMouseX;
        event.motion.yrel = y - mMouseY;
        mMouseX = x;
        mMouseY = y;
        push(event);
    }

    void InputInjector::mouseButton(std::uint8_t button, bool pressed)
    {
        if (pressed)
            mMouseButtons |= SDL_BUTTON(button);
        else
            mMouseButtons &= ~SDL_BUTTON(button);

        SDL_Event event{};
        event.button.type = pressed ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
        event.button.timestamp = SDL_GetTicks();
        event.button.windowID = SDL_GetWindowID(mWindow);
        event.button.which = 0;
        event.button.button = button;
        event.button.state = pressed ? SDL_PRESSED : SDL_RELEASED;
        event.button.clicks = 1;
        event.button.x = mMouseX;
        event.button.y = mMouseY;
        push(event);
    }

    void InputInjector::mouseWheel(float x, float y)
    {
        SDL_Event event{};
        event.wheel.type = SDL_MOUSEWHEEL;
        event.wheel.timestamp = SDL_GetTicks();
        event.wheel.windowID = SDL_GetWindowID(mWindow);
        event.wheel.which = 0;
        event.wheel.x = static_cast<Sint32>(x);
        event.wheel.y = static_cast<Sint32>(y);
        event.wheel.preciseX = x;
        event.wheel.preciseY = y;
        event.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
        event.wheel.mouseX = mMouseX;
        event.wheel.mouseY = mMouseY;
        push(event);
    }
}
