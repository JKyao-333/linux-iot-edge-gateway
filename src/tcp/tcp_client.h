#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace tcp {

class TcpClient {
public:
    TcpClient(
        std::string host,
        std::uint16_t port
    );

    ~TcpClient();

    TcpClient(const TcpClient&) = delete;
    TcpClient& operator=(const TcpClient&) = delete;

    bool connect_to_server();

    bool send_json(
        const std::string& json
    );

    void disconnect();

    bool is_connected() const;

private:
    bool send_all(
        const char* data,
        std::size_t length
    );

    std::string host_;
    std::uint16_t port_;
    int socket_fd_ = -1;
};

}  // namespace tcp
