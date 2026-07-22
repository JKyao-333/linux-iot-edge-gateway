#include "app/sensor_data.h"
#include <iomanip>
#include <sstream>
#include "protocol/frame.h"

namespace app {

namespace {

constexpr std::size_t kSensorPayloadSize = 11;

uint16_t read_u16_be(const std::vector<uint8_t>& data, std::size_t offset) {
    return static_cast<uint16_t>((static_cast<uint16_t>(data[offset]) << 8)
                               | static_cast<uint16_t>(data[offset + 1]));
}

int16_t read_i16_be(const std::vector<uint8_t>& data, std::size_t offset) {
    return static_cast<int16_t>((static_cast<uint16_t>(data[offset]) << 8)
                              | static_cast<uint16_t>(data[offset + 1]));
}

void add_warning(SensorData& data, const std::string& warning) {
    data.valid = false;
    data.warnings.push_back(warning);
}

void validate_sensor_data_impl(SensorData& data) {
    if (data.temperature_c < -40.0 || data.temperature_c > 85.0) {
        add_warning(data, "temperature out of range");
    }

    if (data.humidity_percent < 0.0 || data.humidity_percent > 100.0) {
        add_warning(data, "humidity out of range");
    }

    if (data.gas_ppm > 5000) {
        add_warning(data, "gas concentration out of range");
    }

    if (data.battery_mv < 3000 || data.battery_mv > 4200) {
        add_warning(data, "battery voltage out of range");
    }

    if ((data.status & 0x01) != 0) {
        add_warning(data, "device reports sensor fault");
    }

    if ((data.status & 0x02) != 0) {
        add_warning(data, "device reports low battery");
    }

    if ((data.status & 0x04) != 0) {
        add_warning(data, "device is calibrating");
    }
}

}  // namespace

bool parse_sensor_data(const protocol::Frame& frame, SensorData& out) {
    out = SensorData{};
    out.device_id = frame.device_id;
    out.cmd = frame.cmd;

    if (frame.payload.size() != kSensorPayloadSize) {
        add_warning(out, "invalid payload length");
        return false;
    }

    const int16_t temperature_x10 = read_i16_be(frame.payload, 0);
    const uint16_t humidity_x10 = read_u16_be(frame.payload, 2);

    out.temperature_c = temperature_x10 / 10.0;
    out.humidity_percent = humidity_x10 / 10.0;
    out.gas_ppm = read_u16_be(frame.payload, 4);
    out.battery_mv = read_u16_be(frame.payload, 6);
    out.status = frame.payload[8];
    out.sequence = read_u16_be(frame.payload, 9);

    validate_sensor_data_impl(out);

    return out.valid;
}

void validate_sensor_data(SensorData& data) {
    data.valid = true;
    data.warnings.clear();
    validate_sensor_data_impl(data);
}

std::string sensor_data_to_json(const SensorData& data) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);

    oss << "{";
    oss << "\"device_id\":" << static_cast<int>(data.device_id) << ",";
    oss << "\"cmd\":" << static_cast<int>(data.cmd) << ",";
    oss << "\"temperature_c\":" << data.temperature_c << ",";
    oss << "\"humidity_percent\":" << data.humidity_percent << ",";
    oss << "\"gas_ppm\":" << data.gas_ppm << ",";
    oss << "\"battery_mv\":" << data.battery_mv << ",";
    oss << "\"status\":" << static_cast<int>(data.status) << ",";
    oss << "\"sequence\":" << data.sequence << ",";
    oss << "\"valid\":" << (data.valid ? "true" : "false");

    if (!data.warnings.empty()) {
        oss << ",\"warnings\":[";
        for (std::size_t i = 0; i < data.warnings.size(); ++i) {
            if (i != 0) {
                oss << ",";
            }
            oss << "\"" << data.warnings[i] << "\"";
        }
        oss << "]";
    }

    oss << "}";

    return oss.str();
}
}  // namespace app
