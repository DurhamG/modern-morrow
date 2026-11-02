#include "automation.hpp"

#include <chrono>
#include <stdexcept>

#include <SDL_video.h>

#include <components/debug/debuglog.hpp>
#include <components/files/conversion.hpp>
#include <components/misc/strings/lower.hpp>
#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/statemanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwgui/mode.hpp"

#include "gamepad.hpp"
#include "input.hpp"
#include "screenshot.hpp"

namespace MWAutomation
{
    namespace
    {
        // The same names Lua uses for UI modes (see mwlua/uibindings.cpp).
        std::string_view getModeName(MWGui::GuiMode mode)
        {
            switch (mode)
            {
                case MWGui::GM_None:
                    return "None";
                case MWGui::GM_Inventory:
                    return "Interface";
                case MWGui::GM_Container:
                    return "Container";
                case MWGui::GM_Companion:
                    return "Companion";
                case MWGui::GM_MainMenu:
                    return "MainMenu";
                case MWGui::GM_Journal:
                    return "Journal";
                case MWGui::GM_Scroll:
                    return "Scroll";
                case MWGui::GM_Book:
                    return "Book";
                case MWGui::GM_Alchemy:
                    return "Alchemy";
                case MWGui::GM_Repair:
                    return "Repair";
                case MWGui::GM_Dialogue:
                    return "Dialogue";
                case MWGui::GM_Barter:
                    return "Barter";
                case MWGui::GM_Rest:
                    return "Rest";
                case MWGui::GM_SpellBuying:
                    return "SpellBuying";
                case MWGui::GM_Travel:
                    return "Travel";
                case MWGui::GM_SpellCreation:
                    return "SpellCreation";
                case MWGui::GM_Enchanting:
                    return "Enchanting";
                case MWGui::GM_Recharge:
                    return "Recharge";
                case MWGui::GM_Training:
                    return "Training";
                case MWGui::GM_MerchantRepair:
                    return "MerchantRepair";
                case MWGui::GM_Levelup:
                    return "LevelUp";
                case MWGui::GM_Name:
                    return "ChargenName";
                case MWGui::GM_Race:
                    return "ChargenRace";
                case MWGui::GM_Birth:
                    return "ChargenBirth";
                case MWGui::GM_Class:
                    return "ChargenClass";
                case MWGui::GM_ClassGenerate:
                    return "ChargenClassGenerate";
                case MWGui::GM_ClassPick:
                    return "ChargenClassPick";
                case MWGui::GM_ClassCreate:
                    return "ChargenClassCreate";
                case MWGui::GM_Review:
                    return "ChargenClassReview";
                case MWGui::GM_Loading:
                    return "Loading";
                case MWGui::GM_LoadingWallpaper:
                    return "LoadingWallpaper";
                case MWGui::GM_Jail:
                    return "Jail";
                case MWGui::GM_QuickKeysMenu:
                    return "QuickKeysMenu";
            }
            return "Unknown";
        }

        std::string_view getStateName(MWBase::StateManager::State state)
        {
            switch (state)
            {
                case MWBase::StateManager::State_NoGame:
                    return "menu";
                case MWBase::StateManager::State_Ended:
                    return "ended";
                case MWBase::StateManager::State_Running:
                    return "running";
            }
            return "unknown";
        }
    }

    Automation::Automation(std::uint16_t port, SDL_Window* window, osgViewer::Viewer& viewer,
        osgViewer::ScreenCaptureHandler& screenCaptureHandler,
        osgViewer::ScreenCaptureHandler::CaptureOperation& screenCaptureOperation)
        : mServer(port)
        , mWindow(window)
        , mScreenshotter(std::make_unique<Screenshotter>(viewer, screenCaptureHandler, screenCaptureOperation))
        , mInput(std::make_unique<InputInjector>(window))
        , mGamepad(std::make_unique<VirtualGamepad>())
    {
        registerHandlers();
        registerGamepadHandler();
    }

    Automation::~Automation() = default;

    void Automation::update()
    {
        ++mFrame;

        std::vector<Request> requests;
        mServer.poll(requests);
        for (const Request& request : requests)
            start(request);

        // A command may start another command's wait, so step a snapshot and keep the unfinished ones.
        std::vector<Pending> pending = std::move(mPending);
        mPending.clear();
        for (Pending& command : pending)
            if (!step(command))
                mPending.push_back(std::move(command));
    }

