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

struct ModbusConfig {
    bool enabled = false;
    std::string port = "/dev/ttyUSB0";
    int baud_rate = 115200;
    std::uint8_t slave_id = 1;
    std::uint8_t function_code = 3;
    std::uint16_t start_address = 0;
    std::uint16_t register_count = 6;
    int poll_interval_ms = 1000;
    int response_timeout_ms = 500;
};

struct CanConfig {
    bool enabled = false;
    std::string interface = "can0";
    int heartbeat_timeout_seconds = 5;
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

struct HttpConfig {
    bool enabled = false;
    std::string host = "127.0.0.1";
    std::uint16_t port = 8080;
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
    ModbusConfig modbus;
    CanConfig can;
    MqttConfig mqtt;
    TcpConfig tcp;
    HttpConfig http;
    CacheConfig cache;
    LogConfig log;
};

bool load_gateway_config(
    const std::string& file_path,
    GatewayConfig& gateway_config,
    std::string& error_message
);

}  // namespace config
