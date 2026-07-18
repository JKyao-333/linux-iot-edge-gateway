#include "app/sensor_data.h"

#include <iomanip>
#include <iostream>

int main() {
    app::SensorData data;
    data.device_id = 0x10;
    data.cmd = 0x01;
    data.temperature_c = 25.3;
    data.humidity_percent = 60.8;
    data.gas_ppm = 300;
    data.battery_mv = 3700;
    data.status = 0x00;
    data.sequence = 1;
    data.valid = true;

    std::cout << std::fixed << std::setprecision(1);

    std::cout << "device_id: 0x"
              << std::hex << std::uppercase << static_cast<int>(data.device_id)
              << std::endl;

    std::cout << "cmd: 0x"
              << std::hex << std::uppercase << static_cast<int>(data.cmd)
              << std::endl;

    std::cout << std::dec;
    std::cout << "temperature_c: " << data.temperature_c << std::endl;
    std::cout << "humidity_percent: " << data.humidity_percent << std::endl;
    std::cout << "gas_ppm: " << data.gas_ppm << std::endl;
    std::cout << "battery_mv: " << data.battery_mv << std::endl;
    std::cout << "status: 0x"
              << std::hex << std::uppercase << static_cast<int>(data.status)
              << std::endl;
    std::cout << std::dec;
    std::cout << "sequence: " << data.sequence << std::endl;
    std::cout << "valid: " << (data.valid ? "true" : "false") << std::endl;

    return 0;
}
