#include "device/serial_port.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

namespace device {

namespace {

speed_t to_speed(int baud_rate) {
    switch (baud_rate) {
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        default: return 0;
    }
}

}  // namespace

SerialPort::~SerialPort() {
    close_port();
}

bool SerialPort::open_port(
    const std::string& path,
    int baud_rate,
    std::string& error_message) {

    close_port();
    const speed_t speed = to_speed(baud_rate);
    if (speed == 0) {
        error_message = "unsupported baud rate";
        return false;
    }

    fd_ = ::open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        error_message = std::strerror(errno);
        return false;
    }

    termios tty{};
    if (tcgetattr(fd_, &tty) != 0) {
        error_message = std::strerror(errno);
        close_port();
        return false;
    }

    cfmakeraw(&tty);
    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);
    tty.c_cflag |= CLOCAL | CREAD;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        error_message = std::strerror(errno);
        close_port();
        return false;
    }

    error_message.clear();
    return true;
}

void SerialPort::close_port() noexcept {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool SerialPort::is_open() const noexcept {
    return fd_ >= 0;
}

bool SerialPort::discard_input(std::string& error_message) {
    if (fd_ < 0 || tcflush(fd_, TCIFLUSH) != 0) {
        error_message = fd_ < 0 ? "serial port is closed" : std::strerror(errno);
        return false;
    }
    return true;
}

bool SerialPort::write_all(
    const std::uint8_t* data,
    std::size_t length,
    std::string& error_message) {

    std::size_t written = 0;
    while (written < length) {
        const ssize_t result = ::write(fd_, data + written, length - written);
        if (result > 0) {
            written += static_cast<std::size_t>(result);
            continue;
        }
        if (result < 0 && (errno == EINTR || errno == EAGAIN)) {
            continue;
        }
        error_message = result == 0 ? "serial write returned zero" : std::strerror(errno);
        return false;
    }
    return true;
}

int SerialPort::read_some(
    std::uint8_t* data,
    std::size_t capacity,
    std::chrono::milliseconds timeout,
    std::string& error_message) {

    pollfd descriptor{fd_, POLLIN, 0};
    int ready;
    do {
        ready = poll(&descriptor, 1, static_cast<int>(timeout.count()));
    } while (ready < 0 && errno == EINTR);

    if (ready == 0) {
        return 0;
    }
    if (ready < 0 || (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL))) {
        error_message = ready < 0 ? std::strerror(errno) : "serial connection lost";
        return -1;
    }

    const ssize_t received = ::read(fd_, data, capacity);
    if (received < 0 && (errno == EINTR || errno == EAGAIN)) {
        return 0;
    }
    if (received < 0) {
        error_message = std::strerror(errno);
        return -1;
    }
    return static_cast<int>(received);
}

}  // namespace device
