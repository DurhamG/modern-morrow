#include "apps/openmw/mwstate/cloudsaves.hpp"

#include <components/testing/util.hpp>

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace MWState
{
    namespace
    {
        // The details the service returns for the cloudsaves/ tests' modern-morrow save.
        const std::string sDetails
            = R"({"playerName":"player","playerLevel":1,)"
              R"("cell":"Balmora, Caius Cosades' House","description":"Day 1 - 16 Last Seed 9 a.m.",)"
              R"("contentFiles":["builtin.omwscripts","Morrowind.esm"],"formatVersion":41,)"
              R"("size":108660,"uploaded":"2026-11-15T12:00:00.000Z"})";

        TEST(MWStateCloudAuthorizationTest, shouldSendThePhrasePercentEncoded)
        {
            EXPECT_EQ(cloudAuthorization("silt strider"), "Phrase silt%20strider");
            EXPECT_EQ(cloudAuthorization("Ghost-Fence_1.~"), "Phrase Ghost-Fence_1.~");
            EXPECT_EQ(cloudAuthorization("a/b%c"), "Phrase a%2Fb%25c");
        }

        TEST(MWStateCloudAuthorizationTest, shouldSendUtf8ForOtherAlphabets)
        {
            // cloudsaves/test/unit/phrase.test.ts decodes this phrase, sent the same way.
            EXPECT_EQ(
                cloudAuthorization("\xD0\xA1\xD0\xB8\xD0\xBB\xD1\x82 \xD0\xA1\xD1\x82\xD1\x80\xD0\xB0\xD0\xB9\xD0\xB4"
                                   "\xD0\xB5\xD1\x80"),
                "Phrase %D0%A1%D0%B8%D0%BB%D1%82%20%D0%A1%D1%82%D1%80%D0%B0%D0%B9%D0%B4%D0%B5%D1%80");
        }

        TEST(MWStateCloudDetailsTest, shouldReadTheServicesDetails)
        {
            const std::optional<CloudSaveDetails> details = parseCloudDetails(sDetails);
            ASSERT_TRUE(details.has_value());
            EXPECT_EQ(details->mPlayerName, "player");
            EXPECT_EQ(details->mPlayerLevel, 1);
            EXPECT_EQ(details->mCell, "Balmora, Caius Cosades' House");
            EXPECT_EQ(details->mDescription, "Day 1 - 16 Last Seed 9 a.m.");
            EXPECT_EQ(details->mContentFiles, (std::vector<std::string>{ "builtin.omwscripts", "Morrowind.esm" }));
            EXPECT_FALSE(details->mMoreContentFiles);
            EXPECT_EQ(details->mFormatVersion, 41);
            EXPECT_EQ(details->mSize, 108660u);
            EXPECT_EQ(details->mUploaded, "2026-11-15T12:00:00.000Z");
        }

        TEST(MWStateCloudDetailsTest, shouldTurnEachCharacterBackIntoTheSavesByte)
        {
            // The save's name was "Séverine" in windows-1252, where é is the byte E9. The service sends it as the
            // character U+00E9, which JSON carries as UTF-8, or escaped.
            const std::optional<CloudSaveDetails> raw = parseCloudDetails(R"({"playerName":"S)"
                                                                          "\xC3\xA9"
                                                                          R"(verine","formatVersion":41})");
            ASSERT_TRUE(raw.has_value());
            EXPECT_EQ(raw->mPlayerName, "S\xE9verine");
            const std::optional<CloudSaveDetails> escaped
                = parseCloudDetails(R"({"playerName":"Séverine","formatVersion":41})");
            ASSERT_TRUE(escaped.has_value());
            EXPECT_EQ(escaped->mPlayerName, "S\xE9verine");
        }

        TEST(MWStateCloudDetailsTest, shouldSayWhenContentFilesWereCutShort)
        {
            const std::optional<CloudSaveDetails> details = parseCloudDetails(
                R"({"playerName":"a","formatVersion":41,"contentFiles":["A.esp"],"moreContentFiles":true})");
            ASSERT_TRUE(details.has_value());
            EXPECT_TRUE(details->mMoreContentFiles);
        }

        TEST(MWStateCloudDetailsTest, shouldRefuseSomethingThatIsntTheDetails)
        {
            EXPECT_FALSE(parseCloudDetails("").has_value());
            EXPECT_FALSE(parseCloudDetails("not json").has_value());
            EXPECT_FALSE(parseCloudDetails("[]").has_value());
            EXPECT_FALSE(parseCloudDetails(R"({"error":"this phrase holds no save"})").has_value());
            EXPECT_FALSE(parseCloudDetails(R"({"playerName":"a"})").has_value());
        }

        TEST(MWStateCloudFailureTest, shouldNameWhatWentWrong)
        {
            EXPECT_EQ(cloudFailure(CloudReply{ true, 200, "" }), CloudFailure::None);
            EXPECT_EQ(cloudFailure(CloudReply{ true, 0, "" }), CloudFailure::NoConnection);
            EXPECT_EQ(cloudFailure(CloudReply{ false, 0, "" }), CloudFailure::NotAvailable);
            EXPECT_EQ(cloudFailure(CloudReply{ true, 400, "" }), CloudFailure::Refused);
            EXPECT_EQ(cloudFailure(CloudReply{ true, 401, "" }), CloudFailure::NoPhrase);
            EXPECT_EQ(cloudFailure(CloudReply{ true, 404, "" }), CloudFailure::NoSave);
            EXPECT_EQ(cloudFailure(CloudReply{ true, 413, "" }), CloudFailure::TooLarge);
            EXPECT_EQ(cloudFailure(CloudReply{ true, 429, "" }), CloudFailure::TooManyRequests);
            EXPECT_EQ(cloudFailure(CloudReply{ true, 500, "" }), CloudFailure::ServiceError);
            EXPECT_EQ(cloudFailure(CloudReply{ true, 1010, "" }), CloudFailure::ServiceError);
        }

        TEST(MWStateCloudFailureTest, shouldHaveAMessageForEachFailure)
        {
            EXPECT_TRUE(cloudFailureMessage(CloudFailure::None).empty());
            for (const CloudFailure failure : { CloudFailure::NotAvailable, CloudFailure::NoConnection,
                     CloudFailure::NoPhrase, CloudFailure::NoSave, CloudFailure::Refused, CloudFailure::TooLarge,
                     CloudFailure::TooManyRequests, CloudFailure::ServiceError })
                EXPECT_FALSE(cloudFailureMessage(failure).empty());
        }

        TEST(MWStateCloudDownloadPathTest, shouldBeNamedAfterTheDescription)
        {
            const std::filesystem::path dir = TestingOpenMW::currentTestDirPath() / "Lendias";
            std::filesystem::create_directories(dir);
            EXPECT_EQ(cloudDownloadPath(dir, "Day 1 - 16 Last Seed"), dir / "Day_1___16_Last_Seed.omwsave");
        }

        TEST(MWStateCloudDownloadPathTest, shouldNotReplaceASave)
        {
            const std::filesystem::path dir = TestingOpenMW::currentTestDirPath() / "Lendias";
            std::filesystem::create_directories(dir);
            std::ofstream(dir / "Quicksave.omwsave") << "x";
            std::ofstream(dir / "Quicksave - 1.omwsave") << "x";
            EXPECT_EQ(cloudDownloadPath(dir, "Quicksave"), dir / "Quicksave - 2.omwsave");
        }

        // Records each request and answers with a reply the test chose.
        class StandInTransport final : public CloudTransport
        {
        public:
            struct Request
            {
                std::string mMethod;
                std::string mUrl;
                std::string mAuthorization;
                std::optional<std::filesystem::path> mBodyFile;
                std::optional<std::filesystem::path> mDownloadTo;
            };

            explicit StandInTransport(CloudReply reply, std::vector<Request>& requests)
                : mReply(std::move(reply))
                , mRequests(requests)
            {
            }

            CloudReply send(std::string_view method, const std::string& url, const std::string& authorization,
                const std::filesystem::path* bodyFile, const std::filesystem::path* downloadTo,
                const CloudProgress& progress) override
            {
                mRequests.push_back(Request{ std::string(method), url, authorization,
                    bodyFile ? std::optional(*bodyFile) : std::nullopt,
                    downloadTo ? std::optional(*downloadTo) : std::nullopt });
                if (progress)
                    progress(10, 10);
                return mReply;
            }

        private:
            CloudReply mReply;
            std::vector<Request>& mRequests;
        };

        TEST(MWStateCloudSavesTest, uploadShouldPutTheSaveAndReadTheDetails)
        {
            std::vector<StandInTransport::Request> requests;
            CloudSaves cloud(std::make_unique<StandInTransport>(CloudReply{ true, 200, sDetails }, requests),
                "https://saves.example.workers.dev/");
            int progressCalls = 0;
            const CloudResult result
                = cloud.upload("save.omwsave", "silt strider", [&](std::uint64_t, std::uint64_t) { ++progressCalls; });
            EXPECT_EQ(result.mFailure, CloudFailure::None);
            ASSERT_TRUE(result.mDetails.has_value());
            EXPECT_EQ(result.mDetails->mCell, "Balmora, Caius Cosades' House");
            ASSERT_EQ(requests.size(), 1u);
            EXPECT_EQ(requests[0].mMethod, "PUT");
            EXPECT_EQ(requests[0].mUrl, "https://saves.example.workers.dev/v1/save");
            EXPECT_EQ(requests[0].mAuthorization, "Phrase silt%20strider");
            EXPECT_EQ(requests[0].mBodyFile, std::filesystem::path("save.omwsave"));
            EXPECT_EQ(progressCalls, 1);
        }

        TEST(MWStateCloudSavesTest, detailsShouldGetTheInfo)
        {
            std::vector<StandInTransport::Request> requests;
            CloudSaves cloud(std::make_unique<StandInTransport>(CloudReply{ true, 200, sDetails }, requests),
                "http://localhost:8787");
            EXPECT_EQ(cloud.details("silt strider").mDetails->mPlayerName, "player");
            EXPECT_EQ(requests.at(0).mMethod, "GET");
            EXPECT_EQ(requests.at(0).mUrl, "http://localhost:8787/v1/save/info");
            EXPECT_FALSE(requests.at(0).mBodyFile.has_value());
        }

        TEST(MWStateCloudSavesTest, downloadShouldGetTheSaveIntoTheFile)
        {
            std::vector<StandInTransport::Request> requests;
            CloudSaves cloud(
                std::make_unique<StandInTransport>(CloudReply{ true, 200, "" }, requests), "http://localhost:8787");
            EXPECT_EQ(cloud.download("silt strider", "to.omwsave", {}).mFailure, CloudFailure::None);
            EXPECT_EQ(requests.at(0).mUrl, "http://localhost:8787/v1/save");
            EXPECT_EQ(requests.at(0).mDownloadTo, std::filesystem::path("to.omwsave"));
        }

        TEST(MWStateCloudSavesTest, shouldPassOnWhyARequestFailed)
        {
            std::vector<StandInTransport::Request> requests;
            CloudSaves cloud(std::make_unique<StandInTransport>(
                                 CloudReply{ true, 404, R"({"error":"this phrase holds no save"})" }, requests),
                "http://localhost:8787");
            const CloudResult result = cloud.details("nothing here");
            EXPECT_EQ(result.mFailure, CloudFailure::NoSave);
            EXPECT_FALSE(result.mDetails.has_value());
        }

        // Talks to a real copy of the service through this platform's transport, when OPENMW_CLOUD_TEST_ADDRESS
        // names one, such as http://localhost:8787 from "npm run dev" in cloudsaves/. OPENMW_CLOUD_TEST_SAVE names a
        // save to upload, such as cloudsaves/test/saves/modern-morrow.omwsave.
        TEST(MWStateCloudTransportTest, shouldUploadAndDownloadThroughTheService)
        {
            const char* address = std::getenv("OPENMW_CLOUD_TEST_ADDRESS");
            const char* savePath = std::getenv("OPENMW_CLOUD_TEST_SAVE");
            if (address == nullptr || savePath == nullptr)
                GTEST_SKIP() << "OPENMW_CLOUD_TEST_ADDRESS and OPENMW_CLOUD_TEST_SAVE aren't set";
            CloudSaves cloud(makeCloudTransport("ModernMorrow-tests"), address);
            const std::filesystem::path save(savePath);
            const std::string phrase = "openmw-tests transport check";

            std::uint64_t uploaded = 0;
            const CloudResult upload
                = cloud.upload(save, phrase, [&](std::uint64_t done, std::uint64_t) { uploaded = done; });
            ASSERT_EQ(upload.mFailure, CloudFailure::None);
            EXPECT_EQ(uploaded, std::filesystem::file_size(save));
            EXPECT_EQ(cloud.details(phrase).mDetails->mSize, std::filesystem::file_size(save));

            const std::filesystem::path to = TestingOpenMW::currentTestDirPath() / "downloaded.omwsave";
            std::filesystem::create_directories(to.parent_path());
            ASSERT_EQ(cloud.download(phrase, to, {}).mFailure, CloudFailure::None);
            std::ifstream a(save, std::ios::binary);
            std::ifstream b(to, std::ios::binary);
            EXPECT_EQ(
                std::string(std::istreambuf_iterator<char>(a), {}), std::string(std::istreambuf_iterator<char>(b), {}));
            EXPECT_FALSE(std::filesystem::exists(to.string() + ".part"));

            EXPECT_EQ(cloud.details("openmw-tests phrase with nothing").mFailure, CloudFailure::NoSave);
            const std::filesystem::path nothing = TestingOpenMW::currentTestDirPath() / "nothing.omwsave";
            EXPECT_EQ(cloud.download("openmw-tests phrase with nothing", nothing, {}).mFailure, CloudFailure::NoSave);
            EXPECT_FALSE(std::filesystem::exists(nothing));
        }

        TEST(MWStateCloudTransportTest, shouldSayWhenThereIsNoConnection)
        {
            // Nothing listens on port 9 of this computer, the discard port.
            CloudSaves cloud(makeCloudTransport("ModernMorrow-tests"), "http://127.0.0.1:9");
            const CloudFailure failure = cloud.details("a").mFailure;
            EXPECT_TRUE(failure == CloudFailure::NoConnection || failure == CloudFailure::NotAvailable);
        }

        TEST(MWStateCloudSavesTest, aReplyWithoutDetailsShouldBeAServiceError)
        {
            std::vector<StandInTransport::Request> requests;
            CloudSaves cloud(std::make_unique<StandInTransport>(CloudReply{ true, 200, "<html>" }, requests),
                "http://localhost:8787");
            EXPECT_EQ(cloud.upload("save.omwsave", "a", {}).mFailure, CloudFailure::ServiceError);
            EXPECT_EQ(cloud.details("a").mFailure, CloudFailure::ServiceError);
        }
    }
}
