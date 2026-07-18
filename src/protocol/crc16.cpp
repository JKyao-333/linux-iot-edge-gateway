#include "protocol/crc16.h"

namespace protocol {

uint16_t crc16_modbus(const uint8_t* data, std::size_t length) {
    uint16_t crc = 0xFFFF;

    for (std::size_t i = 0; i < length; ++i) {
        crc ^= data[i];

        for (int bit = 0; bit < 8; ++bit) {
            if ((crc & 0x0001) != 0) {
                crc = static_cast<uint16_t>((crc >> 1) ^ 0xA001);
            } else {
                crc = static_cast<uint16_t>(crc >> 1);
            }
        }
    }

    return crc;
}

}  // namespace protocol
