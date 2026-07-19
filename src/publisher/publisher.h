#pragma once

#include <string>
#include <string_view>

namespace publishing {

enum class PublishStatus {
    Published,
    Deferred,
    Failed
};

inline const char* to_string(PublishStatus status) {
    switch (status) {
        case PublishStatus::Published:
            return "published";

        case PublishStatus::Deferred:
            return "deferred";

        case PublishStatus::Failed:
            return "failed";
    }

    return "unknown";
}

class Publisher {
public:
    virtual ~Publisher() = default;

    virtual std::string_view channel() const noexcept = 0;

    virtual PublishStatus publish(
        const std::string& topic,
        const std::string& payload
    ) = 0;
};

}  // namespace publishing
