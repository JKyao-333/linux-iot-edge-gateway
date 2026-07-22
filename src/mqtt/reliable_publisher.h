#pragma once
#include <cstddef>
#include "app/runtime_metrics.h"
#include "cache/message_cache.h"
#include "mqtt/mqtt_client.h"
#include "publisher/publisher.h"

#include <string>
#include <string_view>
#include <mutex>

namespace mqtt {

using PublishResult = publishing::PublishStatus;

class ReliablePublisher final : public publishing::Publisher {
public:
    ReliablePublisher(
        MqttClient& mqtt_client,
        cache::MessageCache& message_cache,
        app::RuntimeMetrics* runtime_metrics = nullptr
    );

    PublishResult publish(
        const std::string& topic,
        const std::string& payload
    ) override;

    std::string_view channel() const noexcept override;

    std::size_t flush_cache();
private:
    MqttClient& mqtt_client_;
    cache::MessageCache& message_cache_;
    app::RuntimeMetrics* runtime_metrics_;
    std::mutex mutex_;
};

const char* to_string(PublishResult result);

}  // namespace mqtt
