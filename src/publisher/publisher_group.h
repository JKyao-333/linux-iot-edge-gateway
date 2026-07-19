#pragma once

#include "publisher/publisher.h"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace publishing {

struct PublishOutcome {
    std::string channel;
    PublishStatus status = PublishStatus::Failed;
};

class PublisherGroup {
public:
    void add(Publisher& publisher);

    std::vector<PublishOutcome> publish(
        const std::string& topic,
        const std::string& payload
    );

    std::size_t size() const noexcept;

private:
    std::vector<std::reference_wrapper<Publisher>> publishers_;
};

}  // namespace publishing
