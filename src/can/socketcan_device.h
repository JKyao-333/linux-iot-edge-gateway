#pragma once

#include "can/can_interface.h"

#include <chrono>
#include <string>

namespace canbus {

class SocketCanDevice {
public:
    explicit SocketCanDevice(std::string interface_name);
    ~SocketCanDevice();

    SocketCanDevice(const SocketCanDevice&) = delete;
    SocketCanDevice& operator=(const SocketCanDevice&) = delete;

    bool open_device(std::string& error_message);
    void close_device() noexcept;
    int read_frame(
        CanFrame& frame,
        std::chrono::milliseconds timeout,
        std::string& error_message
    );

private:
    std::string interface_name_;
    int socket_fd_ = -1;
};

}  // namespace canbus
