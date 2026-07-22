#pragma once

#include "app/sensor_data.h"

#include <cstdint>
#include <string>

namespace device {

enum class ProtocolType {
    Uart,
    ModbusRtu,
    SocketCan
};

const char* to_string(ProtocolType protocol) noexcept;

struct DeviceData {
    std::uint8_t device_id = 0;
    ProtocolType protocol = ProtocolType::Uart;
    double temperature_c = 0.0;
    double humidity_percent = 0.0;
    std::uint16_t gas_ppm = 0;
    std::uint16_t battery_mv = 0;
    std::uint8_t status = 0;
    std::uint16_t sequence = 0;
};

app::SensorData to_sensor_data(const DeviceData& input);

}  // namespace device
