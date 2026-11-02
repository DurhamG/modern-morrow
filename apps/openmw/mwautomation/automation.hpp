#ifndef OPENMW_MWAUTOMATION_AUTOMATION_H
#define OPENMW_MWAUTOMATION_AUTOMATION_H

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "json.hpp"
#include "server.hpp"

struct SDL_Window;

namespace MWAutomation
{
    // Lets an external tool drive the game for UI testing. It is only created when openmw is started with
    // --automation-port. Requests and replies are single lines of JSON. A request names a command in "cmd" and may
    // carry an "id", which the reply repeats. A reply has "ok": true and a "result", or "ok": false and an "error".
    class Automation
    {
    public:
        Automation(std::uint16_t port, SDL_Window* window);

        // Runs once per frame, before input is processed, so that any input a command injects is handled in the
        // same frame.
        void update();

    private:
        // Called once per frame until it returns a result. It may throw std::runtime_error to fail the command.
        using Poll = std::function<std::optional<Json>()>;
        // Starts a command. Returns the function that finishes it.
        using Handler = std::function<Poll(const Json& request)>;

        struct Pending
        {
            ClientId mClient;
            Json mId;
            Poll mPoll;
        };

        Server mServer;
        SDL_Window* mWindow;
        std::uint64_t mFrame = 0;
        std::unordered_map<std::string, Handler> mHandlers;
        std::vector<Pending> mPending;

        void registerHandlers();
        void start(const Request& request);
        // Returns true when the command finished, successfully or not.
        bool step(Pending& pending);
        void reply(ClientId client, const Json& id, Json result);
        void replyError(ClientId client, const Json& id, std::string_view error);

        Json describeState() const;
    };
}

#endif
