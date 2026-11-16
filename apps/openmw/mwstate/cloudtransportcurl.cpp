// The transport for cloud saves on Linux and macOS, through the system's libcurl, loaded when the first request is
// sent. The system's own libcurl knows where that system keeps its certificates for HTTPS, and is kept up to date with
// it, so the game carries no HTTPS library of its own.
#include "cloudsaves.hpp"

#include <dlfcn.h>

#include <cstdint>
#include <fstream>
#include <iterator>
#include <mutex>
#include <string>
#include <system_error>

#include <components/debug/debuglog.hpp>

namespace MWState
{
    namespace
    {
        // The parts of libcurl's API this uses, with the numbers curl/curl.h gives them. libcurl keeps them the same in
        // every libcurl.so.4, so building the game needs neither curl's headers nor libcurl.
        using CurlHandle = void;
        using CurlCode = int;
        using CurlOffset = long long;
        struct CurlList;

        constexpr CurlCode sCurlOk = 0;
        constexpr long sCurlGlobalDefault = 3;
        constexpr int sCurlInfoResponseCode = 0x200000 + 2;

        enum CurlOption : int
        {
            WriteData = 10000 + 1,
            Url = 10000 + 2,
            ReadData = 10000 + 9,
            WriteFunction = 20000 + 11,
            ReadFunction = 20000 + 12,
            UserAgent = 10000 + 18,
            LowSpeedLimit = 19,
            LowSpeedTime = 20,
            HttpHeader = 10000 + 23,
            CustomRequest = 10000 + 36,
            NoProgress = 43,
            Upload = 46,
            XferInfoData = 10000 + 57,
            ConnectTimeout = 78,
            NoSignal = 99,
            InFileSizeLarge = 30000 + 115,
            XferInfoFunction = 20000 + 219,
        };

        struct Curl
        {
            CurlCode (*mGlobalInit)(long flags);
            CurlHandle* (*mEasyInit)();
            CurlCode (*mEasySetopt)(CurlHandle* handle, int option, ...);
            CurlCode (*mEasyPerform)(CurlHandle* handle);
            CurlCode (*mEasyGetinfo)(CurlHandle* handle, int info, ...);
            void (*mEasyCleanup)(CurlHandle* handle);
            const char* (*mEasyStrerror)(CurlCode code);
            CurlList* (*mSlistAppend)(CurlList* list, const char* text);
            void (*mSlistFreeAll)(CurlList* list);
        };

        template <class Function>
        bool find(void* library, const char* name, Function& function)
        {
            function = reinterpret_cast<Function>(dlsym(library, name));
            return function != nullptr;
        }

        // Loads libcurl once. Returns null when the system has none.
        const Curl* loadCurl()
        {
            static std::once_flag once;
            static Curl curl;
            static bool loaded = false;
            std::call_once(once, [] {
                // Debian and Ubuntu also have a copy built on GnuTLS, under its own name.
                void* library = nullptr;
                for (const char* name : { "libcurl.so.4", "libcurl-gnutls.so.4", "libcurl.4.dylib" })
                    if ((library = dlopen(name, RTLD_NOW | RTLD_LOCAL)) != nullptr)
                        break;
                if (library == nullptr)
                {
                    Log(Debug::Warning) << "Cloud saves: this system has no libcurl, so cloud saves aren't available";
                    return;
                }
                loaded = find(library, "curl_global_init", curl.mGlobalInit)
                    && find(library, "curl_easy_init", curl.mEasyInit)
                    && find(library, "curl_easy_setopt", curl.mEasySetopt)
                    && find(library, "curl_easy_perform", curl.mEasyPerform)
                    && find(library, "curl_easy_getinfo", curl.mEasyGetinfo)
                    && find(library, "curl_easy_cleanup", curl.mEasyCleanup)
                    && find(library, "curl_easy_strerror", curl.mEasyStrerror)
                    && find(library, "curl_slist_append", curl.mSlistAppend)
                    && find(library, "curl_slist_free_all", curl.mSlistFreeAll)
                    && curl.mGlobalInit(sCurlGlobalDefault) == sCurlOk;
                if (!loaded)
                    Log(Debug::Warning) << "Cloud saves: this system's libcurl can't be used";
            });
            return loaded ? &curl : nullptr;
        }

        std::size_t writeToFile(char* data, std::size_t size, std::size_t count, void* file)
        {
            static_cast<std::ofstream*>(file)->write(data, static_cast<std::streamsize>(size * count));
            return static_cast<std::ofstream*>(file)->good() ? size * count : 0;
        }

        std::size_t writeToString(char* data, std::size_t size, std::size_t count, void* text)
        {
            static_cast<std::string*>(text)->append(data, size * count);
            return size * count;
        }

        std::size_t readFromFile(char* buffer, std::size_t size, std::size_t count, void* file)
        {
            std::ifstream& stream = *static_cast<std::ifstream*>(file);
            stream.read(buffer, static_cast<std::streamsize>(size * count));
            return static_cast<std::size_t>(stream.gcount());
        }

