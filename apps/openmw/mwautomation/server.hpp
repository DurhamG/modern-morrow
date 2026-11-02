#ifndef OPENMW_MWAUTOMATION_SERVER_H
#define OPENMW_MWAUTOMATION_SERVER_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace MWAutomation
{
    using ClientId = std::uint64_t;

    struct Request
    {
        ClientId mClient;
        std::string mLine;
    };

    // A TCP server on 127.0.0.1 that exchanges newline-terminated messages. It never blocks: poll() is called once per
    // frame to accept connections, read whole lines and send queued output.
    class Server
    {
    public:
        // Throws std::runtime_error if the port can't be opened.
        explicit Server(std::uint16_t port);
        ~Server();

        Server(const Server&) = delete;
        Server& operator=(const Server&) = delete;

        // Appends every complete line received since the last call.
        void poll(std::vector<Request>& requests);

        // Queues a line for the client. The newline is added here. Does nothing if the client has disconnected.
        void send(ClientId client, std::string line);

    private:
        struct Connection
        {
            std::intptr_t mSocket;
            std::string mInput;
            std::string mOutput;
        };

        std::intptr_t mListenSocket;
        ClientId mNextClient = 1;
        std::map<ClientId, Connection> mConnections;

        void accept();
        // Returns false when the connection should be closed.
        bool read(ClientId client, Connection& connection, std::vector<Request>& requests);
        bool flush(Connection& connection);
    };
}

#endif
