#include "config/gateway_config.h"

#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    const std::string config_path =
        argc >= 2
            ? argv[1]
            : "config/gateway.yaml";

    config::GatewayConfig gateway_config;
    std::string error_message;

    if (!config::load_gateway_config(
            config_path,
            gateway_config,
            error_message)) {

        std::cerr
            << "config load failed: "
            << error_message
            << std::endl;

        return 1;
    }

    std::cout << std::boolalpha;

    std::cout
        << "serial.device: "
        << gateway_config.serial.device
        << std::endl;

    std::cout
        << "serial.baud_rate: "
        << gateway_config.serial.baud_rate
        << std::endl;

    std::cout
        << "mqtt.host: "
        << gateway_config.mqtt.host
        << std::endl;

    std::cout
        << "mqtt.port: "
        << gateway_config.mqtt.port
        << std::endl;

    std::cout
        << "tcp.enabled: "
        << gateway_config.tcp.enabled
        << std::endl;

    std::cout
        << "tcp.host: "
        << gateway_config.tcp.host
        << std::endl;

    std::cout
        << "tcp.port: "
        << gateway_config.tcp.port
        << std::endl;

    std::cout
        << "cache.type: "
        << gateway_config.cache.type
        << std::endl;

    std::cout
        << "cache.path: "
        << gateway_config.cache.path
        << std::endl;

    std::cout
        << "log.path: "
        << gateway_config.log.path
        << std::endl;

    std::cout
        << "log.level: "
        << gateway_config.log.level
        << std::endl;

    const bool values_are_correct =
        gateway_config.serial.device
            == "/tmp/tty_gateway"
        && gateway_config.serial.baud_rate
            == 115200
        && gateway_config.mqtt.host
            == "localhost"
        && gateway_config.mqtt.port
            == 1883
        && gateway_config.mqtt.topic_prefix
            == "sensor"
        && gateway_config.tcp.enabled
        && gateway_config.tcp.host
            == "127.0.0.1"
        && gateway_config.tcp.port
            == 9000
        && gateway_config.cache.type
            == "sqlite"
        && gateway_config.cache.path
            == "data/pending_messages.db"
        && gateway_config.log.path
            == "logs/gateway.log"
        && gateway_config.log.level
            == "DEBUG";

    if (!values_are_correct) {
        std::cerr
            << "config values do not match expected values"
            << std::endl;

        return 1;
    }

    std::cout
        << "gateway config test passed"
        << std::endl;

    return 0;
}
