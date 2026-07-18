#include "protocol/frame_parser.h"

#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    const std::vector<uint8_t> part1 = {
        0xAA, 0x55, 0x04, 0x01
    };

    const std::vector<uint8_t> part2 = {
        0x10,
        0x11, 0x22, 0x33, 0x44,
        0x2F, 0x27
    };

    protocol::FrameParser parser;

    auto frames1 = parser.feed(part1.data(), part1.size());
    std::cout << "after part1 frame count: " << frames1.size() << std::endl;

    auto frames2 = parser.feed(part2.data(), part2.size());
    std::cout << "after part2 frame count: " << frames2.size() << std::endl;

    if (!frames2.empty()) {
        std::cout << "cmd=0x" << std::hex << static_cast<int>(frames2[0].cmd)
                  << " device_id=0x" << static_cast<int>(frames2[0].device_id)
                  << " payload_len=" << std::dec << frames2[0].payload.size()
                  << std::endl;
    }

    return frames1.empty() && frames2.size() == 1 ? 0 : 1;
}
