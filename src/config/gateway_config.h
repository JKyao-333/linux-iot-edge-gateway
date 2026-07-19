#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace config {

struct SerialConfig {
    std::vector<std::string> devices = {
        "/tmp/tty_gateway"
    };
    int baud_rate = 115200;
    int reconnect_interval_seconds = 2;
};

struct MqttTlsConfig {
    bool enabled = false;
    std::string ca_file;
    std::string certificate_file;
    std::string private_key_file;
    bool insecure = false;
};

struct MqttConfig {
    std::string host = "localhost";
    std::uint16_t port = 1883;
    std::string topic_prefix = "sensor";
    int cache_retry_interval_seconds = 5;
    std::string username;
    std::string password;
    MqttTlsConfig tls;
};

struct TcpConfig {
    bool enabled = true;
    std::string host = "127.0.0.1";
    std::uint16_t port = 9000;
};

struct CacheConfig {
    std::string type = "sqlite";
    std::string path = "data/pending_messages.db";
};

struct LogConfig {
    std::string path = "logs/gateway.log";
    std::string level = "DEBUG";
};

struct GatewayConfig {
    SerialConfig serial;
    MqttConfig mqtt;
    TcpConfig tcp;
    CacheConfig cache;
    LogConfig log;
};

bool load_gateway_config(
    const std::string& file_path,
    GatewayConfig& gateway_config,
    std::string& error_message
);

}  // namespace config
