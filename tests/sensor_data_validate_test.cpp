#include "app/sensor_data.h"
#include "protocol/frame.h"

#include <iostream>

static void print_result(const app::SensorData& data) {
    std::cout << "valid: " << (data.valid ? "true" : "false") << std::endl;

    for (const auto& warning : data.warnings) {
        std::cout << "warning: " << warning << std::endl;
    }
}

int main() {
    protocol::Frame good_frame;
    good_frame.cmd = 0x01;
    good_frame.device_id = 0x10;
    good_frame.payload = {
        0x00, 0xFD,
        0x02, 0x60,
        0x01, 0x2C,
        0x0E, 0x74,
        0x00,
        0x00, 0x01
    };

    app::SensorData good_data;
    app::parse_sensor_data(good_frame, good_data);

    std::cout << "good frame:" << std::endl;
    print_result(good_data);

    protocol::Frame bad_frame;
    bad_frame.cmd = 0x01;
    bad_frame.device_id = 0x10;
    bad_frame.payload = {
        0x03, 0xE8,
        0x04, 0xB0,
        0x17, 0x70,
        0x09, 0xC4,
        0x07,
        0x00, 0x02
    };

    app::SensorData bad_data;
    app::parse_sensor_data(bad_frame, bad_data);

    std::cout << "bad frame:" << std::endl;
    print_result(bad_data);

    return good_data.valid && !bad_data.valid ? 0 : 1;
}
