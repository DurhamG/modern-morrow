#include "apps/openmw/mwstate/cloudsaves.hpp"

#include <components/testing/util.hpp>

#include <gtest/gtest.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

namespace MWState
{
    namespace
    {
#ifdef _WIN32
        using Socket = SOCKET;
        const Socket sNoSocket = INVALID_SOCKET;
        constexpr int sSendFlags = 0;
        constexpr int sStopSending = SD_SEND;
        void closeSocket(Socket socket)
        {
            closesocket(socket);
        }
#else
        using Socket = int;
        const Socket sNoSocket = -1;
        // A write to a connection the client has closed would otherwise stop the test program with SIGPIPE.
        constexpr int sSendFlags = MSG_NOSIGNAL;
        constexpr int sStopSending = SHUT_WR;
        void closeSocket(Socket socket)
        {
            close(socket);
        }
#endif

        // A small HTTP server on this computer, for testing the platform's transport without the cloud saves
        // service. It gives every request the same reply, and keeps the last request it read.
        class LocalServer
        {
        public:
            struct Reply
            {
                int mStatus = 200;
                std::string mBody;
                // The Content-Length the reply gives. When it is more than the body's size, the server closes the
                // connection after the body, as a connection that drops in the middle of a download does.
                std::optional<std::size_t> mLength;
            };

            struct Request
            {
                std::string mMethod;
                std::string mTarget;
                // The header names are in lower case.
                std::map<std::string, std::string> mHeaders;
                std::string mBody;
            };

            explicit LocalServer(Reply reply)
                : mReply(std::move(reply))
            {
#ifdef _WIN32
                WSADATA data;
                WSAStartup(MAKEWORD(2, 2), &data);
#endif
                mListener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                sockaddr_in address{};
                address.sin_family = AF_INET;
                address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
                address.sin_port = 0;
                socklen_t size = sizeof(address);
                if (mListener == sNoSocket || bind(mListener, reinterpret_cast<sockaddr*>(&address), size) != 0
                    || listen(mListener, 4) != 0
                    || getsockname(mListener, reinterpret_cast<sockaddr*>(&address), &size) != 0)
                    throw std::runtime_error("the test server can't listen");
                mPort = ntohs(address.sin_port);
                mThread = std::thread([this] { run(); });
            }

            ~LocalServer()
            {
                // Connecting wakes the thread from accept, so it sees that it should stop.
                mStopping = true;
                const Socket wake = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                sockaddr_in address{};
                address.sin_family = AF_INET;
                address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
                address.sin_port = htons(mPort);
                connect(wake, reinterpret_cast<sockaddr*>(&address), sizeof(address));
                mThread.join();
                closeSocket(wake);
                closeSocket(mListener);
#ifdef _WIN32
                WSACleanup();
#endif
            }

            std::string address() const { return "http://127.0.0.1:" + std::to_string(mPort); }

            Request lastRequest()
            {
                std::lock_guard lock(mMutex);
                return mLast;
            }

        private:
            void run()
            {
                for (;;)
                {
                    const Socket client = accept(mListener, nullptr, nullptr);
                    if (client == sNoSocket)
                        return;
                    if (mStopping)
                    {
                        closeSocket(client);
                        return;
                    }
                    serve(client);
                    closeSocket(client);
                }
            }

            static bool sendAll(Socket client, const std::string& data)
            {
                for (std::size_t sent = 0; sent < data.size();)
                {
                    const int piece = static_cast<int>(std::min<std::size_t>(data.size() - sent, 65536));
                    const auto written = ::send(client, data.data() + sent, piece, sSendFlags);
                    if (written <= 0)
                        return false;
                    sent += static_cast<std::size_t>(written);
                }
                return true;
            }

            void serve(Socket client)
            {
                std::string data;
                char buffer[65536];
                const auto receive = [&] {
                    const auto count = recv(client, buffer, static_cast<int>(sizeof(buffer)), 0);
                    if (count <= 0)
                        return false;
                    data.append(buffer, static_cast<std::size_t>(count));
                    return true;
                };

                std::size_t headersEnd = std::string::npos;
                while ((headersEnd = data.find("\r\n\r\n")) == std::string::npos)
                    if (!receive())
                        return;

                Request request;
                std::size_t lineStart = 0;
                for (std::size_t lineEnd = 0; (lineEnd = data.find("\r\n", lineStart)) <= headersEnd;
                    lineStart = lineEnd + 2)
                {
                    const std::string line = data.substr(lineStart, lineEnd - lineStart);
                    if (lineStart == 0)
                    {
                        const std::size_t first = line.find(' ');
                        const std::size_t second = line.find(' ', first + 1);
                        request.mMethod = line.substr(0, first);
                        request.mTarget = line.substr(first + 1, second - first - 1);
                        continue;
                    }
                    const std::size_t colon = line.find(':');
                    if (colon == std::string::npos)
                        continue;
                    std::string name = line.substr(0, colon);
                    std::transform(name.begin(), name.end(), name.begin(),
                        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                    std::string value = line.substr(colon + 1);
                    value.erase(0, value.find_first_not_of(' '));
                    request.mHeaders[name] = value;
                }

                // curl asks before it sends a large body.
                if (request.mHeaders["expect"] == "100-continue" && !sendAll(client, "HTTP/1.1 100 Continue\r\n\r\n"))
                    return;
                const std::string& lengthHeader = request.mHeaders["content-length"];
                const std::size_t length = lengthHeader.empty() ? 0 : std::stoull(lengthHeader);
                data.erase(0, headersEnd + 4);
                while (data.size() < length)
                    if (!receive())
                        return;
                request.mBody = std::move(data);
                {
                    std::lock_guard lock(mMutex);
                    mLast = request;
                }

                const std::size_t declared = mReply.mLength.value_or(mReply.mBody.size());
                const std::string head = "HTTP/1.1 " + std::to_string(mReply.mStatus)
                    + " Reply\r\nContent-Length: " + std::to_string(declared) + "\r\nConnection: close\r\n\r\n";
                if (sendAll(client, head + mReply.mBody))
                    shutdown(client, sStopSending);
            }

            const Reply mReply;
            Socket mListener = sNoSocket;
            unsigned short mPort = 0;
            std::atomic<bool> mStopping = false;
            std::mutex mMutex;
            Request mLast;
            std::thread mThread;
        };

