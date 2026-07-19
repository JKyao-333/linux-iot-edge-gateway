#include "mqtt/mqtt_client.h"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

namespace {

bool wait_for_connection(
    const mqtt::MqttClient& client,
    std::chrono::seconds timeout) {

    const auto deadline =
        std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now()
           < deadline) {

        if (client.is_connected()) {
            return true;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(50)
        );
    }

    return client.is_connected();
}

}  // namespace

int main(int argc, char* argv[]) {
    const std::string host =
        argc >= 2 ? argv[1] : "localhost";

    const std::uint16_t port =
        argc >= 3
            ? static_cast<std::uint16_t>(
                std::stoi(argv[2])
            )
            : 1883;

    mqtt::MqttClientOptions options;

    if (argc >= 5) {
        options.username = argv[3];
        options.password = argv[4];
    }

    if (argc >= 6) {
        options.tls_enabled = true;
        options.ca_file = argv[5];
    }

    mqtt::MqttClient client(
        host,
        port,
        options
    );

    if (!wait_for_connection(
            client,
            std::chrono::seconds(3))) {

        std::cerr
            << "MQTT test failed: connection timeout"
            << std::endl;

        return 1;
    }

    const std::string topic =
        "sensor/10/data";

    const std::string payload =
        "{\"device_id\":16,"
        "\"temperature_c\":25.3,"
        "\"humidity_percent\":60.8}";

    const bool published =
        client.publish(topic, payload);

    std::cout
        << "publish result: "
        << std::boolalpha
        << published
        << std::endl;

    return published ? 0 : 1;
}
