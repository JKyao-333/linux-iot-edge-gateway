#include "can/socketcan_device.h"

#ifdef __linux__
#include <cerrno>
#include <cstring>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace canbus {

SocketCanDevice::SocketCanDevice(std::string interface_name)
    : interface_name_(std::move(interface_name)) {}

SocketCanDevice::~SocketCanDevice() {
    close_device();
}

bool SocketCanDevice::open_device(std::string& error_message) {
#ifdef __linux__
    close_device();
    socket_fd_ = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (socket_fd_ < 0) {
        error_message = std::strerror(errno);
        return false;
    }

    ifreq request{};
    std::strncpy(request.ifr_name, interface_name_.c_str(), IFNAMSIZ - 1);
    if (ioctl(socket_fd_, SIOCGIFINDEX, &request) < 0) {
        error_message = std::strerror(errno);
        close_device();
        return false;
    }

    sockaddr_can address{};
    address.can_family = AF_CAN;
    address.can_ifindex = request.ifr_ifindex;
    if (bind(
            socket_fd_,
            reinterpret_cast<sockaddr*>(&address),
            sizeof(address)) < 0) {
        error_message = std::strerror(errno);
        close_device();
        return false;
    }

    error_message.clear();
    return true;
#else
    error_message = "SocketCAN is only available on Linux";
    return false;
#endif
}

void SocketCanDevice::close_device() noexcept {
#ifdef __linux__
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
#endif
}

int SocketCanDevice::read_frame(
    CanFrame& output,
    std::chrono::milliseconds timeout,
    std::string& error_message) {
#ifdef __linux__
    pollfd descriptor{socket_fd_, POLLIN, 0};
    int ready;
    do {
        ready = poll(&descriptor, 1, static_cast<int>(timeout.count()));
    } while (ready < 0 && errno == EINTR);

    if (ready == 0) {
        return 0;
    }
    if (ready < 0 || (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL))) {
        error_message = ready < 0 ? std::strerror(errno) : "SocketCAN connection lost";
        return -1;
    }

    can_frame frame{};
    const ssize_t received = recv(socket_fd_, &frame, sizeof(frame), 0);
    if (received != static_cast<ssize_t>(sizeof(frame))) {
        error_message = received < 0 ? std::strerror(errno) : "short CAN frame";
        return -1;
    }
    if ((frame.can_id & (CAN_RTR_FLAG | CAN_ERR_FLAG)) != 0) {
        error_message = "CAN RTR or error frame is not sensor data";
        return 2;
    }

    output.extended = (frame.can_id & CAN_EFF_FLAG) != 0;
    output.id = output.extended
        ? frame.can_id & CAN_EFF_MASK
        : frame.can_id & CAN_SFF_MASK;
    output.dlc = frame.can_dlc;
    for (std::size_t index = 0; index < 8; ++index) {
        output.data[index] = frame.data[index];
    }
    return 1;
#else
    (void) output;
    (void) timeout;
    error_message = "SocketCAN is only available on Linux";
    return -1;
#endif
}

}  // namespace canbus
