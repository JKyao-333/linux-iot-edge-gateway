#include "tcp/tcp_client.h"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

namespace tcp {

TcpClient::TcpClient(
    std::string host,
    std::uint16_t port)
    : host_(std::move(host)),
      port_(port) {}

TcpClient::~TcpClient() {
    disconnect();
}

bool TcpClient::connect_to_server() {
    if (is_connected()) {
        return true;
    }

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* addresses = nullptr;
    const std::string port_text =
        std::to_string(port_);

    const int result = getaddrinfo(
        host_.c_str(),
        port_text.c_str(),
        &hints,
        &addresses
    );

    if (result != 0) {
        std::cerr
            << "[ERROR] TCP address lookup failed: "
            << gai_strerror(result)
            << std::endl;

        return false;
    }

    for (
        addrinfo* current = addresses;
        current != nullptr;
        current = current->ai_next
    ) {
        const int candidate_fd = socket(
            current->ai_family,
            current->ai_socktype,
            current->ai_protocol
        );

        if (candidate_fd < 0) {
            continue;
        }

        if (connect(
                candidate_fd,
                current->ai_addr,
                current->ai_addrlen
            ) == 0) {

            socket_fd_ = candidate_fd;
            break;
        }

        close(candidate_fd);
    }

    freeaddrinfo(addresses);

    if (!is_connected()) {
        std::cerr
            << "[ERROR] TCP connection failed: "
            << host_ << ":" << port_
            << std::endl;

        return false;
    }

    std::cout
        << "[INFO] TCP connected: "
        << host_ << ":" << port_
        << std::endl;

    return true;
}

bool TcpClient::send_json(
    const std::string& json) {

    if (!is_connected()
        && !connect_to_server()) {

        return false;
    }

    const std::string message =
        json + "\n";

    if (!send_all(
            message.data(),
            message.size()
        )) {

        std::cerr
            << "[ERROR] TCP send failed: "
            << std::strerror(errno)
            << std::endl;

        disconnect();
        return false;
    }

    std::cout
        << "[INFO] TCP send ok, bytes="
        << message.size()
        << std::endl;

    return true;
}

void TcpClient::disconnect() {
    if (!is_connected()) {
        return;
    }

    close(socket_fd_);
    socket_fd_ = -1;

    std::cout
        << "[INFO] TCP disconnected"
        << std::endl;
}

bool TcpClient::is_connected() const {
    return socket_fd_ >= 0;
}

bool TcpClient::send_all(
    const char* data,
    std::size_t length) {

    std::size_t sent_bytes = 0;

    while (sent_bytes < length) {
        const ssize_t result = send(
            socket_fd_,
            data + sent_bytes,
            length - sent_bytes,
            MSG_NOSIGNAL
        );

        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }

            return false;
        }

        if (result == 0) {
            return false;
        }

        sent_bytes +=
            static_cast<std::size_t>(result);
    }

    return true;
}

}  // namespace tcp
