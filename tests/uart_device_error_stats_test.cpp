#include "device/uart_device.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

int main() {
    const int master = posix_openpt(O_RDWR | O_NOCTTY);
    if (master < 0 || grantpt(master) != 0 || unlockpt(master) != 0) {
        std::cerr << "unable to create pseudo terminal" << std::endl;
        return 1;
    }
    const char* slave_name = ptsname(master);
    if (slave_name == nullptr) {
        close(master);
        return 1;
    }

    device::UartDevice input(slave_name, 115200);
    std::string error;
    if (!input.start(error)) {
        std::cerr << error << std::endl;
        close(master);
        return 1;
    }

    const std::array<std::uint8_t, 18> valid = {
        0xAA, 0x55, 0x0B, 0x01, 0x10, 0x00, 0xFD,
        0x02, 0x60, 0x01, 0x2C, 0x0E, 0x74, 0x00,
        0x00, 0x01, 0x4D, 0x58
    };
    auto bad_crc = valid;
    bad_crc.back() ^= 0xFF;
    const std::array<std::uint8_t, 3> bad_length = {0xAA, 0x55, 0xFF};

    std::vector<std::uint8_t> stream;
    stream.insert(stream.end(), bad_crc.begin(), bad_crc.end());
    stream.insert(stream.end(), bad_length.begin(), bad_length.end());
    stream.insert(stream.end(), valid.begin(), valid.end());
    if (write(master, stream.data(), stream.size())
        != static_cast<ssize_t>(stream.size())) {
        input.stop();
        close(master);
        return 1;
    }

    const auto result = input.read(std::chrono::milliseconds(500));
    input.stop();
    close(master);
    if (result.code != device::ReadCode::Data
        || !result.data.has_value()
        || result.error_stats.crc_errors != 1
        || result.error_stats.length_errors != 1
        || result.error_stats.total() != 2) {
        std::cerr << "UART protocol statistics were not preserved" << std::endl;
        return 1;
    }

    std::cout << "UART error statistics test passed" << std::endl;
    return 0;
}
