#include "protocol/frame_parser.h"

#include <cstdint>
#include <iostream>
#include <vector>

namespace {

const std::vector<std::uint8_t> kFrame = {
    0xAA, 0x55, 0x0B, 0x01, 0x10,
    0x00, 0xFD, 0x02, 0x60, 0x01, 0x2C,
    0x0E, 0x74, 0x00, 0x00, 0x01,
    0x4D, 0x58
};

}  // namespace

int main() {
    protocol::FrameParser parser;

    const std::vector<std::uint8_t> noise(
        parser.buffer_capacity() * 3,
        0x7E
    );

    if (!parser.feed(noise.data(), noise.size()).empty()) {
        std::cerr << "noise unexpectedly produced a frame" << std::endl;
        return 1;
    }

    for (std::size_t index = 0; index < 14; ++index) {
        const auto warmup_frames = parser.feed(
            kFrame.data(),
            kFrame.size()
        );

        if (warmup_frames.size() != 1) {
            std::cerr << "ring position warmup failed" << std::endl;
            return 1;
        }
    }

    const std::size_t split = 6;
    const auto first_frames = parser.feed(kFrame.data(), split);

    if (!first_frames.empty()
        || parser.buffered_byte_count() != split) {

        std::cerr << "partial frame was not retained" << std::endl;
        return 1;
    }

    const auto second_frames = parser.feed(
        kFrame.data() + split,
        kFrame.size() - split
    );

    if (second_frames.size() != 1
        || parser.buffered_byte_count() != 0) {

        std::cerr << "wrapped partial frame was not parsed" << std::endl;
        return 1;
    }

    std::vector<std::uint8_t> sticky_frames;
    sticky_frames.insert(
        sticky_frames.end(),
        kFrame.begin(),
        kFrame.end()
    );
    sticky_frames.insert(
        sticky_frames.end(),
        kFrame.begin(),
        kFrame.end()
    );

    const auto parsed_sticky = parser.feed(
        sticky_frames.data(),
        sticky_frames.size()
    );

    if (parsed_sticky.size() != 2
        || parser.take_overflow_byte_count() != 0) {

        std::cerr << "sticky frame or overflow check failed" << std::endl;
        return 1;
    }

    std::cout
        << "ring-backed parser test passed, capacity="
        << parser.buffer_capacity()
        << std::endl;

    return 0;
}
