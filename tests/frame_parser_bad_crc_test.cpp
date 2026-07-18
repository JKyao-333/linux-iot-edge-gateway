#include "protocol/frame_parser.h"

#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    const std::vector<uint8_t> bad_crc_frame = {
        0xAA, 0x55,
        0x04,
        0x01,
        0x10,
        0x11, 0x22, 0x33, 0x44,
        0x00, 0x00
    };

    const std::vector<uint8_t> good_frame = {
        0xAA, 0x55,
        0x04,
        0x01,
        0x10,
        0x11, 0x22, 0x33, 0x44,
        0x2F, 0x27
    };

    std::vector<uint8_t> stream;
    stream.insert(stream.end(), bad_crc_frame.begin(), bad_crc_frame.end());
    stream.insert(stream.end(), good_frame.begin(), good_frame.end());

    protocol::FrameParser parser;
    auto frames = parser.feed(stream.data(), stream.size());

    std::cout << "parsed frame count: " << frames.size() << std::endl;

    if (!frames.empty()) {
        std::cout << "cmd=0x" << std::hex << static_cast<int>(frames[0].cmd)
                  << " device_id=0x" << static_cast<int>(frames[0].device_id)
                  << " payload_len=" << std::dec << frames[0].payload.size()
                  << std::endl;
    }

    return frames.size() == 1 ? 0 : 1;
}
