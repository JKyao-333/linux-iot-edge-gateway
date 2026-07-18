#include "cache/file_cache.h"

#include <cstdio>
#include <iostream>
#include <string>

int main() {
    const std::string cache_path =
        "data/test_pending_messages.cache";

    std::remove(cache_path.c_str());

    cache::FileCache file_cache(cache_path);

    bool first_ok = file_cache.append(
        "sensor/16/data",
        "{\"sequence\":1,\"valid\":true}"
    );

    bool second_ok = file_cache.append(
        "sensor/16/data",
        "{\"sequence\":2,\"valid\":true}"
    );

    auto messages = file_cache.load_all();

    std::cout << "first append: "
              << (first_ok ? "true" : "false") << std::endl;

    std::cout << "second append: "
              << (second_ok ? "true" : "false") << std::endl;

    std::cout << "cached message count: "
              << messages.size() << std::endl;

    for (std::size_t i = 0; i < messages.size(); ++i) {
        std::cout << "message " << i + 1
                  << " topic=" << messages[i].topic
                  << " payload=" << messages[i].payload
                  << std::endl;
    }

    return first_ok && second_ok && messages.size() == 2 ? 0 : 1;
}