        int reportProgress(void* progress, CurlOffset downloadTotal, CurlOffset downloaded, CurlOffset uploadTotal,
            CurlOffset uploaded)
        {
            const CloudProgress& report = *static_cast<const CloudProgress*>(progress);
            if (uploadTotal > 0)
                report(static_cast<std::uint64_t>(uploaded), static_cast<std::uint64_t>(uploadTotal));
            else if (downloadTotal > 0)
                report(static_cast<std::uint64_t>(downloaded), static_cast<std::uint64_t>(downloadTotal));
            return 0;
        }

        class CurlTransport final : public CloudTransport
        {
        public:
            explicit CurlTransport(std::string userAgent)
                : mUserAgent(std::move(userAgent))
            {
            }

            CloudReply send(std::string_view method, const std::string& url, const std::string& authorization,
                const std::filesystem::path* bodyFile, const std::filesystem::path* downloadTo,
                const CloudProgress& progress) override
            {
                const Curl* curl = loadCurl();
                if (curl == nullptr)
                {
                    CloudReply reply;
                    reply.mAvailable = false;
                    return reply;
                }
                CurlHandle* handle = curl->mEasyInit();
                if (handle == nullptr)
                    return CloudReply{};
                const std::string header = "Authorization: " + authorization;
                CurlList* headers = curl->mSlistAppend(nullptr, header.c_str());

                curl->mEasySetopt(handle, Url, url.c_str());
                curl->mEasySetopt(handle, UserAgent, mUserAgent.c_str());
                curl->mEasySetopt(handle, HttpHeader, headers);
                // Signals can't be used from a thread of the game's own.
                curl->mEasySetopt(handle, NoSignal, 1L);
                // Connecting gives up after this long, and a transfer when nothing comes for a minute.
                curl->mEasySetopt(handle, ConnectTimeout, 15L);
                curl->mEasySetopt(handle, LowSpeedLimit, 1L);
                curl->mEasySetopt(handle, LowSpeedTime, 60L);

                std::ifstream body;
                if (bodyFile != nullptr)
                {
                    body.open(*bodyFile, std::ios::binary);
                    std::error_code ec;
                    const std::uintmax_t size = std::filesystem::file_size(*bodyFile, ec);
                    if (!body || ec)
                    {
                        curl->mSlistFreeAll(headers);
                        curl->mEasyCleanup(handle);
                        Log(Debug::Warning) << "Cloud saves: the save to upload can't be read";
                        return CloudReply{};
                    }
                    // An upload is a PUT, which is what the service takes.
                    curl->mEasySetopt(handle, Upload, 1L);
                    curl->mEasySetopt(handle, ReadFunction, &readFromFile);
                    curl->mEasySetopt(handle, ReadData, &body);
                    curl->mEasySetopt(handle, InFileSizeLarge, static_cast<CurlOffset>(size));
                }
                else if (method != "GET")
                    curl->mEasySetopt(handle, CustomRequest, std::string(method).c_str());

                if (progress)
                {
                    curl->mEasySetopt(handle, NoProgress, 0L);
                    curl->mEasySetopt(handle, XferInfoFunction, &reportProgress);
                    curl->mEasySetopt(handle, XferInfoData, &progress);
                }

                // A download goes to a file beside its destination first. It only takes the destination's name once
                // the whole save has come, so a download cut short leaves nothing.
                CloudReply reply;
                std::filesystem::path partial;
                std::ofstream file;
                if (downloadTo != nullptr)
                {
                    partial = *downloadTo;
                    partial += ".part";
                    file.open(partial, std::ios::binary | std::ios::trunc);
                    curl->mEasySetopt(handle, WriteFunction, &writeToFile);
                    curl->mEasySetopt(handle, WriteData, &file);
                }
                else
                {
                    curl->mEasySetopt(handle, WriteFunction, &writeToString);
                    curl->mEasySetopt(handle, WriteData, &reply.mBody);
                }

                const CurlCode result = curl->mEasyPerform(handle);
                long status = 0;
                curl->mEasyGetinfo(handle, sCurlInfoResponseCode, &status);
                curl->mSlistFreeAll(headers);
                curl->mEasyCleanup(handle);

                if (downloadTo != nullptr)
                {
                    file.close();
                    std::error_code ec;
                    if (result == sCurlOk && status == 200)
                    {
                        if (!file.fail())
                            std::filesystem::rename(partial, *downloadTo, ec);
                        if (file.fail() || ec)
                        {
                            std::filesystem::remove(partial, ec);
                            Log(Debug::Warning) << "Cloud saves: the downloaded save can't be written";
                            return CloudReply{};
                        }
                    }
                    else
                    {
                        // Anything but the save, such as an error, belongs in the reply.
                        if (result == sCurlOk)
                        {
                            std::ifstream error(partial, std::ios::binary);
                            reply.mBody.assign(std::istreambuf_iterator<char>(error), std::istreambuf_iterator<char>());
                        }
                        std::filesystem::remove(partial, ec);
                    }
                }
                if (result != sCurlOk)
                {
                    // The log says why, but never the address or the phrase.
                    Log(Debug::Warning) << "Cloud saves: the request failed: " << curl->mEasyStrerror(result);
                    return CloudReply{};
                }
                reply.mStatus = static_cast<int>(status);
                return reply;
            }

        private:
            std::string mUserAgent;
        };
    }

    std::unique_ptr<CloudTransport> makeCloudTransport(const std::string& userAgent)
    {
        return std::make_unique<CurlTransport>(userAgent);
    }
}
