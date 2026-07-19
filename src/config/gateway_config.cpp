#include "config/gateway_config.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cctype>
#include <set>
#include <string>
#include <vector>

namespace {

template <typename T>
void load_if_present(
    const YAML::Node& node,
    const char* key,
    T& value) {

    if (node[key]) {
        value = node[key].as<T>();
    }
}

bool is_supported_baud_rate(int baud_rate) {
    return baud_rate == 9600
        || baud_rate == 19200
        || baud_rate == 38400
        || baud_rate == 57600
        || baud_rate == 115200;
}

std::string uppercase(std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char character) {
            return static_cast<char>(
                std::toupper(character)
            );
        }
    );

    return value;
}

std::string lowercase(std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char character) {
            return static_cast<char>(
                std::tolower(character)
            );
        }
    );

    return value;
}

bool is_valid_log_level(const std::string& level) {
    return level == "DEBUG"
        || level == "INFO"
        || level == "WARN"
        || level == "ERROR";
}

}  // namespace

namespace config {

bool load_gateway_config(
    const std::string& file_path,
    GatewayConfig& gateway_config,
    std::string& error_message) {

    try {
        const YAML::Node root =
            YAML::LoadFile(file_path);

        if (!root.IsMap()) {
            error_message =
                "configuration root must be a YAML map";

            return false;
        }

        const YAML::Node serial = root["serial"];

        if (serial) {
            if (serial["devices"]) {
                if (!serial["devices"].IsSequence()) {
                    error_message =
                        "serial.devices must be a YAML sequence";

                    return false;
                }

                gateway_config.serial.devices =
                    serial["devices"]
                        .as<std::vector<std::string>>();
            } else if (serial["device"]) {
                gateway_config.serial.devices = {
                    serial["device"].as<std::string>()
                };
            }

            load_if_present(
                serial,
                "baud_rate",
                gateway_config.serial.baud_rate
            );

            load_if_present(
                serial,
                "reconnect_interval_seconds",
                gateway_config.serial
                    .reconnect_interval_seconds
            );
        }

        const YAML::Node mqtt = root["mqtt"];

        if (mqtt) {
            load_if_present(
                mqtt,
                "host",
                gateway_config.mqtt.host
            );

            int mqtt_port =
                gateway_config.mqtt.port;

            load_if_present(
                mqtt,
                "port",
                mqtt_port
            );

            if (mqtt_port < 1 || mqtt_port > 65535) {
                error_message =
                    "mqtt.port must be between 1 and 65535";

                return false;
            }

            gateway_config.mqtt.port =
                static_cast<std::uint16_t>(mqtt_port);

            load_if_present(
                mqtt,
                "topic_prefix",
                gateway_config.mqtt.topic_prefix
            );

            load_if_present(
                mqtt,
                "cache_retry_interval_seconds",
                gateway_config.mqtt
                    .cache_retry_interval_seconds
            );
        }

        const YAML::Node tcp = root["tcp"];

        if (tcp) {
            load_if_present(
                tcp,
                "enabled",
                gateway_config.tcp.enabled
            );

            load_if_present(
                tcp,
                "host",
                gateway_config.tcp.host
            );

            int tcp_port =
                gateway_config.tcp.port;

            load_if_present(
                tcp,
                "port",
                tcp_port
            );

            if (tcp_port < 1 || tcp_port > 65535) {
                error_message =
                    "tcp.port must be between 1 and 65535";

                return false;
            }

            gateway_config.tcp.port =
                static_cast<std::uint16_t>(tcp_port);
        }

        const YAML::Node cache = root["cache"];

        if (cache) {
            load_if_present(
                cache,
                "type",
                gateway_config.cache.type
            );

            gateway_config.cache.type =
                lowercase(gateway_config.cache.type);

            load_if_present(
                cache,
                "path",
                gateway_config.cache.path
            );
        }

        const YAML::Node log = root["log"];

        if (log) {
            load_if_present(
                log,
                "path",
                gateway_config.log.path
            );

            load_if_present(
                log,
                "level",
                gateway_config.log.level
            );

            gateway_config.log.level =
                uppercase(gateway_config.log.level);
        }

        if (gateway_config.serial.devices.empty()) {
            error_message =
                "serial.devices must not be empty";

            return false;
        }

        std::set<std::string> unique_serial_devices;

        for (const auto& device
             : gateway_config.serial.devices) {

            if (device.empty()) {
                error_message =
                    "serial device path must not be empty";

                return false;
            }

            if (!unique_serial_devices.insert(device).second) {
                error_message =
                    "serial.devices must not contain duplicates";

                return false;
            }
        }

        if (!is_supported_baud_rate(
                gateway_config.serial.baud_rate)) {

            error_message =
                "serial.baud_rate is not supported";

            return false;
        }

        if (gateway_config.serial
                .reconnect_interval_seconds < 1) {

            error_message =
                "serial reconnect interval must be positive";

            return false;
        }

        if (gateway_config.mqtt.host.empty()) {
            error_message =
                "mqtt.host must not be empty";

            return false;
        }

        if (gateway_config.mqtt.topic_prefix.empty()) {
            error_message =
                "mqtt.topic_prefix must not be empty";

            return false;
        }

        if (gateway_config.mqtt
                .cache_retry_interval_seconds < 1) {

            error_message =
                "mqtt cache retry interval must be positive";

            return false;
        }

        if (gateway_config.tcp.enabled
            && gateway_config.tcp.host.empty()) {

            error_message =
                "tcp.host must not be empty when TCP is enabled";

            return false;
        }

        if (gateway_config.cache.path.empty()) {
            error_message =
                "cache.path must not be empty";

            return false;
        }

        if (gateway_config.cache.type != "sqlite"
            && gateway_config.cache.type != "file") {

            error_message =
                "cache.type must be sqlite or file";

            return false;
        }

        if (gateway_config.log.path.empty()) {
            error_message =
                "log.path must not be empty";

            return false;
        }

        if (!is_valid_log_level(
                gateway_config.log.level)) {

            error_message =
                "log.level must be DEBUG, INFO, WARN or ERROR";

            return false;
        }

        error_message.clear();
        return true;
    } catch (const YAML::Exception& exception) {
        error_message =
            "YAML error: "
            + std::string(exception.what());

        return false;
    }
}

}  // namespace config
