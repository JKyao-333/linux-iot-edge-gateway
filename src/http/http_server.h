#pragma once

#include "app/runtime_metrics.h"
#include "app/runtime_status.h"
#include "log/logger.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

namespace http {

class HttpServer {
public:
    HttpServer(
        std::string host,
        std::uint16_t port,
        const app::RuntimeStatus& runtime_status,
        const app::RuntimeMetrics& runtime_metrics,
        logging::Logger& logger
    );

    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    bool start(std::string& error_message);
    void stop();
    std::uint16_t bound_port() const noexcept;

private:
    void serve();
    void handle_client(int client_fd);
    std::string build_response(
        int status_code,
        const std::string& content_type,
        const std::string& body
    ) const;

    std::string host_;
    std::uint16_t port_;
    const app::RuntimeStatus& runtime_status_;
    const app::RuntimeMetrics& runtime_metrics_;
    logging::Logger& logger_;
    std::atomic<bool> running_{false};
    int listen_fd_ = -1;
    std::thread worker_;
};

}  // namespace http
