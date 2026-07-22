#include "http/http_server.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <poll.h>
#include <sstream>
#include <string_view>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

namespace http {

namespace {

const char* reason_phrase(int status_code) {
    switch (status_code) {
        case 200:
            return "OK";
        case 404:
            return "Not Found";
        case 503:
            return "Service Unavailable";
        default:
            return "Internal Server Error";
    }
}

bool send_all(int fd, const std::string& response) {
    std::size_t sent = 0;

    while (sent < response.size()) {
        const ssize_t result = send(
            fd,
            response.data() + sent,
            response.size() - sent,
            MSG_NOSIGNAL
        );

        if (result < 0 && errno == EINTR) {
            continue;
        }

        if (result <= 0) {
            return false;
        }

        sent += static_cast<std::size_t>(result);
    }

    return true;
}

}  // namespace

HttpServer::HttpServer(
    std::string host,
    std::uint16_t port,
    const app::RuntimeStatus& runtime_status,
    const app::RuntimeMetrics& runtime_metrics,
    logging::Logger& logger)
    : host_(std::move(host)),
      port_(port),
      runtime_status_(runtime_status),
      runtime_metrics_(runtime_metrics),
      logger_(logger) {}

HttpServer::~HttpServer() {
    stop();
}

bool HttpServer::start(std::string& error_message) {
    if (running_.load()) {
        error_message = "HTTP server is already running";
        return false;
    }

    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);

    if (listen_fd_ < 0) {
        error_message = "socket failed: "
            + std::string(std::strerror(errno));
        logger_.error("http", error_message);
        return false;
    }

    const int reuse_address = 1;
    setsockopt(
        listen_fd_,
        SOL_SOCKET,
        SO_REUSEADDR,
        &reuse_address,
        sizeof(reuse_address)
    );

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port_);

    if (host_ == "localhost") {
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    } else if (inet_pton(
            AF_INET,
            host_.c_str(),
            &address.sin_addr) != 1) {

        error_message = "invalid HTTP IPv4 host: " + host_;
        logger_.error("http", error_message);
        close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    if (bind(
            listen_fd_,
            reinterpret_cast<const sockaddr*>(&address),
            sizeof(address)) != 0) {

        error_message = "bind failed for " + host_ + ":"
            + std::to_string(port_) + ": "
            + std::string(std::strerror(errno));
        logger_.error("http", error_message);
        close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    if (listen(listen_fd_, 16) != 0) {
        error_message = "listen failed: "
            + std::string(std::strerror(errno));
        logger_.error("http", error_message);
        close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    sockaddr_in bound_address{};
    socklen_t bound_length = sizeof(bound_address);
    if (getsockname(
            listen_fd_,
            reinterpret_cast<sockaddr*>(&bound_address),
            &bound_length) == 0) {

        port_ = ntohs(bound_address.sin_port);
    }

    running_.store(true);

    try {
        worker_ = std::thread(&HttpServer::serve, this);
    } catch (const std::exception& exception) {
        running_.store(false);
        close(listen_fd_);
        listen_fd_ = -1;
        error_message = "HTTP worker start failed: "
            + std::string(exception.what());
        logger_.error("http", error_message);
        return false;
    }

    logger_.info(
        "http",
        "server started: " + host_ + ":"
            + std::to_string(port_)
    );

    error_message.clear();
    return true;
}

void HttpServer::stop() {
    const bool was_running = running_.exchange(false);

    if (listen_fd_ >= 0) {
        shutdown(listen_fd_, SHUT_RDWR);
        close(listen_fd_);
        listen_fd_ = -1;
    }

    if (worker_.joinable()) {
        worker_.join();
    }

    if (was_running) {
        logger_.info("http", "server stopped");
    }
}

std::uint16_t HttpServer::bound_port() const noexcept {
    return port_;
}

void HttpServer::serve() {
    while (running_.load()) {
        const int client_fd = accept(
            listen_fd_,
            nullptr,
            nullptr
        );

        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }

            if (!running_.load()
                || errno == EBADF
                || errno == EINVAL) {

                break;
            }

            logger_.warn(
                "http",
                "accept failed: "
                    + std::string(std::strerror(errno))
            );
            continue;
        }

        handle_client(client_fd);
        close(client_fd);
    }
}

void HttpServer::handle_client(int client_fd) {
    pollfd descriptor{};
    descriptor.fd = client_fd;
    descriptor.events = POLLIN;

    while (running_.load()) {
        const int poll_result = poll(&descriptor, 1, 250);

        if (poll_result > 0) {
            break;
        }

        if (poll_result < 0 && errno != EINTR) {
            return;
        }
    }

    if (!running_.load()) {
        return;
    }

    char request_buffer[4096]{};
    const ssize_t received = recv(
        client_fd,
        request_buffer,
        sizeof(request_buffer) - 1,
        0
    );

    if (received <= 0) {
        return;
    }

    const std::string_view request(
        request_buffer,
        static_cast<std::size_t>(received)
    );
    const std::size_t line_end = request.find("\r\n");
    const std::string_view request_line = request.substr(
        0,
        line_end == std::string_view::npos
            ? request.size()
            : line_end
    );

    std::string path;
    if (request_line.rfind("GET ", 0) == 0) {
        const std::size_t path_end = request_line.find(' ', 4);
        if (path_end != std::string_view::npos) {
            path = std::string(
                request_line.substr(4, path_end - 4)
            );
        }
    }

    logger_.debug(
        "http",
        "request path=" + (path.empty() ? "<invalid>" : path)
    );

    std::string response;

    if (path == "/health") {
        response = build_response(
            200,
            "application/json",
            runtime_status_.health_json(runtime_metrics_)
        );
    } else if (path == "/ready") {
        response = build_response(
            runtime_status_.ready() ? 200 : 503,
            "application/json",
            runtime_status_.readiness_json()
        );
    } else if (path == "/metrics") {
        response = build_response(
            200,
            "text/plain; version=0.0.4; charset=utf-8",
            runtime_metrics_.render_prometheus()
        );
    } else {
        response = build_response(
            404,
            "application/json",
            "{\"status\":\"not_found\"}"
        );
    }

    if (!send_all(client_fd, response)) {
        logger_.debug("http", "response send failed");
    }
}

std::string HttpServer::build_response(
    int status_code,
    const std::string& content_type,
    const std::string& body) const {

    std::ostringstream response;
    response << "HTTP/1.1 " << status_code << " "
             << reason_phrase(status_code) << "\r\n"
             << "Content-Type: " << content_type << "\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n"
             << "Cache-Control: no-store\r\n\r\n"
             << body;

    return response.str();
}

}  // namespace http
