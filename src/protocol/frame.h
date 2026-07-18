#pragma once

#include <cstdint>
#include <vector>

namespace protocol {

struct Frame {
    uint8_t cmd = 0;
    uint8_t device_id = 0;
    std::vector<uint8_t> payload;
    uint16_t crc = 0;
};

}  // namespace protocol
