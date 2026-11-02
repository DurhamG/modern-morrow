#include "server.hpp"

#include <algorithm>
#include <stdexcept>
#include <string_view>

#include <components/debug/debuglog.hpp>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace MWAutomation
{
    namespace
    {
#ifdef _WIN32
        using NativeSocket = SOCKET;
        constexpr NativeSocket sInvalidSocket = INVALID_SOCKET;

        void closeSocket(NativeSocket socket)
        {
            closesocket(socket);
        }

        bool wouldBlock()
        {
            return WSAGetLastError() == WSAEWOULDBLOCK;
        }

        void setNonBlocking(NativeSocket socket)
        {
            u_long enabled = 1;
            ioctlsocket(socket, FIONBIO, &enabled);
        }
#else
        using NativeSocket = int;
        constexpr NativeSocket sInvalidSocket = -1;

        void closeSocket(NativeSocket socket)
        {
            close(socket);
        }

        bool wouldBlock()
        {
#if EAGAIN == EWOULDBLOCK
            return errno == EAGAIN;
#else
            return errno == EAGAIN || errno == EWOULDBLOCK;
#endif
        }

        void setNonBlocking(NativeSocket socket)
        {
            fcntl(socket, F_SETFL, fcntl(socket, F_GETFL, 0) | O_NONBLOCK);
        }
#endif

        NativeSocket toNative(std::intptr_t socket)
        {
            return static_cast<NativeSocket>(socket);
        }

        // Limits how much one client can send without a newline, so a broken client can't use unbounded memory.
        constexpr std::size_t sMaxLineLength = 16 * 1024 * 1024;
    }

    Server::Server(std::uint16_t port)
    {
#ifdef _WIN32
        WSADATA data;
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
            throw std::runtime_error("WSAStartup failed");
#endif
        const NativeSocket listenSocket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listenSocket == sInvalidSocket)
            throw std::runtime_error("Failed to create the automation socket");

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::bind(listenSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0
            || ::listen(listenSocket, 4) != 0)
        {
            closeSocket(listenSocket);
            throw std::runtime_error("Failed to listen on 127.0.0.1:" + std::to_string(port));
        }
        setNonBlocking(listenSocket);
        mListenSocket = static_cast<std::intptr_t>(listenSocket);
        Log(Debug::Info) << "Automation server listening on 127.0.0.1:" << port;
    }

    Server::~Server()
    {
        for (auto& [_, connection] : mConnections)
            closeSocket(toNative(connection.mSocket));
        closeSocket(toNative(mListenSocket));
#ifdef _WIN32
        WSACleanup();
#endif
    }

    void Server::poll(std::vector<Request>& requests)
    {
        accept();
        for (auto it = mConnections.begin(); it != mConnections.end();)
        {
            if (read(it->first, it->second, requests) && flush(it->second))
                ++it;
            else
            {
                closeSocket(toNative(it->second.mSocket));
                it = mConnections.erase(it);
            }
        }
    }

    void Server::send(ClientId client, std::string line)
    {
        auto it = mConnections.find(client);
        if (it == mConnections.end())
            return;
        it->second.mOutput += line;
        it->second.mOutput += '\n';
        if (!flush(it->second))
        {
            closeSocket(toNative(it->second.mSocket));
            mConnections.erase(it);
        }
    }

    void Server::accept()
    {
        while (true)
        {
            const NativeSocket socket = ::accept(toNative(mListenSocket), nullptr, nullptr);
            if (socket == sInvalidSocket)
                return;
            setNonBlocking(socket);
            mConnections.emplace(mNextClient++, Connection{ static_cast<std::intptr_t>(socket), {}, {} });
        }
    }

    bool Server::read(ClientId client, Connection& connection, std::vector<Request>& requests)
    {
        char buffer[4096];
        while (true)
        {
            const auto received = ::recv(toNative(connection.mSocket), buffer, sizeof(buffer), 0);
            if (received == 0)
                return false;
            if (received < 0)
            {
                if (wouldBlock())
                    break;
                return false;
            }
            connection.mInput.append(buffer, static_cast<std::size_t>(received));
        }

        std::size_t start = 0;
        while (true)
        {
            const std::size_t end = connection.mInput.find('\n', start);
            if (end == std::string::npos)
                break;
            std::string_view line(connection.mInput.data() + start, end - start);
            if (!line.empty() && line.back() == '\r')
                line.remove_suffix(1);
            if (!line.empty())
                requests.push_back(Request{ client, std::string(line) });
            start = end + 1;
        }
        connection.mInput.erase(0, start);
        return connection.mInput.size() <= sMaxLineLength;
    }

    bool Server::flush(Connection& connection)
    {
        while (!connection.mOutput.empty())
        {
            const auto sent = ::send(toNative(connection.mSocket), connection.mOutput.data(),
                static_cast<int>(std::min<std::size_t>(connection.mOutput.size(), 1 << 20)), 0);
            if (sent < 0)
                return wouldBlock();
            connection.mOutput.erase(0, static_cast<std::size_t>(sent));
        }
        return true;
    }
}
