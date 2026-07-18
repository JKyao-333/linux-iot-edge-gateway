#pragma once

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
private:
    std::vector<uint8_t> buffer_;
    std::size_t crc_error_count_ = 0;
    std::size_t length_error_count_ = 0;
};


}  // namespace protocol
