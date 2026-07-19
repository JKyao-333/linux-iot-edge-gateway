#include "publisher/publisher_group.h"

#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace {

class FakePublisher final : public publishing::Publisher {
public:
    FakePublisher(
        std::string name,
        publishing::PublishStatus result)
        : name_(std::move(name)),
          result_(result) {}

    std::string_view channel() const noexcept override {
        return name_;
    }

    publishing::PublishStatus publish(
        const std::string& topic,
        const std::string& payload) override {

        ++publish_count_;
        last_topic_ = topic;
        last_payload_ = payload;
        return result_;
    }

    int publish_count() const noexcept {
        return publish_count_;
    }

    const std::string& last_topic() const noexcept {
        return last_topic_;
    }

    const std::string& last_payload() const noexcept {
        return last_payload_;
    }

private:
    std::string name_;
    publishing::PublishStatus result_;
    int publish_count_ = 0;
    std::string last_topic_;
    std::string last_payload_;
};

}  // namespace

int main() {
    FakePublisher mqtt_publisher(
        "mqtt",
        publishing::PublishStatus::Failed
    );

    FakePublisher tcp_publisher(
        "tcp",
        publishing::PublishStatus::Published
    );

    publishing::PublisherGroup publishers;
    publishers.add(mqtt_publisher);
    publishers.add(tcp_publisher);

    const std::string topic = "sensor/16/data";
    const std::string payload =
        R"({"device_id":16,"valid":true})";

    const auto outcomes = publishers.publish(
        topic,
        payload
    );

    const bool passed =
        publishers.size() == 2
        && outcomes.size() == 2
        && outcomes[0].channel == "mqtt"
        && outcomes[0].status
            == publishing::PublishStatus::Failed
        && outcomes[1].channel == "tcp"
        && outcomes[1].status
            == publishing::PublishStatus::Published
        && mqtt_publisher.publish_count() == 1
        && tcp_publisher.publish_count() == 1
        && mqtt_publisher.last_topic() == topic
        && tcp_publisher.last_topic() == topic
        && mqtt_publisher.last_payload() == payload
        && tcp_publisher.last_payload() == payload;

    std::cout
        << "publisher group outcomes: "
        << outcomes.size()
        << std::endl;

    return passed ? 0 : 1;
}
