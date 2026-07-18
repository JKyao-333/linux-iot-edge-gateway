#include "app/sensor_data.h"
#include "protocol/frame.h"

#include <cstdint>
#include <iostream>

int main() {
    protocol::Frame frame;
    frame.cmd = 0x01;
    frame.device_id = 0x10;
    frame.payload = {
        0x00, 0xFD,
        0x02, 0x60,
        0x01, 0x2C,
        0x0E, 0x74,
        0x00,
        0x00, 0x01
    };

    app::SensorData data;
    bool ok = app::parse_sensor_data(frame, data);

    std::cout << "parse ok: " << (ok ? "true" : "false") << std::endl;
    std::cout << "temperature_c: " << data.temperature_c << std::endl;
    std::cout << "humidity_percent: " << data.humidity_percent << std::endl;
    std::cout << "gas_ppm: " << data.gas_ppm << std::endl;
    std::cout << "battery_mv: " << data.battery_mv << std::endl;
    std::cout << "status: " << static_cast<int>(data.status) << std::endl;
    std::cout << "sequence: " << data.sequence << std::endl;

    return ok ? 0 : 1;
}
