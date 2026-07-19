#include "publisher/publisher_group.h"

namespace publishing {

void PublisherGroup::add(Publisher& publisher) {
    publishers_.push_back(publisher);
}

std::vector<PublishOutcome> PublisherGroup::publish(
    const std::string& topic,
    const std::string& payload) {

    std::vector<PublishOutcome> outcomes;
    outcomes.reserve(publishers_.size());

    for (Publisher& publisher : publishers_) {
        outcomes.push_back({
            std::string(publisher.channel()),
            publisher.publish(topic, payload)
        });
    }

    return outcomes;
}

std::size_t PublisherGroup::size() const noexcept {
    return publishers_.size();
}

}  // namespace publishing
