#include "protocol/frame_parser.h"

#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    const std::vector<uint8_t> one_frame = {
        0xAA, 0x55,
        0x04,
        0x01,
        0x10,
        0x11, 0x22, 0x33, 0x44,
        0x2F, 0x27
    };

    std::vector<uint8_t> stream;
    stream.insert(stream.end(), one_frame.begin(), one_frame.end());
    stream.insert(stream.end(), one_frame.begin(), one_frame.end());

    protocol::FrameParser parser;
    auto frames = parser.parse_stream(stream);

    std::cout << "parsed frame count: " << frames.size() << std::endl;

    for (std::size_t i = 0; i < frames.size(); ++i) {
        std::cout << "frame " << i + 1
                  << " cmd=0x" << std::hex << static_cast<int>(frames[i].cmd)
                  << " device_id=0x" << static_cast<int>(frames[i].device_id)
                  << " payload_len=" << std::dec << frames[i].payload.size()
                  << std::endl;
    }

    return frames.size() == 2 ? 0 : 1;
}
