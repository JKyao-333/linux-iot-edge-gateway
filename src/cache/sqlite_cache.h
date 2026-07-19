#pragma once

#include "cache/message_cache.h"

#include <cstddef>
#include <string>
#include <vector>

struct sqlite3;

namespace cache {

class SqliteCache : public MessageCache {
public:
    explicit SqliteCache(std::string path);
    ~SqliteCache() override;

    SqliteCache(const SqliteCache&) = delete;
    SqliteCache& operator=(const SqliteCache&) = delete;

    bool is_ready() const noexcept;
    const std::string& error_message() const noexcept;

    bool append(
        const std::string& topic,
        const std::string& payload
    ) override;

    std::vector<CachedMessage> load_all() const override;

    bool remove_first(std::size_t count) override;

private:
    bool initialize();
    bool execute(const char* sql) const;
    void set_error(const std::string& message) const;

    std::string path_;
    sqlite3* database_ = nullptr;
    bool ready_ = false;
    mutable std::string error_message_;
};

}  // namespace cache
