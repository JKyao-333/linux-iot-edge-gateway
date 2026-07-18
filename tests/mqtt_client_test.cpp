#include "mqtt/mqtt_client.h"

#include <iostream>
#include <string>

int main() {
    mqtt::MqttClient client("localhost", 1883);

    const std::string topic = "sensor/10/data";
    const std::string payload =
        "{\"device_id\":16,\"temperature_c\":25.3,\"humidity_percent\":60.8}";

    bool ok = client.publish(topic, payload);

    std::cout << "publish result: " << (ok ? "true" : "false") << std::endl;

    return ok ? 0 : 1;
}