        const std::string sUserAgent = "ModernMorrow/tests";
        const std::string sAuthorization = "Phrase a%20test%20phrase";

        // Bytes of every value, so a transport that changed any of them, such as by treating the file as text, fails.
        std::string makeSave(std::size_t size)
        {
            std::string save(size, '\0');
            for (std::size_t i = 0; i < size; ++i)
                save[i] = static_cast<char>((i * 7919 + i / 256) % 256);
            return save;
        }

        std::filesystem::path testFile(const std::string& name)
        {
            const std::filesystem::path path = TestingOpenMW::currentTestDirPath() / name;
            std::filesystem::create_directories(path.parent_path());
            std::filesystem::remove(path);
            std::filesystem::remove(path.string() + ".part");
            return path;
        }

        std::string readFile(const std::filesystem::path& path)
        {
            std::ifstream file(path, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(file), {});
        }

        TEST(MWStateCloudTransportLocalTest, shouldSendAnUploadByteForByte)
        {
            const std::string save = makeSave(300000);
            const std::filesystem::path path = testFile("upload.omwsave");
            std::ofstream(path, std::ios::binary) << save;
            LocalServer server({ 200, R"({"ok":true})", {} });

            std::uint64_t done = 0;
            std::uint64_t total = 0;
            const CloudReply reply = makeCloudTransport(sUserAgent)
                                         ->send("PUT", server.address() + "/save", sAuthorization, &path, nullptr,
                                             [&](std::uint64_t d, std::uint64_t t) {
                                                 done = d;
                                                 total = t;
                                             });
            ASSERT_TRUE(reply.mAvailable);
            EXPECT_EQ(reply.mStatus, 200);
            EXPECT_EQ(reply.mBody, R"({"ok":true})");

            LocalServer::Request request = server.lastRequest();
            EXPECT_EQ(request.mMethod, "PUT");
            EXPECT_EQ(request.mTarget, "/save");
            EXPECT_EQ(request.mBody.size(), save.size());
            EXPECT_TRUE(request.mBody == save);
            EXPECT_EQ(request.mHeaders["content-length"], std::to_string(save.size()));
            EXPECT_EQ(request.mHeaders["authorization"], sAuthorization);
            EXPECT_EQ(request.mHeaders["user-agent"], sUserAgent);
            EXPECT_EQ(done, save.size());
            EXPECT_EQ(total, save.size());
        }

        TEST(MWStateCloudTransportLocalTest, shouldDownloadTheWholeSave)
        {
            const std::string save = makeSave(200000);
            LocalServer server({ 200, save, {} });
            const std::filesystem::path to = testFile("whole.omwsave");

            const CloudReply reply = makeCloudTransport(sUserAgent)
                                         ->send("GET", server.address() + "/save", sAuthorization, nullptr, &to, {});
            ASSERT_TRUE(reply.mAvailable);
            EXPECT_EQ(reply.mStatus, 200);
            EXPECT_TRUE(reply.mBody.empty());
            EXPECT_TRUE(readFile(to) == save);
            EXPECT_FALSE(std::filesystem::exists(to.string() + ".part"));
            EXPECT_EQ(server.lastRequest().mMethod, "GET");
        }

        TEST(MWStateCloudTransportLocalTest, shouldLeaveNothingWhenADownloadIsCutShort)
        {
            // The reply says the save has 4000 bytes, and the connection closes after 1000 of them.
            LocalServer server({ 200, makeSave(1000), 4000 });
            const std::filesystem::path to = testFile("cut-short.omwsave");

            const CloudReply reply = makeCloudTransport(sUserAgent)
                                         ->send("GET", server.address() + "/save", sAuthorization, nullptr, &to, {});
            ASSERT_TRUE(reply.mAvailable);
            EXPECT_EQ(reply.mStatus, 0);
            EXPECT_FALSE(std::filesystem::exists(to));
            EXPECT_FALSE(std::filesystem::exists(to.string() + ".part"));
        }

        TEST(MWStateCloudTransportLocalTest, shouldKeepTheServicesTextWhenThereIsNoSave)
        {
            LocalServer server({ 404, "this phrase holds no save", {} });
            const std::filesystem::path to = testFile("no-save.omwsave");

            const CloudReply reply = makeCloudTransport(sUserAgent)
                                         ->send("GET", server.address() + "/save", sAuthorization, nullptr, &to, {});
            ASSERT_TRUE(reply.mAvailable);
            EXPECT_EQ(reply.mStatus, 404);
            EXPECT_EQ(reply.mBody, "this phrase holds no save");
            EXPECT_FALSE(std::filesystem::exists(to));
            EXPECT_FALSE(std::filesystem::exists(to.string() + ".part"));
        }
    }
}
