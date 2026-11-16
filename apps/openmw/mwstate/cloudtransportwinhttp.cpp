// The Windows transport for cloud saves, through WinHTTP, which ships with Windows.
#include "cloudsaves.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <fstream>
#include <system_error>
#include <vector>

#include <components/debug/debuglog.hpp>

namespace MWState
{
    namespace
    {
        // Uploads go in pieces of this size, so their progress can be shown.
        constexpr std::size_t sChunk = 64 * 1024;

        std::wstring wide(std::string_view utf8)
        {
            if (utf8.empty())
                return {};
            const int size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
            std::wstring result(static_cast<std::size_t>(size), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), result.data(), size);
            return result;
        }

        class Handle
        {
        public:
            explicit Handle(HINTERNET handle = nullptr)
                : mHandle(handle)
            {
            }
            Handle(const Handle&) = delete;
            Handle& operator=(const Handle&) = delete;
            ~Handle()
            {
                if (mHandle != nullptr)
                    WinHttpCloseHandle(mHandle);
            }
            HINTERNET get() const { return mHandle; }

        private:
            HINTERNET mHandle;
        };

        // Logs why a request got no reply. The URL and the phrase aren't logged.
        CloudReply noReply(const char* step)
        {
            Log(Debug::Warning) << "Cloud saves: " << step << " failed with WinHTTP error " << GetLastError();
            return CloudReply{};
        }

        class WinHttpTransport final : public CloudTransport
        {
        public:
            explicit WinHttpTransport(const std::string& userAgent)
                : mSession(WinHttpOpen(wide(userAgent).c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0))
            {
                // Resolving the name, connecting, sending and waiting for the reply each give up after this long.
                if (mSession.get() != nullptr)
                    WinHttpSetTimeouts(mSession.get(), 15000, 15000, 60000, 60000);
            }

            CloudReply send(std::string_view method, const std::string& url, const std::string& authorization,
                const std::filesystem::path* bodyFile, const std::filesystem::path* downloadTo,
                const CloudProgress& progress) override
            {
                if (mSession.get() == nullptr)
                    return noReply("opening a session");

                std::wstring wideUrl = wide(url);
                URL_COMPONENTS parts{};
                parts.dwStructSize = sizeof(parts);
                parts.dwHostNameLength = static_cast<DWORD>(-1);
                parts.dwUrlPathLength = static_cast<DWORD>(-1);
                parts.dwExtraInfoLength = static_cast<DWORD>(-1);
                if (!WinHttpCrackUrl(wideUrl.c_str(), 0, 0, &parts))
                    return noReply("reading the address");
                const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
                std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
                path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);

                const Handle connection(WinHttpConnect(mSession.get(), host.c_str(), parts.nPort, 0));
                if (connection.get() == nullptr)
                    return noReply("connecting");
                const DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
                const Handle request(WinHttpOpenRequest(connection.get(), wide(method).c_str(), path.c_str(), nullptr,
                    WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
                if (request.get() == nullptr)
                    return noReply("opening the request");

                std::vector<char> body;
                if (bodyFile != nullptr)
                {
                    std::ifstream file(*bodyFile, std::ios::binary);
                    body.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
                    if (file.bad())
                        return noReply("reading the save");
                }
                const std::wstring headers = L"Authorization: " + wide(authorization);
                const DWORD total = static_cast<DWORD>(body.size());
                if (!WinHttpSendRequest(
                        request.get(), headers.c_str(), static_cast<DWORD>(-1), WINHTTP_NO_REQUEST_DATA, 0, total, 0))
                    return noReply("sending the request");
                for (std::size_t sent = 0; sent < body.size();)
                {
                    const DWORD piece = static_cast<DWORD>(std::min(sChunk, body.size() - sent));
                    DWORD written = 0;
                    if (!WinHttpWriteData(request.get(), body.data() + sent, piece, &written))
                        return noReply("sending the save");
                    sent += written;
                    if (progress)
                        progress(sent, body.size());
                }
                if (!WinHttpReceiveResponse(request.get(), nullptr))
                    return noReply("receiving the reply");

                CloudReply reply;
                DWORD status = 0;
                DWORD statusSize = sizeof(status);
                if (!WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX))
                    return noReply("reading the reply's status");
                DWORD length = 0;
                DWORD lengthSize = sizeof(length);
                WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                    WINHTTP_HEADER_NAME_BY_INDEX, &length, &lengthSize, WINHTTP_NO_HEADER_INDEX);

                // A download goes to a file beside its destination first, so a download cut short leaves nothing.
                const bool toFile = downloadTo != nullptr && status == 200;
                std::filesystem::path partial;
                std::ofstream file;
                if (toFile)
                {
                    partial = *downloadTo;
                    partial += ".part";
                    file.open(partial, std::ios::binary | std::ios::trunc);
                    if (!file)
                        return noReply("making the downloaded file");
                }
                std::vector<char> buffer(sChunk);
                std::uint64_t received = 0;
                for (;;)
                {
                    DWORD read = 0;
                    if (!WinHttpReadData(request.get(), buffer.data(), static_cast<DWORD>(buffer.size()), &read))
                    {
                        if (toFile)
                        {
                            file.close();
                            std::error_code ec;
                            std::filesystem::remove(partial, ec);
                        }
                        return noReply("receiving the reply's body");
                    }
                    if (read == 0)
                        break;
                    received += read;
                    if (toFile)
                    {
                        file.write(buffer.data(), read);
                        if (progress)
                            progress(received, length);
                    }
                    else
                        reply.mBody.append(buffer.data(), read);
                }
                if (toFile)
                {
                    file.close();
                    std::error_code ec;
                    std::filesystem::rename(partial, *downloadTo, ec);
                    if (file.fail() || ec)
                    {
                        std::filesystem::remove(partial, ec);
                        return noReply("writing the downloaded file");
                    }
                }
                reply.mStatus = static_cast<int>(status);
                return reply;
            }

        private:
            Handle mSession;
        };
    }

    std::unique_ptr<CloudTransport> makeCloudTransport(const std::string& userAgent)
    {
        return std::make_unique<WinHttpTransport>(userAgent);
    }
}
