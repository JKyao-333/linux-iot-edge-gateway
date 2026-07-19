#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_set>

struct mosquitto;

namespace mqtt {

struct MqttClientOptions {
    std::string username;
    std::string password;
    bool tls_enabled = false;
    std::string ca_file;
    std::string certificate_file;
    std::string private_key_file;
    bool tls_insecure = false;
};

class MqttClient {
public:
    MqttClient(
        std::string host,
        std::uint16_t port,
        MqttClientOptions options = {}
    );

    ~MqttClient();

    MqttClient(const MqttClient&) = delete;
    MqttClient& operator=(const MqttClient&) = delete;

    bool publish(
        const std::string& topic,
        const std::string& payload
    );

    bool is_connected() const;

private:
    static void on_connect(
        struct mosquitto* client,
        void* user_data,
        int result_code
    );

    static void on_disconnect(
        struct mosquitto* client,
        void* user_data,
        int result_code
    );

    static void on_publish(
        struct mosquitto* client,
        void* user_data,
        int message_id
    );

    bool initialize();
    bool configure_security();

    std::string host_;
    std::uint16_t port_ = 1883;
    MqttClientOptions options_;

    struct mosquitto* client_ = nullptr;

    bool library_initialized_ = false;
    bool network_loop_started_ = false;

    std::atomic<bool> connected_{false};

    std::mutex acknowledgment_mutex_;
    std::condition_variable acknowledgment_condition_;
    std::unordered_set<int> acknowledged_message_ids_;

    const std::chrono::milliseconds
        publish_timeout_{2000};
};

}  // namespace mqtt
