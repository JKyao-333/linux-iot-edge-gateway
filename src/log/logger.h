#pragma once

#include <fstream>
#include <mutex>
#include <string>

namespace logging {

enum class Level {
    Debug = 0,
    Info,
    Warn,
    Error
};

class Logger {
public:
    explicit Logger(
        std::string file_path,
        Level minimum_level = Level::Debug
    );

    void debug(
        const std::string& module,
        const std::string& message
    );

    void info(
        const std::string& module,
        const std::string& message
    );

    void warn(
        const std::string& module,
        const std::string& message
    );

    void error(
        const std::string& module,
        const std::string& message
    );

private:
    void write(
        Level level,
        const std::string& module,
        const std::string& message
    );

    static const char* level_name(Level level);
    static std::string current_timestamp();

    std::string file_path_;
    Level minimum_level_;
    std::ofstream file_;
    std::mutex mutex_;
};

}  // namespace logging
