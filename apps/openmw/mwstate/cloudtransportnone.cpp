// The transport for platforms where cloud saves can't send requests yet. Every request says so, and the Cloud tab
// shows that cloud saves aren't available here.
#include "cloudsaves.hpp"

namespace MWState
{
    namespace
    {
        class NoTransport final : public CloudTransport
        {
        public:
            CloudReply send(std::string_view, const std::string&, const std::string&, const std::filesystem::path*,
                const std::filesystem::path*, const CloudProgress&) override
            {
                CloudReply reply;
                reply.mAvailable = false;
                return reply;
            }
        };
    }

    std::unique_ptr<CloudTransport> makeCloudTransport(const std::string&)
    {
        return std::make_unique<NoTransport>();
    }
}
