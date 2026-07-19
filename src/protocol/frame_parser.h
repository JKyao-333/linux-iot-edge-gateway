#pragma once

#include "protocol/byte_ring_buffer.h"
#include "protocol/frame.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace protocol {

class FrameParser {
public:
    std::optional<Frame> parse_complete_frame(const std::vector<uint8_t>& data) const;

    std::vector<Frame> parse_stream(const std::vector<uint8_t>& data) const;

    std::vector<Frame> feed(const uint8_t* data, std::size_t length);
    void reset();
    std::size_t take_crc_error_count();
    std::size_t take_length_error_count();
    std::size_t take_overflow_byte_count();
    std::size_t buffered_byte_count() const noexcept;
    std::size_t buffer_capacity() const noexcept;

private:
    void drain_buffer(std::vector<Frame>& frames);

    static constexpr std::size_t kBufferCapacity = 256;
    ByteRingBuffer<kBufferCapacity> buffer_;
    std::size_t crc_error_count_ = 0;
    std::size_t length_error_count_ = 0;
    std::size_t overflow_byte_count_ = 0;
};


}  // namespace protocol
