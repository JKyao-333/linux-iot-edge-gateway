#include "protocol/frame_parser.h"

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

int main() {
    const std::vector<uint8_t> raw_frame = {
        0xAA, 0x55,
        0x04,
        0x01,
        0x10,
        0x11, 0x22, 0x33, 0x44,
        0x2F, 0x27
    };

    protocol::FrameParser parser;
    auto frame = parser.parse_complete_frame(raw_frame);

    if (!frame.has_value()) {
        std::cerr << "parse failed" << std::endl;
        return 1;
    }

    std::cout << "parse ok" << std::endl;

    std::cout << "cmd: 0x"
              << std::uppercase << std::hex << std::setw(2)
              << std::setfill('0') << static_cast<int>(frame->cmd)
              << std::endl;

    std::cout << "device_id: 0x"
              << std::setw(2)
              << static_cast<int>(frame->device_id)
              << std::endl;

    std::cout << "payload:";
    for (uint8_t byte : frame->payload) {
        std::cout << " 0x"
                  << std::setw(2)
                  << static_cast<int>(byte);
    }
    std::cout << std::endl;

    std::cout << "crc: 0x"
              << std::setw(4)
              << frame->crc
              << std::endl;

    return 0;
}
