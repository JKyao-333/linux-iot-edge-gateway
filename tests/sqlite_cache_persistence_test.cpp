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
        "/tmp/sqlite_cache_persistence_test.db";

    remove_database(database_path);

    {
        cache::SqliteCache first_process(database_path);

        if (!first_process.is_ready()
            || !first_process.append(
                "sensor/16/data",
                "{\"sequence\":101}")
            || !first_process.append(
                "sensor/16/data",
                "{\"sequence\":102}")) {

            return 1;
        }
    }

    bool passed = false;

    {
        cache::SqliteCache restarted_process(database_path);
        const auto restored = restarted_process.load_all();

        passed = restarted_process.is_ready()
            && restored.size() == 2
            && restored[0].payload
                == "{\"sequence\":101}"
            && restored[1].payload
                == "{\"sequence\":102}"
            && restarted_process.remove_first(1);

        std::cout
            << "restored count: " << restored.size()
            << std::endl;
    }

    {
        cache::SqliteCache final_process(database_path);
        const auto remaining = final_process.load_all();

        passed = passed
            && remaining.size() == 1
            && remaining[0].payload
                == "{\"sequence\":102}";

        std::cout
            << "remaining count: " << remaining.size()
            << std::endl;
    }

    remove_database(database_path);
    return passed ? 0 : 1;
}
