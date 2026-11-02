#ifndef OPENMW_MWAUTOMATION_INPUT_H
#define OPENMW_MWAUTOMATION_INPUT_H

#include <cstdint>
#include <string_view>

#include <SDL_keyboard.h>

struct SDL_Window;

namespace MWAutomation
{
    // Adds synthetic keyboard and mouse events to SDL's event queue. The game reads them in the same loop as real
    // input, so they work without the window having focus. Code that reads key or mouse state directly from SDL,
    // such as SDL_GetKeyboardState, doesn't see them.
    class InputInjector
    {
    public:
        explicit InputInjector(SDL_Window* window);

        // Throws std::runtime_error for unknown names. Names are SDL's, such as "Escape", "Return", "A" or "F1".
        static SDL_Scancode parseKey(std::string_view name);
        // Throws std::runtime_error for unknown names: "left", "middle", "right", "x1" or "x2".
        static std::uint8_t parseMouseButton(std::string_view name);

        void key(SDL_Scancode scancode, bool pressed, SDL_Keymod modifiers);
        void text(std::string_view utf8);
        void mouseMove(int x, int y);
        void mouseButton(std::uint8_t button, bool pressed);
        void mouseWheel(float x, float y);

    private:
        SDL_Window* mWindow;
        int mMouseX = 0;
        int mMouseY = 0;
        std::uint32_t mMouseButtons = 0;
    };
}

#endif
