#include "publisher/tcp_publisher.h"

#include <mutex>

namespace publishing {

TcpPublisher::TcpPublisher(tcp::TcpClient& tcp_client)
    : tcp_client_(tcp_client) {}

std::string_view TcpPublisher::channel() const noexcept {
    return "tcp";
}

PublishStatus TcpPublisher::publish(
    const std::string& topic,
    const std::string& payload) {

    static_cast<void>(topic);
    std::lock_guard<std::mutex> lock(mutex_);

    return tcp_client_.send_json(payload)
        ? PublishStatus::Published
        : PublishStatus::Failed;
}

}  // namespace publishing
