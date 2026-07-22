#include "app/sensor_data.h"
#include "can/can_parser.h"

#include <iostream>
#include <string>

int main() {
    canbus::CanFrame frame;
    frame.id = 0x123;
    frame.dlc = 8;
    const std::uint8_t payload[8] = {
        0x00, 0xFD,
        0x02, 0x60,
        0x01, 0x2C,
        0x25,
        0x00
    };
    for (std::size_t index = 0; index < 8; ++index) {
        frame.data[index] = payload[index];
    }

    device::DeviceData device_data;
    std::string error;
    if (!canbus::parse_sensor_frame(frame, device_data, error)) {
        std::cerr << error << std::endl;
        return 1;
    }

    const app::SensorData sensor_data =
        device::to_sensor_data(device_data);
    if (!sensor_data.valid
        || sensor_data.device_id != 0x23
        || sensor_data.temperature_c != 25.3
        || sensor_data.humidity_percent != 60.8
        || sensor_data.gas_ppm != 300
        || sensor_data.battery_mv != 3700) {

        std::cerr << "CAN data conversion failed" << std::endl;
        return 1;
    }

    frame.id = 0x124;
    if (!canbus::parse_sensor_frame(frame, device_data, error)
        || device_data.device_id != 0x24) {

        std::cerr << "CAN identifier mapping failed" << std::endl;
        return 1;
    }

    frame.dlc = 7;
    if (canbus::parse_sensor_frame(frame, device_data, error)) {
        std::cerr << "invalid DLC was accepted" << std::endl;
        return 1;
    }

    frame.dlc = 8;
    frame.extended = true;
    frame.id = 0x123;
    if (canbus::parse_sensor_frame(frame, device_data, error)) {
        std::cerr << "extended CAN identifier was accepted" << std::endl;
        return 1;
    }

    frame.extended = false;
    for (std::uint32_t index = 0; index < 10000; ++index) {
        frame.id = 0x100U + (index % 0x6FFU);
        if (!canbus::parse_sensor_frame(frame, device_data, error)) {
            std::cerr << "CAN continuous-frame parsing failed" << std::endl;
            return 1;
        }
    }

    std::cout << "CAN parser test passed" << std::endl;
    return 0;
}
