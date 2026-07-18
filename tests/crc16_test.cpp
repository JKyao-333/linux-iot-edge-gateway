#include "protocol/crc16.h"

#include <cstdint>
#include <iomanip>
#include <iostream>

int main() {
    const uint8_t data[] = {
        0x04,
        0x01,
        0x10,
        0x11, 0x22, 0x33, 0x44
    };

    uint16_t crc = protocol::crc16_modbus(data, sizeof(data));

    std::cout << "CRC16: 0x"
              << std::uppercase
              << std::hex
              << std::setw(4)
              << std::setfill('0')
              << crc
              << std::endl;

    std::cout << "CRC low byte: 0x"
              << std::setw(2)
              << static_cast<int>(crc & 0xFF)
              << std::endl;

    std::cout << "CRC high byte: 0x"
              << std::setw(2)
              << static_cast<int>((crc >> 8) & 0xFF)
              << std::endl;

    return 0;
}
