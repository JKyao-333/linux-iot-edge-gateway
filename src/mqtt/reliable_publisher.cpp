#include "mqtt/reliable_publisher.h"
#include <mutex>
#include <vector>
#include <iostream>

namespace mqtt {

ReliablePublisher::ReliablePublisher(
    MqttClient& mqtt_client,
    cache::MessageCache& message_cache)
    : mqtt_client_(mqtt_client),
      message_cache_(message_cache) {}

PublishResult ReliablePublisher::publish(
    const std::string& topic,
    const std::string& payload) {

    std::lock_guard<std::mutex> lock(mutex_);

    if (mqtt_client_.publish(topic, payload)) {
        return PublishResult::Published;
    }

    if (message_cache_.append(topic, payload)) {
        std::cout << "[WARN] mqtt unavailable, message cached"
                  << ", topic=" << topic << std::endl;

        return PublishResult::Deferred;
    }

    std::cerr << "[ERROR] mqtt publish and local cache both failed"
              << ", topic=" << topic << std::endl;

    return PublishResult::Failed;
}

std::string_view ReliablePublisher::channel() const noexcept {
    return "mqtt";
}

std::size_t ReliablePublisher::flush_cache() {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto messages = message_cache_.load_all();

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

    if (!message_cache_.remove_first(published_count)) {
        std::cerr << "[ERROR] failed to update cache after replay"
                  << std::endl;

        return published_count;
    }

    std::cout << "[INFO] cache replay complete"
              << ", published=" << published_count
              << ", remaining="
              << messages.size() - published_count
              << std::endl;

    return published_count;
}

const char* to_string(PublishResult result) {
    switch (result) {
        case PublishResult::Published:
            return "published";

        case PublishResult::Deferred:
            return "deferred";

        case PublishResult::Failed:
            return "failed";
    }

    return "unknown";
}

}  // namespace mqtt
