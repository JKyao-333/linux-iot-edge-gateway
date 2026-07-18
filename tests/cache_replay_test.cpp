#include "cache/file_cache.h"
#include "mqtt/mqtt_client.h"
#include "mqtt/reliable_publisher.h"

#include <cstdio>
#include <iostream>
#include <string>

int main() {
    const std::string cache_path =
        "data/test_cache_replay.cache";

    std::remove(cache_path.c_str());
    std::remove((cache_path + ".tmp").c_str());

    cache::FileCache file_cache(cache_path);

    file_cache.append(
        "sensor/16/data",
        "{\"sequence\":101,\"valid\":true}"
    );

    file_cache.append(
        "sensor/16/data",
        "{\"sequence\":102,\"valid\":true}"
    );

    const auto before = file_cache.load_all();

    mqtt::MqttClient mqtt_client(
        "localhost",
        1883
    );

    mqtt::ReliablePublisher publisher(
        mqtt_client,
        file_cache
    );

    const std::size_t published_count =
        publisher.flush_cache();

    const auto after = file_cache.load_all();

    std::cout << "before count: "
              << before.size() << std::endl;

    std::cout << "published count: "
              << published_count << std::endl;

    std::cout << "after count: "
              << after.size() << std::endl;

    return before.size() == 2
        && published_count == 2
        && after.empty()
        ? 0
        : 1;
}
