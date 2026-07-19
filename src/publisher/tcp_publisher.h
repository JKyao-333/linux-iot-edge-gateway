#pragma once

#include "publisher/publisher.h"
#include "tcp/tcp_client.h"

#include <mutex>

namespace publishing {

class TcpPublisher final : public Publisher {
public:
    explicit TcpPublisher(tcp::TcpClient& tcp_client);

    std::string_view channel() const noexcept override;

    PublishStatus publish(
        const std::string& topic,
        const std::string& payload
    ) override;

private:
    tcp::TcpClient& tcp_client_;
    std::mutex mutex_;
};

}  // namespace publishing
