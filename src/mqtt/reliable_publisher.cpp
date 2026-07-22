#include "mqtt/reliable_publisher.h"
#include <mutex>
#include <vector>
#include <iostream>

namespace mqtt {

ReliablePublisher::ReliablePublisher(
    MqttClient& mqtt_client,
    cache::MessageCache& message_cache,
    app::RuntimeMetrics* runtime_metrics)
    : mqtt_client_(mqtt_client),
      message_cache_(message_cache),
      runtime_metrics_(runtime_metrics) {

    if (runtime_metrics_ != nullptr) {
        runtime_metrics_->set_cache_depth(
            message_cache_.load_all().size()
        );
    }
}

PublishResult ReliablePublisher::publish(
    const std::string& topic,
    const std::string& payload) {

    std::lock_guard<std::mutex> lock(mutex_);

    if (mqtt_client_.publish(topic, payload)) {
        if (runtime_metrics_ != nullptr) {
            runtime_metrics_->record_mqtt_publish_success();
        }

        return PublishResult::Published;
    }

    if (runtime_metrics_ != nullptr) {
        runtime_metrics_->record_mqtt_publish_failed();
    }

    if (message_cache_.append(topic, payload)) {
        if (runtime_metrics_ != nullptr) {
            runtime_metrics_->record_cache_enqueue();
            runtime_metrics_->set_cache_depth(
                message_cache_.load_all().size()
            );
        }

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

    if (runtime_metrics_ != nullptr) {
        runtime_metrics_->record_cache_flush_attempt();
    }

    const auto messages = message_cache_.load_all();

    if (runtime_metrics_ != nullptr) {
        runtime_metrics_->set_cache_depth(messages.size());
    }

    if (messages.empty()) {
        return 0;
    }

    std::size_t published_count = 0;

    for (const auto& message : messages) {
        if (!mqtt_client_.publish(
                message.topic,
                message.payload)) {

            if (runtime_metrics_ != nullptr) {
                runtime_metrics_->record_mqtt_publish_failed();
            }

            std::cout << "[WARN] cache replay stopped"
                      << ", published=" << published_count
                      << ", remaining="
                      << messages.size() - published_count
                      << std::endl;

            break;
        }

        if (runtime_metrics_ != nullptr) {
            runtime_metrics_->record_mqtt_publish_success();
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

    if (runtime_metrics_ != nullptr) {
        runtime_metrics_->set_cache_depth(
            messages.size() - published_count
        );
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