    void Automation::start(const Request& request)
    {
        Json message;
        try
        {
            message = Json::parse(request.mLine);
        }
        catch (const std::exception& e)
        {
            replyError(request.mClient, Json(), e.what());
            return;
        }

        const Json* idField = message.find("id");
        const Json id = idField != nullptr ? *idField : Json();
        Pending pending{ request.mClient, id, nullptr };
        try
        {
            if (!message.isObject())
                throw std::runtime_error("a request must be a JSON object");
            const std::string command = message.getString("cmd");
            const auto handler = mHandlers.find(command);
            if (handler == mHandlers.end())
                throw std::runtime_error("unknown command '" + command + "'");
            pending.mPoll = handler->second(message);
        }
        catch (const std::exception& e)
        {
            replyError(request.mClient, id, e.what());
            return;
        }
        if (!step(pending))
            mPending.push_back(std::move(pending));
    }

    bool Automation::step(Pending& pending)
    {
        try
        {
            std::optional<Json> result = pending.mPoll();
            if (!result)
                return false;
            reply(pending.mClient, pending.mId, std::move(*result));
        }
        catch (const std::exception& e)
        {
            replyError(pending.mClient, pending.mId, e.what());
        }
        return true;
    }

    void Automation::reply(ClientId client, const Json& id, Json result)
    {
        Json message = Json::object({ { "id", id }, { "ok", true }, { "result", std::move(result) } });
        mServer.send(client, message.dump());
    }

    void Automation::replyError(ClientId client, const Json& id, std::string_view error)
    {
        Json message = Json::object({ { "id", id }, { "ok", false }, { "error", error } });
        mServer.send(client, message.dump());
    }

    Json Automation::describeState() const
    {
        const MWBase::Environment& environment = MWBase::Environment::get();
        const MWBase::StateManager::State state = environment.getStateManager()->getState();

        Json modes = Json::array();
        for (const MWGui::GuiMode mode : environment.getWindowManager()->getGuiModeStack())
            modes.push(getModeName(mode));

        int windowWidth = 0;
        int windowHeight = 0;
        SDL_GetWindowSize(mWindow, &windowWidth, &windowHeight);

        Json result = Json::object({
            { "state", getStateName(state) },
            { "frame", mFrame },
            { "modes", std::move(modes) },
            { "window", Json::object({ { "width", windowWidth }, { "height", windowHeight } }) },
            { "scale", Settings::gui().mScalingFactor.get() },
        });
        if (state == MWBase::StateManager::State_Running)
            result.set("cell", environment.getWorld()->getCellName());
        return result;
    }

    void Automation::registerHandlers()
    {
        mHandlers["ping"]
            = [this](const Json&) -> Poll { return [this] { return Json::object({ { "frame", mFrame } }); }; };

        // Waits for a number of frames ("frames"), or of real seconds ("seconds"). With neither, waits one frame.
        mHandlers["wait"] = [this](const Json& request) -> Poll {
            const double seconds = request.getNumber("seconds", 0);
            if (seconds > 0)
            {
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);
                return [this, deadline]() -> std::optional<Json> {
                    if (std::chrono::steady_clock::now() < deadline)
                        return std::nullopt;
                    return Json::object({ { "frame", mFrame } });
                };
            }
            const std::uint64_t target = mFrame + static_cast<std::uint64_t>(request.getNumber("frames", 1));
            return [this, target]() -> std::optional<Json> {
                if (mFrame < target)
                    return std::nullopt;
                return Json::object({ { "frame", mFrame } });
            };
        };

        mHandlers["state"] = [this](const Json&) -> Poll { return [this] { return describeState(); }; };

        // Captures the next frame, including the UI, to "path". The format comes from the extension. Optional
        // "crop": [x, y, width, height] in window pixels from the top left, and "scale" (0 to 1) to shrink the image.
        mHandlers["screenshot"] = [this](const Json& request) -> Poll {
            ScreenshotRequest screenshot;
            const std::string path = request.getString("path");
            if (path.empty())
                throw std::runtime_error("screenshot needs a \"path\"");
            screenshot.mPath = Files::pathFromUnicodeString(path);
            screenshot.mScale = static_cast<float>(request.getNumber("scale", 1));
            if (const Json* cropField = request.find("crop"); cropField != nullptr && !cropField->isNull())
            {
                if (!cropField->isArray() || cropField->asArray().size() != 4)
                    throw std::runtime_error("\"crop\" must be [x, y, width, height]");
                const Json::Array& values = cropField->asArray();
                screenshot.mCrop
                    = CropRect{ static_cast<int>(values[0].asNumber()), static_cast<int>(values[1].asNumber()),
                          static_cast<int>(values[2].asNumber()), static_cast<int>(values[3].asNumber()) };
            }
            mScreenshotter->start(std::move(screenshot));

            const std::uint64_t deadline = mFrame + 300;
            return [this, deadline]() -> std::optional<Json> {
                std::optional<ScreenshotResult> result = mScreenshotter->poll();
                if (!result)
                {
                    if (mFrame < deadline)
                        return std::nullopt;
                    mScreenshotter->cancel();
                    throw std::runtime_error("no frame was rendered for the screenshot");
                }
                return Json::object({ { "path", Files::pathToUnicodeString(result->mPath) },
                    { "width", result->mWidth }, { "height", result->mHeight }, { "frame", mFrame } });
            };
        };

