#include "cache/sqlite_cache.h"

#include <filesystem>
#include <iostream>
#include <string>

namespace {

void remove_database(const std::string& path) {
    std::error_code error;
    std::filesystem::remove(path, error);
    std::filesystem::remove(path + "-wal", error);
    std::filesystem::remove(path + "-shm", error);
}

}  // namespace

int main() {
    const std::string database_path =
        "/tmp/sqlite_cache_test.db";

    remove_database(database_path);

    bool passed = false;

    {
        cache::SqliteCache message_cache(database_path);

        if (!message_cache.is_ready()) {
            std::cerr
                << message_cache.error_message()
                << std::endl;
            return 1;
        }

        const bool appended =
            message_cache.append(
                "sensor/16/data",
                "{\"sequence\":1,\n\"note\":\"a\\tb\"}"
            )
            && message_cache.append(
                "sensor/16/data",
                "{\"sequence\":2}"
            )
            && message_cache.append(
                "sensor/17/data",
                "{\"sequence\":3}"
            );

        const auto before = message_cache.load_all();
        const bool removed = message_cache.remove_first(2);
        const auto after = message_cache.load_all();

        passed = appended
            && before.size() == 3
            && before[0].payload.find('\n')
                != std::string::npos
            && removed
            && after.size() == 1
            && after[0].topic == "sensor/17/data"
            && after[0].payload == "{\"sequence\":3}";

        std::cout
            << "before count: " << before.size()
            << std::endl;
        std::cout
            << "after count: " << after.size()
            << std::endl;
    }

    remove_database(database_path);
    return passed ? 0 : 1;
}
