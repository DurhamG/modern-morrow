#include "automation.hpp"

#include <chrono>
#include <stdexcept>

#include <SDL_video.h>

#include <components/debug/debuglog.hpp>
#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/statemanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwgui/mode.hpp"

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

    Automation::Automation(std::uint16_t port, SDL_Window* window)
        : mServer(port)
        , mWindow(window)
    {
        registerHandlers();
    }

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

        mHandlers["quit"] = [](const Json&) -> Poll {
            return [] {
                Log(Debug::Info) << "Quit requested by the automation server";
                MWBase::Environment::get().getStateManager()->requestQuit();
                return Json::object();
            };
        };
    }
}
