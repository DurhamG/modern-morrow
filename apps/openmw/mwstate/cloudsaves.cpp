#include "cloudsaves.hpp"

#include <array>
#include <charconv>
#include <chrono>
#include <stdexcept>
#include <utility>

#include <components/misc/utf8stream.hpp>

#include "../mwautomation/json.hpp"

#include "charactermanager.hpp"

namespace MWState
{
    namespace
    {
        bool isUnreserved(unsigned char c)
        {
            return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.'
                || c == '_' || c == '~';
        }

        // The service sends each byte of a save's 8-bit string as the character with the same number. A character
        // above 255 can't have come from a save, so it becomes "?".
        std::string legacyBytes(std::string_view utf8)
        {
            std::string bytes;
            Utf8Stream stream(utf8);
            while (!stream.eof())
            {
                const Utf8Stream::UnicodeChar c = stream.consume();
                bytes += c <= 0xFF ? static_cast<char>(c) : '?';
            }
            return bytes;
        }

        std::string legacyString(const MWAutomation::Json& object, std::string_view key)
        {
            const MWAutomation::Json* value = object.find(key);
            return value != nullptr && value->isString() ? legacyBytes(value->asString()) : std::string();
        }
    }

    std::string cloudAuthorization(std::string_view phrase)
    {
        constexpr std::string_view hex = "0123456789ABCDEF";
        std::string result = "Phrase ";
        for (const char ch : phrase)
        {
            const unsigned char c = static_cast<unsigned char>(ch);
            if (isUnreserved(c))
                result += ch;
            else
            {
                result += '%';
                result += hex[c >> 4];
                result += hex[c & 0xF];
            }
        }
        return result;
    }

    std::optional<CloudSaveDetails> parseCloudDetails(std::string_view json)
    {
        MWAutomation::Json root;
        try
        {
            root = MWAutomation::Json::parse(json);
        }
        catch (const std::runtime_error&)
        {
            return std::nullopt;
        }
        const MWAutomation::Json* name = root.find("playerName");
        const MWAutomation::Json* format = root.find("formatVersion");
        if (!root.isObject() || name == nullptr || !name->isString() || format == nullptr || !format->isNumber())
            return std::nullopt;

        CloudSaveDetails details;
        details.mPlayerName = legacyBytes(name->asString());
        if (const MWAutomation::Json* level = root.find("playerLevel"); level != nullptr && level->isNumber())
            details.mPlayerLevel = static_cast<int>(level->asNumber());
        details.mCell = legacyString(root, "cell");
        details.mDescription = legacyString(root, "description");
        if (const MWAutomation::Json* files = root.find("contentFiles"); files != nullptr && files->isArray())
            for (const MWAutomation::Json& file : files->asArray())
                if (file.isString())
                    details.mContentFiles.push_back(legacyBytes(file.asString()));
        if (const MWAutomation::Json* more = root.find("moreContentFiles"); more != nullptr && more->isBool())
            details.mMoreContentFiles = more->asBool();
        details.mFormatVersion = static_cast<int>(format->asNumber());
        if (const MWAutomation::Json* size = root.find("size"); size != nullptr && size->isNumber())
            details.mSize = static_cast<std::uint64_t>(size->asNumber());
        if (const MWAutomation::Json* uploaded = root.find("uploaded"); uploaded != nullptr && uploaded->isString())
            details.mUploaded = uploaded->asString();
        return details;
    }

    std::optional<std::time_t> parseCloudTime(std::string_view uploaded)
    {
        // The service writes JavaScript's Date.toISOString(): YYYY-MM-DDTHH:MM:SS, then milliseconds and Z.
        const auto number = [&](std::size_t at, std::size_t length) -> std::optional<int> {
            int value = 0;
            if (uploaded.size() < at + length)
                return std::nullopt;
            const auto [end, error] = std::from_chars(uploaded.data() + at, uploaded.data() + at + length, value);
            if (error != std::errc() || end != uploaded.data() + at + length)
                return std::nullopt;
            return value;
        };
        const std::optional<int> year = number(0, 4), month = number(5, 2), day = number(8, 2), hour = number(11, 2),
                                 minute = number(14, 2), second = number(17, 2);
        if (!year || !month || !day || !hour || !minute || !second || uploaded[4] != '-' || uploaded[7] != '-'
            || uploaded[10] != 'T' || uploaded[13] != ':' || uploaded[16] != ':')
            return std::nullopt;
        const std::chrono::year_month_day date{ std::chrono::year(*year), std::chrono::month(*month),
            std::chrono::day(*day) };
        if (!date.ok() || *hour > 23 || *minute > 59 || *second > 60)
            return std::nullopt;
        const std::chrono::sys_seconds time = std::chrono::sys_days(date) + std::chrono::hours(*hour)
            + std::chrono::minutes(*minute) + std::chrono::seconds(*second);
        return std::chrono::system_clock::to_time_t(time);
    }

