#pragma once
#include <cstddef>
#include "cache/file_cache.h"
#include "mqtt/mqtt_client.h"

#include <string>

namespace mqtt {

enum class PublishResult {
    Published,
    Cached,
    Failed
};

class ReliablePublisher {
public:
    ReliablePublisher(
        MqttClient& mqtt_client,
        cache::FileCache& file_cache
    );

    PublishResult publish(
        const std::string& topic,
        const std::string& payload
    );
std::size_t flush_cache();
private:
    MqttClient& mqtt_client_;
    cache::FileCache& file_cache_;
};

const char* to_string(PublishResult result);

}  // namespace mqtt
