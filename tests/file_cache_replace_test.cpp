#include "cache/file_cache.h"

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

int main() {
    const std::string cache_path =
        "data/test_replace_messages.cache";

    std::remove(cache_path.c_str());
    std::remove((cache_path + ".tmp").c_str());

    cache::FileCache file_cache(cache_path);

    file_cache.append(
        "sensor/16/data",
        "{\"sequence\":1}"
    );

    file_cache.append(
        "sensor/16/data",
        "{\"sequence\":2}"
    );

    file_cache.append(
        "sensor/16/data",
        "{\"sequence\":3}"
    );

    auto before = file_cache.load_all();

    std::vector<cache::CachedMessage> remaining(
        before.begin() + 1,
        before.end()
    );

    bool replace_ok = file_cache.replace_all(remaining);
    auto after = file_cache.load_all();
    const bool remove_ok = file_cache.remove_first(1);
    const auto final_messages = file_cache.load_all();

    std::cout << "before count: "
              << before.size() << std::endl;

    std::cout << "replace result: "
              << (replace_ok ? "true" : "false") << std::endl;

    std::cout << "after count: "
              << after.size() << std::endl;

    std::cout << "final count: "
              << final_messages.size() << std::endl;

    for (std::size_t i = 0; i < after.size(); ++i) {
        std::cout << "remaining " << i + 1
                  << " payload=" << after[i].payload
                  << std::endl;
    }

    return replace_ok
        && remove_ok
        && before.size() == 3
        && after.size() == 2
        && final_messages.size() == 1
        && final_messages[0].payload
            == "{\"sequence\":3}"
        ? 0
        : 1;
}
