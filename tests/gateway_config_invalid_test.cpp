#include "config/gateway_config.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

namespace {

bool write_file(
    const std::string& path,
    const std::string& content) {

    std::ofstream output(path);

    if (!output.is_open()) {
        return false;
    }

    output << content;
    return output.good();
}

bool expect_load_failure(
    const std::string& path,
    const std::string& expected_error) {

    config::GatewayConfig gateway_config;
    std::string error_message;

    const bool loaded =
        config::load_gateway_config(
            path,
            gateway_config,
            error_message
        );

    std::cout
        << "error: "
        << error_message
        << std::endl;

    return !loaded
        && error_message.find(expected_error)
            != std::string::npos;
}

}  // namespace

int main() {
    const std::string test_path =
        "/tmp/gateway_config_invalid_test.yaml";

    if (!write_file(
            test_path,
            "mqtt:\n"
            "  port: 70000\n")) {

        std::cerr
            << "failed to create invalid config"
            << std::endl;

        return 1;
    }

    if (!expect_load_failure(
            test_path,
            "mqtt.port")) {

        std::cerr
            << "invalid MQTT port was not rejected"
            << std::endl;

        std::remove(test_path.c_str());
        return 1;
    }

    if (!write_file(
            test_path,
            "serial:\n"
            "  baud_rate: 12345\n")) {

        std::cerr
            << "failed to rewrite invalid config"
            << std::endl;

        std::remove(test_path.c_str());
        return 1;
    }

    if (!expect_load_failure(
            test_path,
            "baud_rate")) {

        std::cerr
            << "invalid baud rate was not rejected"
            << std::endl;

        std::remove(test_path.c_str());
        return 1;
    }

    if (!write_file(
            test_path,
            "mqtt:\n"
            "  port: [1883\n")) {

        std::cerr
            << "failed to write malformed YAML"
            << std::endl;

        std::remove(test_path.c_str());
        return 1;
    }

    if (!expect_load_failure(
            test_path,
            "YAML error")) {

        std::cerr
            << "malformed YAML was not rejected"
            << std::endl;

        std::remove(test_path.c_str());
        return 1;
    }

    if (!write_file(
            test_path,
            "cache:\n"
            "  type: redis\n")) {

        std::cerr
            << "failed to write invalid cache type"
            << std::endl;

        std::remove(test_path.c_str());
        return 1;
    }

    if (!expect_load_failure(
            test_path,
            "cache.type")) {

        std::cerr
            << "invalid cache type was not rejected"
            << std::endl;

        std::remove(test_path.c_str());
        return 1;
    }

    std::remove(test_path.c_str());

    std::cout
        << "invalid gateway config test passed"
        << std::endl;

    return 0;
}
