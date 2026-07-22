#include "can/can_parser.h"

#include <cstdint>

namespace canbus {

namespace {

std::uint16_t read_u16_be(
    const std::uint8_t* data,
    std::size_t offset) {

    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(data[offset]) << 8
        | static_cast<std::uint16_t>(data[offset + 1])
    );
}

}  // namespace

bool parse_sensor_frame(
    const CanFrame& frame,
    device::DeviceData& output,
    std::string& error_message) {

    if (frame.extended || (frame.id & 0x1FFFFFFFU) > 0x7FFU) {
        error_message = "only standard 11-bit CAN identifiers are supported";
        return false;
    }

    if (frame.dlc != 8) {
        error_message = "CAN sensor frame DLC must be 8";
        return false;
    }

    output = device::DeviceData{};
    output.device_id = static_cast<std::uint8_t>(frame.id & 0xFFU);
    output.protocol = device::ProtocolType::SocketCan;
    output.temperature_c =
        static_cast<std::int16_t>(read_u16_be(frame.data, 0)) / 10.0;
    output.humidity_percent = read_u16_be(frame.data, 2) / 10.0;
    output.gas_ppm = read_u16_be(frame.data, 4);
    output.battery_mv =
        static_cast<std::uint16_t>(frame.data[6]) * 100;
    output.status = frame.data[7];
    output.sequence = 0;
    error_message.clear();
    return true;
}

}  // namespace canbus
