#include "protocol/frame_parser.h"

#include "protocol/crc16.h"

#include <utility>

namespace protocol {

namespace {

constexpr std::uint8_t kHeader1 = 0xAA;
constexpr std::uint8_t kHeader2 = 0x55;
constexpr std::uint8_t kMaxPayloadLength = 64;
constexpr std::size_t kFixedFieldSize = 2 + 1 + 1 + 1 + 2;
constexpr std::size_t kMinFrameSize = kFixedFieldSize;

}  // namespace

std::optional<Frame> FrameParser::parse_complete_frame(
    const std::vector<std::uint8_t>& data) const {

    if (data.size() < kMinFrameSize) {
        return std::nullopt;
    }

    if (data[0] != kHeader1 || data[1] != kHeader2) {
        return std::nullopt;
    }

    const std::uint8_t payload_length = data[2];
    if (payload_length > kMaxPayloadLength) {
        return std::nullopt;
    }

    const std::size_t expected_size =
        kFixedFieldSize + payload_length;

    if (data.size() != expected_size) {
        return std::nullopt;
    }

    Frame frame;
    frame.cmd = data[3];
    frame.device_id = data[4];

    const std::size_t payload_start = 5;
    const std::size_t payload_end =
        payload_start + payload_length;

    frame.payload.assign(
        data.begin() + payload_start,
        data.begin() + payload_end
    );

    const std::uint8_t crc_low = data[payload_end];
    const std::uint8_t crc_high = data[payload_end + 1];
    frame.crc = static_cast<std::uint16_t>(crc_low)
        | static_cast<std::uint16_t>(crc_high) << 8;

    const std::uint16_t calculated_crc = crc16_modbus(
        &data[2],
        1 + 1 + 1 + payload_length
    );

    if (calculated_crc != frame.crc) {
        return std::nullopt;
    }

    return frame;
}

std::vector<Frame> FrameParser::parse_stream(
    const std::vector<std::uint8_t>& data) const {

    FrameParser parser;
    return parser.feed(data.data(), data.size());
}

std::vector<Frame> FrameParser::feed(
    const std::uint8_t* data,
    std::size_t length) {

    std::vector<Frame> frames;

    for (std::size_t index = 0; index < length; ++index) {
        if (!buffer_.push(data[index])) {
            buffer_.consume(1);
            ++overflow_byte_count_;
            buffer_.push(data[index]);
        }

        drain_buffer(frames);
    }

    return frames;
}

void FrameParser::drain_buffer(std::vector<Frame>& frames) {
    while (!buffer_.empty()) {
        if (buffer_.at(0) != kHeader1) {
            buffer_.consume(1);
            continue;
        }

        if (buffer_.size() < 2) {
            return;
        }

        if (buffer_.at(1) != kHeader2) {
            buffer_.consume(1);
            continue;
        }

        if (buffer_.size() < 3) {
            return;
        }

        const std::uint8_t payload_length = buffer_.at(2);
        if (payload_length > kMaxPayloadLength) {
            ++length_error_count_;
            buffer_.consume(1);
            continue;
        }

        const std::size_t frame_size =
            kFixedFieldSize + payload_length;

        if (buffer_.size() < frame_size) {
            return;
        }

        const std::vector<std::uint8_t> raw_frame =
            buffer_.copy(0, frame_size);

        auto frame = parse_complete_frame(raw_frame);
        if (frame.has_value()) {
            frames.push_back(std::move(*frame));
            buffer_.consume(frame_size);
            continue;
        }

        ++crc_error_count_;
        buffer_.consume(1);
    }
}

std::size_t FrameParser::take_crc_error_count() {
    const std::size_t count = crc_error_count_;
    crc_error_count_ = 0;
    return count;
}

std::size_t FrameParser::take_length_error_count() {
    const std::size_t count = length_error_count_;
    length_error_count_ = 0;
    return count;
}

std::size_t FrameParser::take_overflow_byte_count() {
    const std::size_t count = overflow_byte_count_;
    overflow_byte_count_ = 0;
    return count;
}

std::size_t FrameParser::buffered_byte_count() const noexcept {
    return buffer_.size();
}

std::size_t FrameParser::buffer_capacity() const noexcept {
    return buffer_.capacity();
}

void FrameParser::reset() {
    buffer_.clear();
    crc_error_count_ = 0;
    length_error_count_ = 0;
    overflow_byte_count_ = 0;
}

}  // namespace protocol
