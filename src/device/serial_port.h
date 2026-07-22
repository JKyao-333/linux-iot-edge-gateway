#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

namespace device {

class SerialPort {
public:
    SerialPort() = default;
    ~SerialPort();

    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    bool open_port(
        const std::string& path,
        int baud_rate,
        std::string& error_message
    );
    void close_port() noexcept;
    bool is_open() const noexcept;
    bool discard_input(std::string& error_message);
    bool write_all(
        const std::uint8_t* data,
        std::size_t length,
        std::string& error_message
    );
    int read_some(
        std::uint8_t* data,
        std::size_t capacity,
        std::chrono::milliseconds timeout,
        std::string& error_message
    );

private:
    int fd_ = -1;
};

}  // namespace device
