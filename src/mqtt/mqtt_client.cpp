#include "mqtt/mqtt_client.h"

#include <mosquitto.h>

#include <climits>
#include <iostream>
#include <utility>

namespace mqtt {

MqttClient::MqttClient(
    std::string host,
    std::uint16_t port)
    : host_(std::move(host)),
      port_(port) {

    initialize();
}

MqttClient::~MqttClient() {
    connected_.store(false);
    acknowledgment_condition_.notify_all();

    if (client_ != nullptr) {
        if (network_loop_started_) {
            mosquitto_disconnect(client_);
            mosquitto_loop_stop(client_, true);
        }

        mosquitto_destroy(client_);
        client_ = nullptr;
    }

    if (library_initialized_) {
        mosquitto_lib_cleanup();
        library_initialized_ = false;
    }
}

bool MqttClient::initialize() {
    int result = mosquitto_lib_init();

    if (result != MOSQ_ERR_SUCCESS) {
        std::cerr
            << "[ERROR] mosquitto library init failed: "
            << mosquitto_strerror(result)
            << std::endl;

        return false;
    }

    library_initialized_ = true;

    client_ = mosquitto_new(
        nullptr,
        true,
        this
    );

    if (client_ == nullptr) {
        std::cerr
            << "[ERROR] mosquitto client creation failed"
            << std::endl;

        return false;
    }

    mosquitto_connect_callback_set(
        client_,
        &MqttClient::on_connect
    );

    mosquitto_disconnect_callback_set(
        client_,
        &MqttClient::on_disconnect
    );

    mosquitto_publish_callback_set(
        client_,
        &MqttClient::on_publish
    );

    result = mosquitto_reconnect_delay_set(
        client_,
        1,
        30,
        true
    );

    if (result != MOSQ_ERR_SUCCESS) {
        std::cerr
            << "[ERROR] MQTT reconnect policy failed: "
            << mosquitto_strerror(result)
            << std::endl;

        return false;
    }

    result = mosquitto_connect_async(
        client_,
        host_.c_str(),
        static_cast<int>(port_),
        60
    );

    if (result != MOSQ_ERR_SUCCESS) {
        std::cerr
            << "[ERROR] MQTT connect setup failed: "
            << mosquitto_strerror(result)
            << std::endl;

        return false;
    }

    result = mosquitto_loop_start(client_);

    if (result != MOSQ_ERR_SUCCESS) {
        std::cerr
            << "[ERROR] MQTT network loop failed: "
            << mosquitto_strerror(result)
            << std::endl;

        return false;
    }

    network_loop_started_ = true;

    std::cout
        << "[INFO] MQTT client initialized, broker="
        << host_
        << ":"
        << port_
        << std::endl;

    return true;
}

bool MqttClient::publish(
    const std::string& topic,
    const std::string& payload) {

    if (client_ == nullptr || !connected_.load()) {
        std::cerr
            << "[WARN] MQTT unavailable, topic="
            << topic
            << std::endl;

        return false;
    }

    if (payload.size() > static_cast<std::size_t>(INT_MAX)) {
        std::cerr
            << "[ERROR] MQTT payload is too large"
            << std::endl;

        return false;
    }

    std::unique_lock<std::mutex> lock(
        acknowledgment_mutex_
    );

    acknowledged_message_ids_.clear();

    int message_id = 0;

    const int result = mosquitto_publish(
        client_,
        &message_id,
        topic.c_str(),
        static_cast<int>(payload.size()),
        payload.data(),
        1,
        false
    );

    if (result != MOSQ_ERR_SUCCESS) {
        std::cerr
            << "[ERROR] MQTT publish failed, topic="
            << topic
            << ", error="
            << mosquitto_strerror(result)
            << std::endl;

        return false;
    }

    acknowledgment_condition_.wait_for(
        lock,
        publish_timeout_,
        [this, message_id]() {
            return !connected_.load()
                || acknowledged_message_ids_.find(
                    message_id
                ) != acknowledged_message_ids_.end();
        }
    );

    const auto acknowledged =
        acknowledged_message_ids_.find(message_id);

    if (acknowledged
        == acknowledged_message_ids_.end()) {

        std::cerr
            << "[WARN] MQTT PUBACK timeout or connection lost, topic="
            << topic
            << std::endl;

        return false;
    }

    acknowledged_message_ids_.erase(acknowledged);

    std::cout
        << "[INFO] MQTT publish acknowledged, topic="
        << topic
        << ", message_id="
        << message_id
        << std::endl;

    return true;
}

bool MqttClient::is_connected() const {
    return connected_.load();
}

void MqttClient::on_connect(
    struct mosquitto* client,
    void* user_data,
    int result_code) {

    static_cast<void>(client);

    auto* mqtt_client =
        static_cast<MqttClient*>(user_data);

    if (mqtt_client == nullptr) {
        return;
    }

    const bool connected = result_code == 0;
    mqtt_client->connected_.store(connected);

    if (connected) {
        std::cout
            << "[INFO] MQTT connected"
            << std::endl;
    } else {
        std::cerr
            << "[WARN] MQTT connection rejected, code="
            << result_code
            << std::endl;
    }
}

void MqttClient::on_disconnect(
    struct mosquitto* client,
    void* user_data,
    int result_code) {

    static_cast<void>(client);

    auto* mqtt_client =
        static_cast<MqttClient*>(user_data);

    if (mqtt_client == nullptr) {
        return;
    }

    mqtt_client->connected_.store(false);
    mqtt_client->acknowledgment_condition_.notify_all();

    if (result_code == 0) {
        std::cout
            << "[INFO] MQTT disconnected"
            << std::endl;
    } else {
        std::cerr
            << "[WARN] MQTT connection lost, code="
            << result_code
            << std::endl;
    }
}

void MqttClient::on_publish(
    struct mosquitto* client,
    void* user_data,
    int message_id) {

    static_cast<void>(client);

    auto* mqtt_client =
        static_cast<MqttClient*>(user_data);

    if (mqtt_client == nullptr) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(
            mqtt_client->acknowledgment_mutex_
        );

        mqtt_client
            ->acknowledged_message_ids_
            .insert(message_id);
    }

    mqtt_client
        ->acknowledgment_condition_
        .notify_all();
}

}  // namespace mqtt
