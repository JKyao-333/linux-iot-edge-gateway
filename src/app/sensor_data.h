#pragma once
#include "protocol/frame.h"
#include <cstdint>
#include <string>
#include <vector>

namespace app {

struct SensorData {
    uint8_t device_id = 0;
    uint8_t cmd = 0;

    double temperature_c = 0.0;
    double humidity_percent = 0.0;
    uint16_t gas_ppm = 0;
    uint16_t battery_mv = 0;
    uint8_t status = 0;
    uint16_t sequence = 0;

    bool valid = true;
    std::vector<std::string> warnings;
};
bool parse_sensor_data(const protocol::Frame& frame, SensorData& out);
std::string sensor_data_to_json(const SensorData& data);
}  // namespace app
