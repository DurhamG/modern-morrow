#ifndef GAME_STATE_CLOUDSAVES_H
#define GAME_STATE_CLOUDSAVES_H

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace MWState
{
    /// A cloud save moves a game from one device to another. The service at the "cloud address" setting keeps one save
    /// for each phrase. See ui-rework/cloud-saves.md, and cloudsaves/ for the service itself.

    /// The details of the save a phrase holds, as the service returns them. The strings are the save's own bytes, in
    /// the game's 8-bit encoding, as the details of a local save are.
    struct CloudSaveDetails
    {
        std::string mPlayerName;
        std::optional<int> mPlayerLevel;
        std::string mCell;
        std::string mDescription;
        std::vector<std::string> mContentFiles;
        // The service cuts a long list of content files short, and says so.
        bool mMoreContentFiles = false;
        int mFormatVersion = 0;
        std::uint64_t mSize = 0;
        // When it was uploaded, such as "2026-11-15T12:00:00.000Z".
        std::string mUploaded;
    };

    /// What came back from the service.
    struct CloudReply
    {
        // False when this platform has no way to send requests yet.
        bool mAvailable = true;
        // The HTTP status, or 0 when no reply came, such as without a connection.
        int mStatus = 0;
        // The reply's body. A downloaded save goes to its file instead.
        std::string mBody;
    };

    /// Why a request failed, for the message the menus show.
    enum class CloudFailure
    {
        None,
        NotAvailable,
        NoConnection,
        NoPhrase,
        NoSave,
        Refused,
        TooLarge,
        TooManyRequests,
        ServiceError,
    };

    /// The Authorization header's value for a phrase. Headers only carry ASCII safely, so the phrase goes as UTF-8
    /// with every byte other than an ASCII letter, a digit or one of "-._~" percent-encoded, as a URL carries it. The
    /// service decodes it and ignores case and spacing.
    std::string cloudAuthorization(std::string_view phrase);

    /// Reads the details from the service's JSON. The service sends each byte of the save's 8-bit strings as the
    /// character with the same number, which this turns back into the bytes. Returns nothing when the JSON isn't
    /// the details.
    std::optional<CloudSaveDetails> parseCloudDetails(std::string_view json);

    CloudFailure cloudFailure(const CloudReply& reply);

    /// The localisation key of the message for a failure, in the Interface context.
    std::string_view cloudFailureMessage(CloudFailure failure);

    /// The file a downloaded cloud save is written to, in a character's folder. It is named after the save's
    /// description, as a local save is, with a number added when that name is taken.
    std::filesystem::path cloudDownloadPath(const std::filesystem::path& characterDir, std::string_view description);

    /// Told how many bytes of an upload or download have gone, and how many there are in all.
    using CloudProgress = std::function<void(std::uint64_t done, std::uint64_t total)>;

    /// Sends HTTP requests. Each platform has its own, made by makeCloudTransport.
    class CloudTransport
    {
    public:
        virtual ~CloudTransport() = default;

        /// Sends a request and waits for the reply. The body comes from bodyFile when one is given. When downloadTo
        /// is given and the status is 200, the reply's body is written to that file instead of the reply. The file
        /// only appears once the whole body has arrived.
        virtual CloudReply send(std::string_view method, const std::string& url, const std::string& authorization,
            const std::filesystem::path* bodyFile, const std::filesystem::path* downloadTo,
            const CloudProgress& progress)
            = 0;
    };

    /// This platform's transport. userAgent is sent with every request.
    std::unique_ptr<CloudTransport> makeCloudTransport(const std::string& userAgent);

    /// The result of a request: the save's details, or why it failed.
    struct CloudResult
    {
        CloudFailure mFailure = CloudFailure::None;
        std::optional<CloudSaveDetails> mDetails;
    };

    /// The three requests the service answers. Each waits for the reply, so the menus run them on a thread of their
    /// own.
    class CloudSaves
    {
    public:
        CloudSaves(std::unique_ptr<CloudTransport> transport, std::string address);

        /// Uploads a save, which becomes the phrase's save. The result has the details the service read from it.
        CloudResult upload(const std::filesystem::path& save, std::string_view phrase, const CloudProgress& progress);

        /// The details of the phrase's save.
        CloudResult details(std::string_view phrase);

        /// Downloads the phrase's save to a file.
        CloudResult download(std::string_view phrase, const std::filesystem::path& to, const CloudProgress& progress);

    private:
        std::unique_ptr<CloudTransport> mTransport;
        std::string mAddress;
    };
}

#endif
