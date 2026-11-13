#include "automation.hpp"

#include <chrono>
#include <fstream>
#include <stdexcept>

#include <MyGUI_Gui.h>
#include <MyGUI_ResourceLayout.h>
#include <MyGUI_XmlDocument.h>

#include <SDL_events.h>
#include <SDL_video.h>

#include <components/debug/debuglog.hpp>
#include <components/files/conversion.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/misc/strings/lower.hpp>
#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/luamanager.hpp"
#include "../mwbase/statemanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwgui/mode.hpp"

#include "../mwworld/ptr.hpp"

#include "gamepad.hpp"
#include "input.hpp"
#include "screenshot.hpp"
#include "uidump.hpp"

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
        registerScriptingHandlers();
        registerLayoutHandler();
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

        // Touch screen input, in window pixels from the top left. "action" is "tap" (the default), "down", "move",
        // "up", "drag" or "pinch". Down, move and up take "finger" (0 to 9, default 0), "x" and "y". A tap puts a
        // finger down and lifts it in the same frame. A drag puts a finger down on the first point of "path", a list
        // of [x, y], moves it to each other point "frames" frames apart (default 1), and lifts it at the end unless
        // "release" is false. A pinch puts two fingers down side by side, "from" pixels apart around "x" and "y", and
        // moves them to "to" pixels apart in "steps" moves (default 6), then lifts them.
        mHandlers["touch"] = [this](const Json& request) -> Poll {
            const std::string action = request.getString("action", "tap");
            const auto finger = static_cast<std::int64_t>(request.getNumber("finger", 0));
            const auto x = static_cast<float>(request.getNumber("x", 0));
            const auto y = static_cast<float>(request.getNumber("y", 0));
            std::vector<std::pair<int, std::function<void()>>> steps;
            if (action == "down" || action == "move" || action == "up")
            {
                const std::uint32_t type
                    = action == "down" ? SDL_FINGERDOWN : (action == "move" ? SDL_FINGERMOTION : SDL_FINGERUP);
                steps.emplace_back(0, [=, this] { mInput->finger(type, finger, x, y); });
            }
            else if (action == "tap")
            {
                steps.emplace_back(0, [=, this] {
                    mInput->finger(SDL_FINGERDOWN, finger, x, y);
                    mInput->finger(SDL_FINGERUP, finger, x, y);
                });
            }
            else if (action == "drag")
            {
                const Json* path = request.find("path");
                if (path == nullptr || !path->isArray() || path->asArray().empty())
                    throw std::runtime_error("drag needs a \"path\" of [x, y] points");
                const int frames = std::max(1, static_cast<int>(request.getNumber("frames", 1)));
                int frame = 0;
                bool first = true;
                float lastX = 0;
                float lastY = 0;
                for (const Json& point : path->asArray())
                {
                    if (!point.isArray() || point.asArray().size() != 2)
                        throw std::runtime_error("each point of \"path\" must be [x, y]");
                    lastX = static_cast<float>(point.asArray()[0].asNumber());
                    lastY = static_cast<float>(point.asArray()[1].asNumber());
                    const std::uint32_t type = first ? SDL_FINGERDOWN : SDL_FINGERMOTION;
                    steps.emplace_back(frame, [=, this] { mInput->finger(type, finger, lastX, lastY); });
                    frame += frames;
                    first = false;
                }
                if (request.getBool("release", true))
                    steps.emplace_back(frame, [=, this] { mInput->finger(SDL_FINGERUP, finger, lastX, lastY); });
            }
            else if (action == "pinch")
            {
                const auto from = static_cast<float>(request.getNumber("from", 100));
                const auto to = static_cast<float>(request.getNumber("to", 400));
                const int count = std::max(1, static_cast<int>(request.getNumber("steps", 6)));
                steps.emplace_back(0, [=, this] {
                    mInput->finger(SDL_FINGERDOWN, 0, x - from / 2, y);
                    mInput->finger(SDL_FINGERDOWN, 1, x + from / 2, y);
                });
                for (int i = 1; i <= count; ++i)
                {
                    const float half = (from + (to - from) * i / count) / 2;
                    steps.emplace_back(i, [=, this] {
                        mInput->finger(SDL_FINGERMOTION, 0, x - half, y);
                        mInput->finger(SDL_FINGERMOTION, 1, x + half, y);
                    });
                }
                steps.emplace_back(count + 1, [=, this] {
                    mInput->finger(SDL_FINGERUP, 0, x - to / 2, y);
                    mInput->finger(SDL_FINGERUP, 1, x + to / 2, y);
                });
            }
            else
                throw std::runtime_error("\"action\" must be tap, down, move, up, drag or pinch");
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

    void Automation::registerScriptingHandlers()
    {
        // Runs a console command, in the legacy script language, or a "lua ..." console command. Waits "frames"
        // frames (default 3) for output, and replies with the lines the console printed. Fails when the console
        // printed an error.
        mHandlers["console"] = [this](const Json& request) -> Poll {
            const std::string command = request.getString("command");
            if (command.empty())
                throw std::runtime_error("console needs a \"command\"");
            const std::uint64_t done = mFrame + static_cast<std::uint64_t>(request.getNumber("frames", 3));
            beginConsoleCapture();
            MWBase::Environment::get().getWindowManager()->executeConsoleCommand(command);
            // The console echoes the command first. Leave that out of the reply.
            if (!mConsoleOutput.empty() && mConsoleOutput.front().first.starts_with("> "))
                mConsoleOutput.erase(mConsoleOutput.begin());
            return [this, done]() -> std::optional<Json> {
                if (mFrame < done)
                    return std::nullopt;
                return endConsoleCapture();
            };
        };

        // Runs Lua code in the "player" (the default while a game is running), "global" or "menu" (the default at
        // the main menu) context, the same way the console's Lua mode does. A lone expression's value is printed.
        // Waits "frames" frames (default 3, or 6 for global) for output, and replies with the printed lines. Fails
        // when the code raised an error.
        mHandlers["lua"] = [this](const Json& request) -> Poll {
            const std::string code = request.getString("code");
            if (code.empty())
                throw std::runtime_error("lua needs \"code\"");
            const bool running
                = MWBase::Environment::get().getStateManager()->getState() == MWBase::StateManager::State_Running;
            const std::string context = request.getString("context", running ? "player" : "menu");
            std::string enter;
            std::string mode;
            if (context == "player")
            {
                enter = "luap";
                mode = "Lua[Player]";
            }
            else if (context == "global")
            {
                enter = "luag";
                mode = "Lua[Global]";
            }
            else if (context == "menu")
            {
                enter = "luam";
                mode = "Lua[Menu]";
            }
            else
                throw std::runtime_error("\"context\" must be player, global or menu");
            if (context != "menu" && !running)
                throw std::runtime_error("the " + context + " context needs a running game");
            const int frames = static_cast<int>(request.getNumber("frames", context == "global" ? 6 : 3));

            // Entering Lua mode prints a message and changes the console's mode in a later frame, so run the code
            // two frames after entering it.
            MWBase::LuaManager& lua = *MWBase::Environment::get().getLuaManager();
            lua.handleConsoleCommand("", enter, MWWorld::Ptr());
            const std::uint64_t runFrame = mFrame + 2;
            const std::uint64_t doneFrame = runFrame + frames;
            bool started = false;
            return [this, &lua, code, mode, runFrame, doneFrame, started]() mutable -> std::optional<Json> {
                if (mFrame < runFrame)
                    return std::nullopt;
                if (!started)
                {
                    beginConsoleCapture();
                    lua.handleConsoleCommand(mode, code, MWWorld::Ptr());
                    started = true;
                }
                if (mFrame < doneFrame)
                    return std::nullopt;
                Json result;
                try
                {
                    result = endConsoleCapture();
                }
                catch (...)
                {
                    lua.handleConsoleCommand(mode, "exit()", MWWorld::Ptr());
                    throw;
                }
                lua.handleConsoleCommand(mode, "exit()", MWWorld::Ptr());
                return result;
            };
        };

        // Describes the widget tree. See uidump.hpp for the format. "hidden": true includes hidden widgets, "root"
        // limits the dump to widgets with that name, and "depth" limits how deep it goes.
        mHandlers["ui"] = [this](const Json& request) -> Poll {
            UiDumpOptions options;
            options.mHidden = request.getBool("hidden");
            options.mRoot = request.getString("root");
            options.mMaxDepth = static_cast<int>(request.getNumber("depth", 0));
            return [this, options] {
                Json result = dumpUi(options);
                const Json state = describeState();
                if (const Json* modes = state.find("modes"))
                    result.set("modes", *modes);
                return result;
            };
        };
    }

    void Automation::registerLayoutHandler()
    {
        // Shows the widgets from a MyGUI layout file at "path", replacing the ones from any layout loaded before.
        // "close": true removes them instead. The file is read when the command runs, so a layout can be changed
        // and shown again without rebuilding the game. This is meant for UI mock-ups.
        mHandlers["layout"] = [this](const Json& request) -> Poll {
            const bool close = request.getBool("close");
            const std::string path = request.getString("path");
            if (!close && path.empty())
                throw std::runtime_error("layout needs a \"path\", or \"close\": true");
            return [this, close, path]() -> std::optional<Json> {
                closeLayout();
                if (close)
                    return Json::object();

                std::ifstream stream(Files::pathFromUnicodeString(path));
                if (!stream)
                    throw std::runtime_error("can't open " + path);
                MyGUI::xml::Document document;
                if (!document.open(stream))
                    throw std::runtime_error("can't parse " + path + ": " + document.getLastError());
                MyGUI::ResourceLayout layout(document.getRoot(), path);
                for (MyGUI::Widget* widget : layout.createLayout())
                    mLayoutWidgets.push_back(widget);
                return Json::object({ { "widgets", static_cast<double>(mLayoutWidgets.size()) } });
            };
        };
    }

    void Automation::closeLayout()
    {
        for (MyGUI::Widget* widget : mLayoutWidgets)
            MyGUI::Gui::getInstance().destroyWidget(widget);
        mLayoutWidgets.clear();
    }

    void Automation::beginConsoleCapture()
    {
        if (mCapturingConsole)
            throw std::runtime_error("another console or lua command is still running");
        mCapturingConsole = true;
        mConsoleOutput.clear();
        MWBase::Environment::get().getWindowManager()->setConsolePrintListener(
            [this](const std::string& text, std::string_view color) {
                mConsoleOutput.emplace_back(
                    text, Misc::StringUtils::ciEqual(color, MWBase::WindowManager::sConsoleColor_Error));
            });
    }

    Json Automation::endConsoleCapture()
    {
        MWBase::Environment::get().getWindowManager()->setConsolePrintListener(nullptr);
        mCapturingConsole = false;

        Json lines = Json::array();
        std::string errors;
        for (auto& [text, isError] : mConsoleOutput)
        {
            std::string line = text;
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
                line.pop_back();
            if (isError)
                errors += (errors.empty() ? "" : "\n") + line;
            lines.push(std::move(line));
        }
        mConsoleOutput.clear();
        if (!errors.empty())
            throw std::runtime_error(errors);
        return Json::object({ { "output", std::move(lines) } });
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
