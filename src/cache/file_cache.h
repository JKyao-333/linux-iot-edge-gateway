#pragma once

#include "cache/message_cache.h"

#include <cstddef>
#include <string>

namespace cache {

class FileCache : public MessageCache {
public:
    explicit FileCache(std::string path);

    bool append(
        const std::string& topic,
        const std::string& payload
    ) override;

    std::vector<CachedMessage> load_all() const override;

    bool remove_first(std::size_t count) override;

    bool replace_all(const std::vector<CachedMessage>& messages);

private:
    std::string path_;
};

}  // namespace cache
