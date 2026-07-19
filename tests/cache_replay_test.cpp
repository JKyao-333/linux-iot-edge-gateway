#include "cache/sqlite_cache.h"
#include "mqtt/mqtt_client.h"
#include "mqtt/reliable_publisher.h"

#include <chrono>
#include <filesystem>
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
    const std::string cache_path =
        "/tmp/test_cache_replay.db";

    std::error_code remove_error;
    std::filesystem::remove(cache_path, remove_error);
    std::filesystem::remove(
        cache_path + "-wal",
        remove_error
    );
    std::filesystem::remove(
        cache_path + "-shm",
        remove_error
    );

    cache::SqliteCache message_cache(cache_path);

    if (!message_cache.is_ready()) {
        std::cerr
            << message_cache.error_message()
            << std::endl;
        return 1;
    }

    message_cache.append(
        "sensor/16/data",
        "{\"sequence\":101,\"valid\":true}"
    );

    message_cache.append(
        "sensor/16/data",
        "{\"sequence\":102,\"valid\":true}"
    );

    const auto before =
        message_cache.load_all();

    mqtt::MqttClient mqtt_client(
        "localhost",
        1883
    );

    if (!wait_for_connection(
            mqtt_client,
            std::chrono::seconds(3))) {

        std::cerr
            << "cache replay test failed: "
            << "MQTT connection timeout"
            << std::endl;

        return 1;
    }

    mqtt::ReliablePublisher publisher(
        mqtt_client,
        message_cache
    );

    const std::size_t published_count =
        publisher.flush_cache();

    const auto after =
        message_cache.load_all();

    std::cout
        << "before count: "
        << before.size()
        << std::endl;

    std::cout
        << "published count: "
        << published_count
        << std::endl;

    std::cout
        << "after count: "
        << after.size()
        << std::endl;

    const bool passed =
        before.size() == 2
        && published_count == 2
        && after.empty();

    return passed ? 0 : 1;
}
