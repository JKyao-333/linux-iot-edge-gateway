#pragma once
#include <cstddef>
#include "cache/message_cache.h"
#include "mqtt/mqtt_client.h"
#include "publisher/publisher.h"

#include <string>
#include <string_view>

namespace mqtt {

using PublishResult = publishing::PublishStatus;

class ReliablePublisher final : public publishing::Publisher {
public:
    ReliablePublisher(
        MqttClient& mqtt_client,
        cache::MessageCache& message_cache
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
};

const char* to_string(PublishResult result);

}  // namespace mqtt
