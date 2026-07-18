#include "protocol/frame.h"

#include <cstdint>
#include <iomanip>
#include <iostream>

int main() {
    protocol::Frame frame;
    frame.cmd = 0x01;
    frame.device_id = 0x10;
    frame.payload = {0x11, 0x22, 0x33, 0x44};
    frame.crc = 0x272F;

    std::cout << "cmd: 0x"
              << std::uppercase << std::hex << std::setw(2)
              << std::setfill('0') << static_cast<int>(frame.cmd)
              << std::endl;

    std::cout << "device_id: 0x"
              << std::setw(2)
              << static_cast<int>(frame.device_id)
              << std::endl;

    std::cout << "payload:";
    for (uint8_t byte : frame.payload) {
        std::cout << " 0x"
                  << std::setw(2)
                  << static_cast<int>(byte);
    }
    std::cout << std::endl;

    std::cout << "crc: 0x"
              << std::setw(4)
              << frame.crc
              << std::endl;

    return 0;
}
