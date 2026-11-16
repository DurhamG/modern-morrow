#include "cloudrequests.hpp"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <utility>

namespace MWState
{
    // Shared with the thread, which keeps it alive while it finishes a request after CloudRequests is gone.
    struct CloudRequests::State
    {
        std::unique_ptr<CloudSaves> mCloud;
        std::mutex mMutex;
        std::condition_variable mWake;
        bool mStop = false;
        bool mBusy = false;
        std::optional<Kind> mPending;
        std::string mPhrase;
        std::filesystem::path mFile;
        std::optional<Finished> mFinished;
        std::atomic<std::uint64_t> mDone{ 0 };
        std::atomic<std::uint64_t> mTotal{ 0 };

        void run()
        {
            for (;;)
            {
                Kind kind;
                std::string phrase;
                std::filesystem::path file;
                {
                    std::unique_lock lock(mMutex);
                    mWake.wait(lock, [&] { return mStop || mPending.has_value(); });
                    if (mStop)
                        return;
                    kind = *mPending;
                    mPending.reset();
                    phrase = mPhrase;
                    file = mFile;
                }

                const CloudProgress progress = [this](std::uint64_t done, std::uint64_t total) {
                    mDone = done;
                    mTotal = total;
                };
                CloudResult result;
                switch (kind)
                {
                    case Kind::Upload:
                        result = mCloud->upload(file, phrase, progress);
                        break;
                    case Kind::Details:
                        result = mCloud->details(phrase);
                        break;
                    case Kind::Download:
                        result = mCloud->download(phrase, file, progress);
                        break;
                }

                std::lock_guard lock(mMutex);
                mFinished = Finished{ kind, std::move(result), std::move(file) };
            }
        }
    };

    CloudRequests::CloudRequests(std::unique_ptr<CloudSaves> cloud)
        : mState(std::make_shared<State>())
    {
        mState->mCloud = std::move(cloud);
        // The thread holds its own reference to the state, so it can outlive this object.
        std::thread([state = mState] { state->run(); }).detach();
    }

    CloudRequests::~CloudRequests()
    {
        {
            std::lock_guard lock(mState->mMutex);
            mState->mStop = true;
        }
        mState->mWake.notify_one();
    }

    bool CloudRequests::busy() const
    {
        std::lock_guard lock(mState->mMutex);
        return mState->mBusy;
    }

    bool CloudRequests::start(Kind kind, const std::string& phrase, const std::filesystem::path& file)
    {
        {
            std::lock_guard lock(mState->mMutex);
            if (mState->mBusy)
                return false;
            mState->mBusy = true;
            mState->mPending = kind;
            mState->mPhrase = phrase;
            mState->mFile = file;
            mState->mDone = 0;
            mState->mTotal = 0;
        }
        mState->mWake.notify_one();
        return true;
    }

    bool CloudRequests::upload(const std::filesystem::path& save, const std::string& phrase)
    {
        return start(Kind::Upload, phrase, save);
    }

    bool CloudRequests::details(const std::string& phrase)
    {
        return start(Kind::Details, phrase, {});
    }

    bool CloudRequests::download(const std::string& phrase, const std::filesystem::path& to)
    {
        return start(Kind::Download, phrase, to);
    }

    CloudRequests::Progress CloudRequests::progress() const
    {
        return Progress{ mState->mDone, mState->mTotal };
    }

    std::optional<CloudRequests::Finished> CloudRequests::takeResult()
    {
        std::lock_guard lock(mState->mMutex);
        if (!mState->mFinished)
            return std::nullopt;
        std::optional<Finished> finished = std::move(mState->mFinished);
        mState->mFinished.reset();
        mState->mBusy = false;
        return finished;
    }
}
