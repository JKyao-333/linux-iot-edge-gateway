#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace cache {

struct CachedMessage {
    std::string topic;
    std::string payload;
};

class MessageCache {
public:
    virtual ~MessageCache() = default;

    virtual bool append(
        const std::string& topic,
        const std::string& payload
    ) = 0;

    virtual std::vector<CachedMessage> load_all() const = 0;

    virtual bool remove_first(std::size_t count) = 0;
};

}  // namespace cache
