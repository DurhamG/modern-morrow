#include "apps/openmw/mwstate/cloudrequests.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

namespace MWState
{
    namespace
    {
        using namespace std::chrono_literals;

        const std::string sDetails = R"({"playerName":"player","formatVersion":41,"size":10})";

        // Answers each request with the reply the test gives, once the test lets it. Meanwhile it reports half of an
        // upload or download as done.
        struct Gate
        {
            std::mutex mMutex;
            std::condition_variable mChanged;
            bool mOpen = false;
            int mStarted = 0;
            std::atomic<bool> mFinished{ false };

            void open()
            {
                std::lock_guard lock(mMutex);
                mOpen = true;
                mChanged.notify_all();
            }

            void waitForStart(int count)
            {
                std::unique_lock lock(mMutex);
                ASSERT_TRUE(mChanged.wait_for(lock, 5s, [&] { return mStarted >= count; }));
            }
        };

        class GatedTransport final : public CloudTransport
        {
        public:
            GatedTransport(std::shared_ptr<Gate> gate, CloudReply reply)
                : mGate(std::move(gate))
                , mReply(std::move(reply))
            {
            }

            CloudReply send(std::string_view, const std::string&, const std::string&, const std::filesystem::path*,
                const std::filesystem::path*, const CloudProgress& progress) override
            {
                if (progress)
                    progress(50, 100);
                std::unique_lock lock(mGate->mMutex);
                ++mGate->mStarted;
                mGate->mChanged.notify_all();
                mGate->mChanged.wait(lock, [&] { return mGate->mOpen; });
                mGate->mFinished = true;
                return mReply;
            }

        private:
            std::shared_ptr<Gate> mGate;
            CloudReply mReply;
        };

        std::unique_ptr<CloudRequests> makeRequests(const std::shared_ptr<Gate>& gate, CloudReply reply)
        {
            return std::make_unique<CloudRequests>(std::make_unique<CloudSaves>(
                std::make_unique<GatedTransport>(gate, std::move(reply)), "http://localhost:8787"));
        }

        std::optional<CloudRequests::Finished> waitForResult(CloudRequests& requests)
        {
            for (int i = 0; i < 500; ++i)
            {
                if (std::optional<CloudRequests::Finished> finished = requests.takeResult())
                    return finished;
                std::this_thread::sleep_for(10ms);
            }
            return std::nullopt;
        }

        TEST(MWStateCloudRequestsTest, shouldRunARequestAndGiveItsResultOnce)
        {
            const auto gate = std::make_shared<Gate>();
            gate->open();
            const auto requests = makeRequests(gate, CloudReply{ true, 200, sDetails });
            EXPECT_FALSE(requests->busy());
            ASSERT_TRUE(requests->details("silt strider"));
            const std::optional<CloudRequests::Finished> finished = waitForResult(*requests);
            ASSERT_TRUE(finished.has_value());
            EXPECT_EQ(finished->mKind, CloudRequests::Kind::Details);
            EXPECT_EQ(finished->mResult.mFailure, CloudFailure::None);
            EXPECT_EQ(finished->mResult.mDetails->mPlayerName, "player");
            EXPECT_FALSE(requests->takeResult().has_value());
            EXPECT_FALSE(requests->busy());
        }

        TEST(MWStateCloudRequestsTest, shouldRefuseASecondRequestUntilTheResultIsTaken)
        {
            const auto gate = std::make_shared<Gate>();
            const auto requests = makeRequests(gate, CloudReply{ true, 404, "" });
            ASSERT_TRUE(requests->download("silt strider", "to.omwsave"));
            gate->waitForStart(1);
            EXPECT_TRUE(requests->busy());
            EXPECT_FALSE(requests->details("silt strider"));
            EXPECT_FALSE(requests->takeResult().has_value());

            gate->open();
            const std::optional<CloudRequests::Finished> finished = waitForResult(*requests);
            ASSERT_TRUE(finished.has_value());
            EXPECT_EQ(finished->mKind, CloudRequests::Kind::Download);
            EXPECT_EQ(finished->mResult.mFailure, CloudFailure::NoSave);
            EXPECT_EQ(finished->mFile, std::filesystem::path("to.omwsave"));
            EXPECT_TRUE(requests->details("silt strider"));
            EXPECT_TRUE(waitForResult(*requests).has_value());
        }

        TEST(MWStateCloudRequestsTest, shouldShowAnUploadsProgressWhileItRuns)
        {
            const auto gate = std::make_shared<Gate>();
            const auto requests = makeRequests(gate, CloudReply{ true, 200, sDetails });
            ASSERT_TRUE(requests->upload("save.omwsave", "silt strider"));
            gate->waitForStart(1);
            EXPECT_EQ(requests->progress().mDone, 50u);
            EXPECT_EQ(requests->progress().mTotal, 100u);
            gate->open();
            const std::optional<CloudRequests::Finished> finished = waitForResult(*requests);
            ASSERT_TRUE(finished.has_value());
            EXPECT_EQ(finished->mKind, CloudRequests::Kind::Upload);
            EXPECT_EQ(finished->mFile, std::filesystem::path("save.omwsave"));
        }

        TEST(MWStateCloudRequestsTest, aRequestShouldFinishOnItsOwnAfterTheRequestsAreGone)
        {
            const auto gate = std::make_shared<Gate>();
            auto requests = makeRequests(gate, CloudReply{ true, 200, sDetails });
            ASSERT_TRUE(requests->details("silt strider"));
            gate->waitForStart(1);
            requests.reset();
            gate->open();
            for (int i = 0; i < 500 && !gate->mFinished; ++i)
                std::this_thread::sleep_for(10ms);
            EXPECT_TRUE(gate->mFinished);
        }
    }
}
