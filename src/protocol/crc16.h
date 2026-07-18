#pragma once

#include <cstddef>
#include <cstdint>

namespace protocol {

uint16_t crc16_modbus(const uint8_t* data, std::size_t length);

}  // namespace protocol
