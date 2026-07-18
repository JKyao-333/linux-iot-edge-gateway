#include "protocol/frame_parser.h"

#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    const std::vector<uint8_t> bad_length_prefix = {
        0xAA, 0x55,
        0xFF,
        0x01,
        0x10
    };

    const std::vector<uint8_t> good_frame = {
        0xAA, 0x55,
        0x0B,
        0x01,
        0x10,
        0x00, 0xFD,
        0x02, 0x60,
        0x01, 0x2C,
        0x0E, 0x74,
        0x00,
        0x00, 0x01,
        0x4D, 0x58
    };

    std::vector<uint8_t> stream;
    stream.insert(
        stream.end(),
        bad_length_prefix.begin(),
        bad_length_prefix.end()
    );

    stream.insert(
        stream.end(),
        good_frame.begin(),
        good_frame.end()
    );

    protocol::FrameParser parser;

    const auto frames = parser.feed(
        stream.data(),
        stream.size()
    );

    const std::size_t length_errors =
        parser.take_length_error_count();

    std::cout << "parsed frame count: "
              << frames.size() << std::endl;

    std::cout << "length error count: "
              << length_errors << std::endl;

    return frames.size() == 1
        && length_errors == 1
        ? 0
        : 1;
}
