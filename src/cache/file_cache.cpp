#include "cache/file_cache.h"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <utility>

namespace cache {

namespace {

bool contains_separator(const std::string& text) {
    return text.find('\t') != std::string::npos
        || text.find('\n') != std::string::npos
        || text.find('\r') != std::string::npos;
}

}  // namespace

FileCache::FileCache(std::string path)
    : path_(std::move(path)) {}

bool FileCache::append(
    const std::string& topic,
    const std::string& payload) {

    if (contains_separator(topic) || contains_separator(payload)) {
        std::cerr << "[ERROR] cache message contains invalid separator"
                  << std::endl;
        return false;
    }

    const std::filesystem::path cache_path(path_);
    const std::filesystem::path parent = cache_path.parent_path();

    if (!parent.empty()) {
        std::error_code error;
        std::filesystem::create_directories(parent, error);

        if (error) {
            std::cerr << "[ERROR] create cache directory failed: "
                      << error.message() << std::endl;
            return false;
        }
    }

    std::ofstream output(path_, std::ios::app);
    if (!output.is_open()) {
        std::cerr << "[ERROR] open cache file failed: "
                  << path_ << std::endl;
        return false;
    }

    output << topic << '\t' << payload << '\n';
    output.flush();

    return output.good();
}

std::vector<CachedMessage> FileCache::load_all() const {
    std::vector<CachedMessage> messages;

    std::ifstream input(path_);
    if (!input.is_open()) {
        return messages;
    }

    std::string line;

    while (std::getline(input, line)) {
        const std::size_t separator = line.find('\t');

        if (separator == std::string::npos) {
            std::cerr << "[WARN] skip malformed cache line"
                      << std::endl;
            continue;
        }

        CachedMessage message;
        message.topic = line.substr(0, separator);
        message.payload = line.substr(separator + 1);

        messages.push_back(std::move(message));
    }

    return messages;
}

bool FileCache::replace_all(
    const std::vector<CachedMessage>& messages) {

    for (const auto& message : messages) {
        if (contains_separator(message.topic)
            || contains_separator(message.payload)) {

            std::cerr << "[ERROR] cache message contains invalid separator"
                      << std::endl;
            return false;
        }
    }

    const std::filesystem::path cache_path(path_);
    const std::filesystem::path parent = cache_path.parent_path();

    if (!parent.empty()) {
        std::error_code error;
        std::filesystem::create_directories(parent, error);

        if (error) {
            std::cerr << "[ERROR] create cache directory failed: "
                      << error.message() << std::endl;
            return false;
        }
    }

    const std::string temporary_path = path_ + ".tmp";

    {
        std::ofstream output(
            temporary_path,
            std::ios::out | std::ios::trunc
        );

        if (!output.is_open()) {
            std::cerr << "[ERROR] open temporary cache file failed: "
                      << temporary_path << std::endl;
            return false;
        }

        for (const auto& message : messages) {
            output << message.topic
                   << '\t'
                   << message.payload
                   << '\n';
        }

        output.flush();

        if (!output.good()) {
            std::cerr << "[ERROR] write temporary cache file failed"
                      << std::endl;
            return false;
        }
    }

    if (std::rename(temporary_path.c_str(), path_.c_str()) != 0) {
        std::cerr << "[ERROR] replace cache file failed: "
                  << std::strerror(errno) << std::endl;

        std::remove(temporary_path.c_str());
        return false;
    }

    return true;
}

bool FileCache::remove_first(std::size_t count) {
    if (count == 0) {
        return true;
    }

    const auto messages = load_all();

    if (count >= messages.size()) {
        return replace_all({});
    }

    return replace_all(
        std::vector<CachedMessage>(
            messages.begin() + count,
            messages.end()
        )
    );
}

}  // namespace cache
