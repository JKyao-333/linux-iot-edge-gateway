#pragma once

#include <string>

namespace mqtt {

class MqttClient {
public:
    MqttClient(std::string host, int port);

    bool publish(const std::string& topic, const std::string& payload) const;

private:
    std::string host_;
    int port_ = 1883;
};

}  // namespace mqtt
