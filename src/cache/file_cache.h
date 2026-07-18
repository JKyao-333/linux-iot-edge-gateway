#pragma once

#include <string>
#include <vector>

namespace cache {

struct CachedMessage {
    std::string topic;
    std::string payload;
};

class FileCache {
public:
    explicit FileCache(std::string path);

    bool append(const std::string& topic, const std::string& payload);
    std::vector<CachedMessage> load_all() const;
    bool replace_all(const std::vector<CachedMessage>& messages);
private:
    std::string path_;
};

}  // namespace cache
