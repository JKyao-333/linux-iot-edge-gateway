#pragma once

#include <cstdint>

namespace canbus {

struct CanFrame {
    std::uint32_t id = 0;
    bool extended = false;
    std::uint8_t dlc = 0;
    std::uint8_t data[8]{};
};

}  // namespace canbus
