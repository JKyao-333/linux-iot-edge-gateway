#include "protocol/frame_parser.h"

#include "protocol/crc16.h"

namespace protocol {

namespace {

constexpr uint8_t kHeader1 = 0xAA;
constexpr uint8_t kHeader2 = 0x55;
constexpr uint8_t kMaxPayloadLength = 64;
constexpr std::size_t kMinFrameSize = 2 + 1 + 1 + 1 + 2;

}  // namespace

std::optional<Frame> FrameParser::parse_complete_frame(const std::vector<uint8_t>& data) const {
    if (data.size() < kMinFrameSize) {
        return std::nullopt;
    }

    if (data[0] != kHeader1 || data[1] != kHeader2) {
        return std::nullopt;
    }

    const uint8_t payload_length = data[2];
    if (payload_length > kMaxPayloadLength) {
    return std::nullopt;
}
const std::size_t expected_size = 2 + 1 + 1 + 1 + payload_length + 2;

    if (data.size() != expected_size) {
        return std::nullopt;
    }

    Frame frame;
    frame.cmd = data[3];
    frame.device_id = data[4];

    const std::size_t payload_start = 5;
    const std::size_t payload_end = payload_start + payload_length;

    frame.payload.assign(data.begin() + payload_start, data.begin() + payload_end);

    const uint8_t crc_low = data[payload_end];
    const uint8_t crc_high = data[payload_end + 1];
    frame.crc = static_cast<uint16_t>(crc_low)
              | static_cast<uint16_t>(crc_high) << 8;

    const uint16_t calculated_crc = crc16_modbus(&data[2], 1 + 1 + 1 + payload_length);
    if (calculated_crc != frame.crc) {
        return std::nullopt;
    }

    return frame;
}

std::vector<Frame> FrameParser::parse_stream(const std::vector<uint8_t>& data) const {
    std::vector<Frame> frames;

    std::size_t index = 0;

    while (index + kMinFrameSize <= data.size()) {
        if (data[index] != kHeader1) {
            ++index;
            continue;
        }

        if (index + 1 >= data.size()) {
            break;
        }

        if (data[index + 1] != kHeader2) {
            ++index;
            continue;
        }

        if (index + 2 >= data.size()) {
            break;
        }

        const uint8_t payload_length = data[index + 2];
        if (payload_length > kMaxPayloadLength) {
    ++index;
    continue;
}
	const std::size_t frame_size = 2 + 1 + 1 + 1 + payload_length + 2;

        if (index + frame_size > data.size()) {
            break;
        }

        std::vector<uint8_t> one_frame(
            data.begin() + index,
            data.begin() + index + frame_size
        );

        auto frame = parse_complete_frame(one_frame);
        if (frame.has_value()) {
            frames.push_back(*frame);
            index += frame_size;
        } else {
            ++index;
        }
    }

    return frames;
}
std::vector<Frame> FrameParser::feed(const uint8_t* data, std::size_t length) {
    buffer_.insert(buffer_.end(), data, data + length);

    std::vector<Frame> frames;
    std::size_t index = 0;

    while (index + kMinFrameSize <= buffer_.size()) {
        if (buffer_[index] != kHeader1) {
            ++index;
            continue;
        }

        if (index + 1 >= buffer_.size()) {
            break;
        }

        if (buffer_[index + 1] != kHeader2) {
            ++index;
            continue;
        }

        if (index + 2 >= buffer_.size()) {
            break;
        }

        const uint8_t payload_length = buffer_[index + 2];
        if (payload_length > kMaxPayloadLength) {
    ++length_error_count_;
    ++index;
    continue;
}
	const std::size_t frame_size = 2 + 1 + 1 + 1 + payload_length + 2;

        if (index + frame_size > buffer_.size()) {
            break;
        }

        std::vector<uint8_t> one_frame(
            buffer_.begin() + index,
            buffer_.begin() + index + frame_size
        );

        auto frame = parse_complete_frame(one_frame);
        if (frame.has_value()) {
            frames.push_back(*frame);
            index += frame_size;
        } else {
	    ++crc_error_count_;
            ++index;
        }
    }

    buffer_.erase(buffer_.begin(), buffer_.begin() + index);

    return frames;

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
void FrameParser::reset() {
    buffer_.clear();
    crc_error_count_ = 0;
    length_error_count_ = 0;
}
}  // namespace protocol
