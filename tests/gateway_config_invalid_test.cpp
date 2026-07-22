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

    if (!write_file(test_path, "{}\n")) {
        std::cerr << "failed to create default config"
                  << std::endl;
        return 1;
    }

    config::GatewayConfig default_config;
    std::string default_error;
    if (!config::load_gateway_config(
            test_path,
            default_config,
            default_error)
        || default_config.http.enabled
        || default_config.http.host != "127.0.0.1"
        || default_config.http.port != 8080) {

        std::cerr << "HTTP defaults are incorrect"
                  << std::endl;
        std::remove(test_path.c_str());
        return 1;
    }

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

    if (!write_file(
            test_path,
            "serial:\n"
            "  devices:\n"
            "    - /tmp/tty_gateway\n"
            "    - /tmp/tty_gateway\n")) {

        std::cerr
            << "failed to write duplicate serial devices"
            << std::endl;

        std::remove(test_path.c_str());
        return 1;
    }

    if (!expect_load_failure(
            test_path,
            "duplicates")) {

        std::cerr
            << "duplicate serial devices were not rejected"
            << std::endl;

        std::remove(test_path.c_str());
        return 1;
    }

    if (!write_file(
            test_path,
            "serial:\n"
            "  devices: []\n")) {

        std::cerr
            << "failed to write empty serial device list"
            << std::endl;

        std::remove(test_path.c_str());
        return 1;
    }

    if (!expect_load_failure(
            test_path,
            "must not be empty")) {

        std::cerr
            << "empty serial device list was not rejected"
            << std::endl;

        std::remove(test_path.c_str());
        return 1;
    }

    if (!write_file(
            test_path,
            "mqtt:\n"
            "  password: secret\n")) {

        std::cerr
            << "failed to write password-only MQTT config"
            << std::endl;

        std::remove(test_path.c_str());
        return 1;
    }

    if (!expect_load_failure(
            test_path,
            "requires mqtt.username")) {

        std::cerr
            << "MQTT password without username was not rejected"
            << std::endl;

        std::remove(test_path.c_str());
        return 1;
    }

    if (!write_file(
            test_path,
            "mqtt:\n"
            "  tls:\n"
            "    enabled: true\n")) {

        std::cerr
            << "failed to write TLS config without CA"
            << std::endl;

        std::remove(test_path.c_str());
        return 1;
    }

    if (!expect_load_failure(
            test_path,
            "ca_file")) {

        std::cerr
            << "TLS config without CA was not rejected"
            << std::endl;

        std::remove(test_path.c_str());
        return 1;
    }

    if (!write_file(
            test_path,
            "mqtt:\n"
            "  tls:\n"
            "    enabled: true\n"
            "    ca_file: /tmp/ca.crt\n"
            "    certificate_file: /tmp/client.crt\n")) {

        std::cerr
            << "failed to write incomplete mTLS config"
            << std::endl;

        std::remove(test_path.c_str());
        return 1;
    }

    if (!expect_load_failure(
            test_path,
            "configured together")) {

        std::cerr
            << "incomplete mTLS config was not rejected"
            << std::endl;

        std::remove(test_path.c_str());
        return 1;
    }

    if (!write_file(
            test_path,
            "mqtt:\n"
            "  tls:\n"
            "    insecure: true\n")) {

        std::cerr
            << "failed to write insecure non-TLS config"
            << std::endl;

        std::remove(test_path.c_str());
        return 1;
    }

    if (!expect_load_failure(
            test_path,
            "requires TLS")) {

        std::cerr
            << "TLS insecure flag without TLS was not rejected"
            << std::endl;

        std::remove(test_path.c_str());
        return 1;
    }

    if (!write_file(
            test_path,
            "http:\n"
            "  enabled: true\n"
            "  port: 0\n")) {

        std::cerr << "failed to write invalid HTTP port"
                  << std::endl;
        std::remove(test_path.c_str());
        return 1;
    }

    if (!expect_load_failure(test_path, "http.port")) {
        std::cerr << "invalid HTTP port was not rejected"
                  << std::endl;
        std::remove(test_path.c_str());
        return 1;
    }

    if (!write_file(
            test_path,
            "http:\n"
            "  enabled: true\n"
            "  host: invalid-host-name\n")) {

        std::cerr << "failed to write invalid HTTP host"
                  << std::endl;
        std::remove(test_path.c_str());
        return 1;
    }

    if (!expect_load_failure(test_path, "http.host")) {
        std::cerr << "invalid HTTP host was not rejected"
                  << std::endl;
        std::remove(test_path.c_str());
        return 1;
    }

    if (!write_file(
            test_path,
            "http:\n"
            "  enabled: false\n"
            "  host: \"\"\n")) {

        std::cerr << "failed to write empty HTTP host"
                  << std::endl;
        std::remove(test_path.c_str());
        return 1;
    }

    if (!expect_load_failure(test_path, "http.host")) {
        std::cerr << "empty HTTP host was not rejected"
                  << std::endl;
        std::remove(test_path.c_str());
        return 1;
    }

    if (!write_file(
            test_path,
            "http:\n"
            "  enabled: not-a-boolean\n")) {

        std::cerr << "failed to write invalid HTTP enabled value"
                  << std::endl;
        std::remove(test_path.c_str());
        return 1;
    }

    if (!expect_load_failure(test_path, "YAML error")) {
        std::cerr << "invalid HTTP enabled value was not rejected"
                  << std::endl;
        std::remove(test_path.c_str());
        return 1;
    }

    if (!write_file(
            test_path,
            "http:\n"
            "  - enabled\n")) {

        std::cerr << "failed to write invalid HTTP type"
                  << std::endl;
        std::remove(test_path.c_str());
        return 1;
    }

    if (!expect_load_failure(test_path, "YAML map")) {
        std::cerr << "invalid HTTP mapping was not rejected"
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
