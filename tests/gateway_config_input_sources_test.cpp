#include "config/gateway_config.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

namespace {

bool load_case(
    const std::string& path,
    const std::string& yaml,
    bool expected,
    const std::string& expected_error = {}) {

    std::ofstream output(path);
    output << yaml;
    output.close();

    config::GatewayConfig gateway_config;
    std::string error;
    const bool loaded = config::load_gateway_config(
        path,
        gateway_config,
        error
    );
    if (loaded != expected) {
        std::cerr << "unexpected load result: " << error << std::endl;
        return false;
    }
    if (!expected && error != expected_error) {
        std::cerr << "unexpected validation error: " << error << std::endl;
        return false;
    }
    return true;
}

}  // namespace

int main() {
    const std::string path = "/tmp/gateway_config_input_sources_test.yaml";
    const std::string uart_only =
        "serial:\n"
        "  devices: [/tmp/tty_gateway]\n"
        "modbus:\n"
        "  enabled: false\n"
        "can:\n"
        "  enabled: false\n";
    const std::string modbus_only =
        "serial:\n"
        "  devices: []\n"
        "modbus:\n"
        "  enabled: true\n"
        "  port: /tmp/tty_modbus\n"
        "can:\n"
        "  enabled: false\n";
    const std::string can_only =
        "serial:\n"
        "  devices: []\n"
        "modbus:\n"
        "  enabled: false\n"
        "can:\n"
        "  enabled: true\n"
        "  interface: vcan0\n";
    const std::string mixed =
        "serial:\n"
        "  devices: [/tmp/tty_gateway]\n"
        "modbus:\n"
        "  enabled: true\n"
        "  port: /tmp/tty_modbus\n"
        "can:\n"
        "  enabled: true\n"
        "  interface: vcan0\n";
    const std::string no_inputs =
        "serial:\n"
        "  devices: []\n"
        "modbus:\n"
        "  enabled: false\n"
        "can:\n"
        "  enabled: false\n";

    const bool passed =
        load_case(path, uart_only, true)
        && load_case(path, modbus_only, true)
        && load_case(path, can_only, true)
        && load_case(path, mixed, true)
        && load_case(
            path,
            no_inputs,
            false,
            "at least one input source must be enabled"
        );
    std::remove(path.c_str());
    if (!passed) {
        return 1;
    }

    std::cout << "gateway input source combinations passed" << std::endl;
    return 0;
}
