#include "protocol/frame_parser.h"

#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    const std::vector<uint8_t> bad_crc_frame = {
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
        0x00, 0x00
    };

    protocol::FrameParser parser;

    const auto frames = parser.feed(
        bad_crc_frame.data(),
        bad_crc_frame.size()
    );

    const std::size_t crc_errors =
        parser.take_crc_error_count();

    const std::size_t after_reset =
        parser.take_crc_error_count();

    std::cout << "parsed frame count: "
              << frames.size() << std::endl;

    std::cout << "crc error count: "
              << crc_errors << std::endl;

    std::cout << "after reset: "
              << after_reset << std::endl;

    return frames.empty()
        && crc_errors == 1
        && after_reset == 0
        ? 0
        : 1;
}
