#include "log/logger.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>

namespace logging {

Logger::Logger(
    std::string file_path,
    Level minimum_level)
    : file_path_(std::move(file_path)),
      minimum_level_(minimum_level) {

    const std::filesystem::path path(file_path_);
    const std::filesystem::path parent = path.parent_path();

    if (!parent.empty()) {
        std::error_code error;
        std::filesystem::create_directories(parent, error);

        if (error) {
            std::cerr << "[ERROR] create log directory failed: "
                      << error.message() << std::endl;
        }
    }

    file_.open(file_path_, std::ios::app);

    if (!file_.is_open()) {
        std::cerr << "[ERROR] open log file failed: "
                  << file_path_ << std::endl;
    }
}

void Logger::debug(
    const std::string& module,
    const std::string& message) {

    write(Level::Debug, module, message);
}

void Logger::info(
    const std::string& module,
    const std::string& message) {

    write(Level::Info, module, message);
}

void Logger::warn(
    const std::string& module,
    const std::string& message) {

    write(Level::Warn, module, message);
}

void Logger::error(
    const std::string& module,
    const std::string& message) {

    write(Level::Error, module, message);
}

void Logger::write(
    Level level,
    const std::string& module,
    const std::string& message) {

    if (static_cast<int>(level)
        < static_cast<int>(minimum_level_)) {

        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    std::ostringstream line;
    line << current_timestamp()
         << " [" << level_name(level) << "]"
         << " [" << module << "] "
         << message;

    if (level == Level::Warn || level == Level::Error) {
        std::cerr << line.str() << std::endl;
    } else {
        std::cout << line.str() << std::endl;
    }

    if (file_.is_open()) {
        file_ << line.str() << std::endl;
        file_.flush();
    }
}

const char* Logger::level_name(Level level) {
    switch (level) {
        case Level::Debug:
            return "DEBUG";

        case Level::Info:
            return "INFO";

        case Level::Warn:
            return "WARN";

        case Level::Error:
            return "ERROR";
    }

    return "UNKNOWN";
}

std::string Logger::current_timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);

    std::tm local_time{};
    localtime_r(&time, &local_time);

    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        ) % 1000;

    std::ostringstream timestamp;
    timestamp << std::put_time(
        &local_time,
        "%Y-%m-%d %H:%M:%S"
    );

    timestamp << "."
              << std::setw(3)
              << std::setfill('0')
              << milliseconds.count();

    return timestamp.str();
}

}  // namespace logging