        mHandlers["quit"] = [](const Json&) -> Poll {
            return [] {
                Log(Debug::Info) << "Quit requested by the automation server";
                MWBase::Environment::get().getStateManager()->requestQuit();
                return Json::object();
            };
        };

        // Keyboard input. With "text", types that text. Otherwise "key" names an SDL key, such as Escape, Return or
        // A, and "action" is "tap" (the default), "down" or "up". A tap holds the key for "hold" frames (default 2).
        // "mods" lists held modifiers: shift, ctrl, alt.
        mHandlers["key"] = [this](const Json& request) -> Poll {
            if (const Json* text = request.find("text"); text != nullptr && !text->isNull())
            {
                if (!text->isString())
                    throw std::runtime_error("\"text\" must be a string");
                mInput->text(text->asString());
                return sequence({});
            }

            const SDL_Scancode scancode = InputInjector::parseKey(request.getString("key"));
            const SDL_Keymod modifiers = parseModifiers(request);
            const std::string action = request.getString("action", "tap");
            if (action == "down")
                return sequence({ { 0, [=, this] { mInput->key(scancode, true, modifiers); } } });
            if (action == "up")
                return sequence({ { 0, [=, this] { mInput->key(scancode, false, modifiers); } } });
            if (action != "tap")
                throw std::runtime_error("\"action\" must be tap, down or up");
            const int hold = std::max(1, static_cast<int>(request.getNumber("hold", 2)));
            return sequence({
                { 0, [=, this] { mInput->key(scancode, true, modifiers); } },
                { hold, [=, this] { mInput->key(scancode, false, modifiers); } },
            });
        };

