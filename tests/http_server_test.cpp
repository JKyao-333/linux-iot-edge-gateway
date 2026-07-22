#include "app/runtime_metrics.h"
#include "app/runtime_status.h"
#include "http/http_server.h"
#include "log/logger.h"

#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace {

std::string request(std::uint16_t port, const std::string& path) {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return {};
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

    if (connect(
            fd,
            reinterpret_cast<const sockaddr*>(&address),
            sizeof(address)) != 0) {

        close(fd);
        return {};
    }

    const std::string http_request =
        "GET " + path + " HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: close\r\n\r\n";

    if (send(
            fd,
            http_request.data(),
            http_request.size(),
            0) < 0) {

        close(fd);
        return {};
    }

    std::string response;
    char buffer[2048];
    while (true) {
        const ssize_t received = recv(
            fd,
            buffer,
            sizeof(buffer),
            0
        );

        if (received <= 0) {
            break;
        }

        response.append(
            buffer,
            static_cast<std::size_t>(received)
        );
    }

    close(fd);
    return response;
}

bool contains(
    const std::string& value,
    const std::string& expected) {

    return value.find(expected) != std::string::npos;
}

}  // namespace

int main() {
    logging::Logger logger(
        "/tmp/http_server_test.log",
        logging::Level::Error
    );
    app::RuntimeMetrics metrics;
    app::RuntimeStatus status("test-version", "sqlite", true, true);
    status.set_config_loaded(true);
    status.set_cache_ready(true);

    http::HttpServer server(
        "127.0.0.1",
        0,
        status,
        metrics,
        logger
    );

    std::string error_message;
    if (!server.start(error_message)) {
        std::cerr << error_message << std::endl;
        return 1;
    }

    const std::uint16_t port = server.bound_port();
    const std::string health = request(port, "/health");
    const std::string not_ready = request(port, "/ready");

    status.set_serial_workers_started(true, 1);
    metrics.set_serial_worker_count(1);

    const std::string ready = request(port, "/ready");
    const std::string prometheus = request(port, "/metrics");
    const std::string missing = request(port, "/missing");

    http::HttpServer conflicting_server(
        "127.0.0.1",
        port,
        status,
        metrics,
        logger
    );
    std::string conflict_error;
    const bool conflict_started =
        conflicting_server.start(conflict_error);

    const int idle_fd = socket(AF_INET, SOCK_STREAM, 0);
    bool idle_connected = false;
    if (idle_fd >= 0) {
        sockaddr_in idle_address{};
        idle_address.sin_family = AF_INET;
        idle_address.sin_port = htons(port);
        inet_pton(
            AF_INET,
            "127.0.0.1",
            &idle_address.sin_addr
        );
        idle_connected = connect(
            idle_fd,
            reinterpret_cast<const sockaddr*>(&idle_address),
            sizeof(idle_address)
        ) == 0;
    }

    std::this_thread::sleep_for(
        std::chrono::milliseconds(20)
    );
    const auto stop_started =
        std::chrono::steady_clock::now();
    server.stop();
    const auto stop_elapsed =
        std::chrono::steady_clock::now() - stop_started;

    if (idle_fd >= 0) {
        close(idle_fd);
    }

    const bool ok =
        contains(health, "HTTP/1.1 200 OK")
        && contains(health, "\"status\":\"ok\"")
        && contains(health, "\"version\":\"test-version\"")
        && contains(health, "\"cache_backend\":\"sqlite\"")
        && contains(not_ready, "HTTP/1.1 503")
        && contains(not_ready, "\"ready\":false")
        && contains(ready, "HTTP/1.1 200 OK")
        && contains(ready, "\"ready\":true")
        && contains(ready, "\"checks\":{")
        && contains(prometheus, "HTTP/1.1 200 OK")
        && contains(
            prometheus,
            "iot_gateway_uptime_seconds"
        )
        && contains(missing, "HTTP/1.1 404")
        && !conflict_started
        && contains(conflict_error, "bind failed")
        && idle_connected
        && stop_elapsed < std::chrono::seconds(2);

    if (!ok) {
        std::cerr << "HTTP endpoint test failed" << std::endl;
        return 1;
    }

    std::cout << "HTTP server test passed" << std::endl;
    return 0;
}
