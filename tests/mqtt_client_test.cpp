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

int main() {
    mqtt::MqttClient client(
        "localhost",
        1883
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