        // Mouse input, in window pixels from the top left. "action" is "move", "down", "up", "click" (the default)
        // or "wheel". "x" and "y" give the position; down, up and click move there first when they are given.
        // "button" is left (the default), middle, right, x1 or x2. A click holds the button for "hold" frames
        // (default 2). A wheel action scrolls by "y" notches, and by "x" notches sideways.
        mHandlers["mouse"] = [this](const Json& request) -> Poll {
            const std::string action = request.getString("action", "click");
            if (action == "wheel")
            {
                const float x = static_cast<float>(request.getNumber("x", 0));
                const float y = static_cast<float>(request.getNumber("y", 0));
                return sequence({ { 0, [=, this] { mInput->mouseWheel(x, y); } } });
            }

            std::vector<std::pair<int, std::function<void()>>> steps;
            const bool hasPosition = request.find("x") != nullptr || request.find("y") != nullptr;
            if (hasPosition)
            {
                const int x = static_cast<int>(request.getNumber("x"));
                const int y = static_cast<int>(request.getNumber("y"));
                steps.emplace_back(0, [=, this] { mInput->mouseMove(x, y); });
            }
            else if (action == "move")
                throw std::runtime_error("move needs \"x\" and \"y\"");

            const std::uint8_t button = InputInjector::parseMouseButton(request.getString("button", "left"));
            // Leave a frame after moving, so the UI updates which widget is under the cursor before the press.
            const int pressFrame = hasPosition ? 1 : 0;
            if (action == "down")
                steps.emplace_back(pressFrame, [=, this] { mInput->mouseButton(button, true); });
            else if (action == "up")
                steps.emplace_back(pressFrame, [=, this] { mInput->mouseButton(button, false); });
            else if (action == "click")
            {
                const int hold = std::max(1, static_cast<int>(request.getNumber("hold", 2)));
                steps.emplace_back(pressFrame, [=, this] { mInput->mouseButton(button, true); });
                steps.emplace_back(pressFrame + hold, [=, this] { mInput->mouseButton(button, false); });
            }
            else if (action != "move")
                throw std::runtime_error("\"action\" must be move, down, up, click or wheel");
            return sequence(std::move(steps));
        };
    }

    void Automation::registerGamepadHandler()
    {
        // Virtual gamepad input. "action" is "connect", "disconnect", "tap", "down", "up" or "axis". A button action
        // needs "button": a, b, x, y, back, guide, start, leftstick, rightstick, leftshoulder, rightshoulder, dpup,
        // dpdown, dpleft or dpright. A tap holds the button for "hold" frames (default 2), and is the default action
        // when a button is given. An axis action sets "axis" (leftx, lefty, rightx, righty, lefttrigger or
        // righttrigger) to "value": -1 to 1 for sticks, with positive y down, and 0 to 1 for triggers. With "hold",
        // the axis goes back to rest after that many frames. The gamepad connects on first use. The game needs a few
        // frames to open it, so the first command waits before it presses anything.
        mHandlers["pad"] = [this](const Json& request) -> Poll {
            const bool hasButton = request.find("button") != nullptr;
            const std::string action
                = request.getString("action", hasButton ? "tap" : (request.find("axis") ? "axis" : "connect"));

            std::vector<std::pair<int, std::function<void()>>> steps;
            int start = 0;
            if (action != "disconnect" && !mGamepad->isConnected())
            {
                mGamepad->connect();
                start = 3;
            }

            if (action == "connect" || action == "disconnect")
            {
                if (action == "disconnect")
                    mGamepad->disconnect();
                steps.emplace_back(start, [] {});
            }
            else if (action == "axis")
            {
                const SDL_GameControllerAxis axis = VirtualGamepad::parseAxis(request.getString("axis"));
                const float value = static_cast<float>(request.getNumber("value", 0));
                steps.emplace_back(start, [=, this] { mGamepad->setAxis(axis, value); });
                if (request.find("hold") != nullptr)
                {
                    const int hold = std::max(1, static_cast<int>(request.getNumber("hold")));
                    steps.emplace_back(start + hold, [=, this] { mGamepad->setAxis(axis, 0); });
                }
            }
            else
            {
                const SDL_GameControllerButton button = VirtualGamepad::parseButton(request.getString("button"));
                if (action == "down")
                    steps.emplace_back(start, [=, this] { mGamepad->setButton(button, true); });
                else if (action == "up")
                    steps.emplace_back(start, [=, this] { mGamepad->setButton(button, false); });
                else if (action == "tap")
                {
                    const int hold = std::max(1, static_cast<int>(request.getNumber("hold", 2)));
                    steps.emplace_back(start, [=, this] { mGamepad->setButton(button, true); });
                    steps.emplace_back(start + hold, [=, this] { mGamepad->setButton(button, false); });
                }
                else
                    throw std::runtime_error("\"action\" must be connect, disconnect, tap, down, up or axis");
            }
            return sequence(std::move(steps));
        };
    }

    SDL_Keymod Automation::parseModifiers(const Json& request)
    {
        const Json* field = request.find("mods");
        if (field == nullptr || field->isNull())
            return KMOD_NONE;
        std::vector<std::string> names;
        if (field->isString())
            names.push_back(field->asString());
        else if (field->isArray())
        {
            for (const Json& item : field->asArray())
            {
                if (!item.isString())
                    throw std::runtime_error("\"mods\" must be a list of names");
                names.push_back(item.asString());
            }
        }
        else
            throw std::runtime_error("\"mods\" must be a name or a list of names");

        int modifiers = KMOD_NONE;
        for (const std::string& name : names)
        {
            const std::string lower = Misc::StringUtils::lowerCase(name);
            if (lower == "shift")
                modifiers |= KMOD_SHIFT;
            else if (lower == "ctrl")
                modifiers |= KMOD_CTRL;
            else if (lower == "alt")
                modifiers |= KMOD_ALT;
            else
                throw std::runtime_error("unknown modifier '" + name + "'");
        }
        return static_cast<SDL_Keymod>(modifiers);
    }

    Automation::Poll Automation::sequence(std::vector<std::pair<int, std::function<void()>>> steps)
    {
        const std::uint64_t start = mFrame;
        std::size_t next = 0;
        return [this, start, next, steps = std::move(steps)]() mutable -> std::optional<Json> {
            while (next < steps.size() && mFrame >= start + static_cast<std::uint64_t>(steps[next].first))
                steps[next++].second();
            if (next < steps.size())
                return std::nullopt;
            return Json::object({ { "frame", mFrame } });
        };
    }
}