    CloudFailure cloudFailure(const CloudReply& reply)
    {
        if (!reply.mAvailable)
            return CloudFailure::NotAvailable;
        switch (reply.mStatus)
        {
            case 0:
                return CloudFailure::NoConnection;
            case 200:
                return CloudFailure::None;
            case 400:
                return CloudFailure::Refused;
            case 401:
                return CloudFailure::NoPhrase;
            case 404:
                return CloudFailure::NoSave;
            case 413:
                return CloudFailure::TooLarge;
            case 429:
                return CloudFailure::TooManyRequests;
            default:
                return CloudFailure::ServiceError;
        }
    }

    std::string_view cloudFailureMessage(CloudFailure failure)
    {
        switch (failure)
        {
            case CloudFailure::None:
                return {};
            case CloudFailure::NotAvailable:
                return "CloudNotAvailable";
            case CloudFailure::NoConnection:
                return "CloudNoConnection";
            case CloudFailure::NoPhrase:
                return "CloudNoPhrase";
            case CloudFailure::NoSave:
                return "CloudNoSave";
            case CloudFailure::Refused:
                return "CloudRefused";
            case CloudFailure::TooLarge:
                return "CloudTooLarge";
            case CloudFailure::TooManyRequests:
                return "CloudTooManyRequests";
            case CloudFailure::ServiceError:
                return "CloudServiceError";
        }
        return "CloudServiceError";
    }

    std::filesystem::path cloudDownloadPath(const std::filesystem::path& characterDir, std::string_view description)
    {
        // A save's file name follows the same rule as a character's folder name. See Character::addSlot.
        const std::string name = characterFolderName(description);
        const std::string ext = ".omwsave";
        std::filesystem::path path = characterDir / (name + ext);
        for (int i = 1; std::filesystem::exists(path); ++i)
            path = characterDir / (name + " - " + std::to_string(i) + ext);
        return path;
    }

    CloudSaves::CloudSaves(std::unique_ptr<CloudTransport> transport, std::string address)
        : mTransport(std::move(transport))
        , mAddress(std::move(address))
    {
        while (!mAddress.empty() && mAddress.back() == '/')
            mAddress.pop_back();
    }

    CloudResult CloudSaves::upload(
        const std::filesystem::path& save, std::string_view phrase, const CloudProgress& progress)
    {
        const CloudReply reply
            = mTransport->send("PUT", mAddress + "/v1/save", cloudAuthorization(phrase), &save, nullptr, progress);
        CloudResult result{ cloudFailure(reply), std::nullopt };
        if (result.mFailure == CloudFailure::None)
        {
            result.mDetails = parseCloudDetails(reply.mBody);
            if (!result.mDetails)
                result.mFailure = CloudFailure::ServiceError;
        }
        return result;
    }

    CloudResult CloudSaves::details(std::string_view phrase)
    {
        const CloudReply reply
            = mTransport->send("GET", mAddress + "/v1/save/info", cloudAuthorization(phrase), nullptr, nullptr, {});
        CloudResult result{ cloudFailure(reply), std::nullopt };
        if (result.mFailure == CloudFailure::None)
        {
            result.mDetails = parseCloudDetails(reply.mBody);
            if (!result.mDetails)
                result.mFailure = CloudFailure::ServiceError;
        }
        return result;
    }

    CloudResult CloudSaves::download(
        std::string_view phrase, const std::filesystem::path& to, const CloudProgress& progress)
    {
        const CloudReply reply
            = mTransport->send("GET", mAddress + "/v1/save", cloudAuthorization(phrase), nullptr, &to, progress);
        return CloudResult{ cloudFailure(reply), std::nullopt };
    }
}
