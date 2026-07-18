#include "mqtt/mqtt_client.h"

#include <cstdlib>
#include <iostream>
#include <sstream>

namespace mqtt {

namespace {

std::string shell_quote(const std::string& value) {
    std::string quoted = "'";

    for (char ch : value) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted += ch;
        }
    }

    quoted += "'";
    return quoted;
}

}  // namespace

MqttClient::MqttClient(std::string host, int port)
    : host_(std::move(host)), port_(port) {}

bool MqttClient::publish(const std::string& topic, const std::string& payload) const {
    std::ostringstream command;
    command << "mosquitto_pub"
            << " -h " << shell_quote(host_)
            << " -p " << port_
            << " -t " << shell_quote(topic)
            << " -m " << shell_quote(payload);

    const int ret = std::system(command.str().c_str());

    if (ret != 0) {
        std::cerr << "[ERROR] mqtt publish failed, topic=" << topic
                  << ", exit_code=" << ret << std::endl;
        return false;
    }

    std::cout << "[INFO] mqtt publish ok, topic=" << topic << std::endl;
    return true;
}

}  // namespace mqtt
