#include "mqtt/reliable_publisher.h"
#include <vector>
#include <iostream>

namespace mqtt {

ReliablePublisher::ReliablePublisher(
    MqttClient& mqtt_client,
    cache::FileCache& file_cache)
    : mqtt_client_(mqtt_client),
      file_cache_(file_cache) {}

PublishResult ReliablePublisher::publish(
    const std::string& topic,
    const std::string& payload) {

    if (mqtt_client_.publish(topic, payload)) {
        return PublishResult::Published;
    }

    if (file_cache_.append(topic, payload)) {
        std::cout << "[WARN] mqtt unavailable, message cached"
                  << ", topic=" << topic << std::endl;

        return PublishResult::Cached;
    }

    std::cerr << "[ERROR] mqtt publish and local cache both failed"
              << ", topic=" << topic << std::endl;

    return PublishResult::Failed;
}

std::size_t ReliablePublisher::flush_cache() {
    const auto messages = file_cache_.load_all();

    if (messages.empty()) {
            return 0;
    }

    std::size_t published_count = 0;

    for (const auto& message : messages) {
        if (!mqtt_client_.publish(
                message.topic,
                message.payload)) {

            std::cout << "[WARN] cache replay stopped"
                      << ", published=" << published_count
                      << ", remaining="
                      << messages.size() - published_count
                      << std::endl;

            break;
        }

        ++published_count;
    }

    if (published_count == 0) {
        return 0;
    }

    std::vector<cache::CachedMessage> remaining(
        messages.begin() + published_count,
        messages.end()
    );

    if (!file_cache_.replace_all(remaining)) {
        std::cerr << "[ERROR] failed to update cache after replay"
                  << std::endl;

        return published_count;
    }

    std::cout << "[INFO] cache replay complete"
              << ", published=" << published_count
              << ", remaining=" << remaining.size()
              << std::endl;

    return published_count;
}

const char* to_string(PublishResult result) {
    switch (result) {
        case PublishResult::Published:
            return "published";

        case PublishResult::Cached:
            return "cached";

        case PublishResult::Failed:
            return "failed";
    }

    return "unknown";
}

}  // namespace mqtt
