#ifndef GAME_STATE_CLOUDREQUESTS_H
#define GAME_STATE_CLOUDREQUESTS_H

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "cloudsaves.hpp"

namespace MWState
{
    /// Runs cloud saves' requests on a thread of their own, one at a time, so the game keeps drawing while one waits
    /// for the service. The menus start a request, then look each frame for its progress and its result.
    class CloudRequests
    {
    public:
        enum class Kind
        {
            Upload,
            Details,
            Download,
        };

        struct Finished
        {
            Kind mKind;
            CloudResult mResult;
            // The file uploaded or downloaded.
            std::filesystem::path mFile;
        };

        struct Progress
        {
            std::uint64_t mDone = 0;
            std::uint64_t mTotal = 0;
        };

        explicit CloudRequests(std::unique_ptr<CloudSaves> cloud);

        /// A request that is still running finishes on its own after this, and its result is dropped.
        ~CloudRequests();

        CloudRequests(const CloudRequests&) = delete;
        CloudRequests& operator=(const CloudRequests&) = delete;

        /// Whether a request is running, or has finished and its result hasn't been taken yet. A new request can only
        /// start when this is false.
        bool busy() const;

        /// Each of these starts a request, and returns false when another is still busy.
        bool upload(const std::filesystem::path& save, const std::string& phrase);
        bool details(const std::string& phrase);
        bool download(const std::string& phrase, const std::filesystem::path& to);

        /// How far the running upload or download has got.
        Progress progress() const;

        /// The finished request's result, once. Until a request finishes, it returns nothing.
        std::optional<Finished> takeResult();

    private:
        struct State;

        bool start(Kind kind, const std::string& phrase, const std::filesystem::path& file);

        std::shared_ptr<State> mState;
    };
}

#endif
