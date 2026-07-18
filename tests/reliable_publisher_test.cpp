#include "cache/file_cache.h"
#include "mqtt/mqtt_client.h"
#include "mqtt/reliable_publisher.h"

#include <cstdio>
#include <iostream>
#include <string>

int main() {
    const std::string cache_path =
        "data/test_publish_failure.cache";

    std::remove(cache_path.c_str());
    std::remove((cache_path + ".tmp").c_str());

    mqtt::MqttClient mqtt_client(
        "localhost",
        18830
    );

    cache::FileCache file_cache(cache_path);

    mqtt::ReliablePublisher publisher(
        mqtt_client,
        file_cache
    );

    const std::string topic = "sensor/16/data";
    const std::string payload =
        "{\"sequence\":1,\"valid\":true}";

    const mqtt::PublishResult result =
        publisher.publish(topic, payload);

    const auto messages = file_cache.load_all();

    std::cout << "publish result: "
              << mqtt::to_string(result)
              << std::endl;

    std::cout << "cached message count: "
              << messages.size()
              << std::endl;

    if (!messages.empty()) {
        std::cout << "cached topic: "
                  << messages[0].topic
                  << std::endl;

        std::cout << "cached payload: "
                  << messages[0].payload
                  << std::endl;
    }

    return result == mqtt::PublishResult::Cached
        && messages.size() == 1
        ? 0
        : 1;
}
