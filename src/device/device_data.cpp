#include "device/device_data.h"

namespace device {

const char* to_string(ProtocolType protocol) noexcept {
    switch (protocol) {
        case ProtocolType::Uart:
            return "UART";
        case ProtocolType::ModbusRtu:
            return "MODBUS_RTU";
        case ProtocolType::SocketCan:
            return "SOCKETCAN";
    }

    return "UNKNOWN";
}

app::SensorData to_sensor_data(const DeviceData& input) {
    app::SensorData output;
    output.device_id = input.device_id;
    output.cmd = 0x01;
    output.temperature_c = input.temperature_c;
    output.humidity_percent = input.humidity_percent;
    output.gas_ppm = input.gas_ppm;
    output.battery_mv = input.battery_mv;
    output.status = input.status;
    output.sequence = input.sequence;
    app::validate_sensor_data(output);
    return output;
}

}  // namespace device
