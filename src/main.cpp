#include "app/sensor_data.h"
#include "app/shutdown_signal.h"
#include "app/runtime_metrics.h"
#include "app/runtime_status.h"
#include "cache/file_cache.h"
#include "cache/message_cache.h"
#include "cache/sqlite_cache.h"
#include "config/gateway_config.h"
#include "device/can_device.h"
#include "device/device_manager.h"
#include "device/modbus_device.h"
#include "device/uart_device.h"
#include "log/logger.h"
#include "http/http_server.h"
#include "mqtt/mqtt_client.h"
#include "mqtt/reliable_publisher.h"
#include "protocol/frame_parser.h"
#include "publisher/publisher_group.h"
#include "publisher/tcp_publisher.h"
#include "tcp/tcp_client.h"
#include "version.h"

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <termios.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {

std::mutex console_mutex;

bool configure_serial(int fd, speed_t baud_rate) {
    termios tty{};

    if (tcgetattr(fd, &tty) != 0) {
        std::cerr
            << "tcgetattr failed: "
            << std::strerror(errno)
            << std::endl;

        return false;
    }

    cfmakeraw(&tty);
    cfsetispeed(&tty, baud_rate);
    cfsetospeed(&tty, baud_rate);

    tty.c_cflag |= CLOCAL;
    tty.c_cflag |= CREAD;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 10;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        std::cerr
            << "tcsetattr failed: "
            << std::strerror(errno)
            << std::endl;

        return false;
    }

    return true;
}

speed_t to_termios_baud_rate(int baud_rate) {
    switch (baud_rate) {
        case 9600:
            return B9600;
        case 19200:
            return B19200;
        case 38400:
            return B38400;
        case 57600:
            return B57600;
        case 115200:
            return B115200;
        default:
            return B115200;
    }
}

int open_serial_device(
    const std::string& device,
    speed_t baud_rate,
    logging::Logger& logger) {

    const int fd = open(
        device.c_str(),
        O_RDWR | O_NOCTTY | O_SYNC
    );

    if (fd < 0) {
        logger.warn(
            "serial",
            "open failed: " + device
                + ", error: " + std::strerror(errno)
        );

        return -1;
    }

    if (!configure_serial(fd, baud_rate)) {
        logger.error(
            "serial",
            "configure failed: " + device
        );

        close(fd);
        return -1;
    }

    logger.info(
        "serial",
        "serial opened: " + device
    );

    return fd;
}

void close_serial_device(int& fd) {
    if (fd < 0) {
        return;
    }

    close(fd);
    fd = -1;
}

void print_raw_bytes(
    const unsigned char* buffer,
    ssize_t length) {

    std::lock_guard<std::mutex> lock(console_mutex);

    std::cout << "RX " << length << " bytes: ";

    for (ssize_t index = 0; index < length; ++index) {
        std::cout
            << "0x"
            << std::uppercase
            << std::hex
            << std::setw(2)
            << std::setfill('0')
            << static_cast<int>(buffer[index])
            << " ";
    }

    std::cout << std::dec << std::endl;
}

void print_frame(const protocol::Frame& frame) {
    std::lock_guard<std::mutex> lock(console_mutex);

    std::cout
        << "Parsed frame: cmd=0x"
        << std::uppercase
        << std::hex
        << std::setw(2)
        << std::setfill('0')
        << static_cast<int>(frame.cmd)
        << " device_id=0x"
        << std::setw(2)
        << static_cast<int>(frame.device_id)
        << " payload=";

    for (std::uint8_t byte : frame.payload) {
        std::cout
            << "0x"
            << std::setw(2)
            << static_cast<int>(byte)
            << " ";
    }

    std::cout
        << "crc=0x"
        << std::setw(4)
        << frame.crc
        << std::dec
        << std::endl;
}

std::string build_mqtt_topic(
    const std::string& topic_prefix,
    const app::SensorData& sensor_data) {

    return topic_prefix + "/"
        + std::to_string(
            static_cast<int>(sensor_data.device_id)
        )
        + "/data";
}

void publish_sensor_data(
    const app::SensorData& sensor_data,
    device::ProtocolType protocol,
    const std::string& mqtt_topic_prefix,
    publishing::PublisherGroup& publishers,
    logging::Logger& logger,
    app::RuntimeMetrics& runtime_metrics) {

    const std::string json = app::sensor_data_to_json(sensor_data);
    const std::string protocol_name = device::to_string(protocol);

    if (!sensor_data.valid) {
        logger.warn(protocol_name, "invalid sensor data: " + json);
        runtime_metrics.record_protocol_error();
        return;
    }

    logger.info(
        "sensor",
        "parsed data: " + json + ", protocol=" + protocol_name
    );
    const auto outcomes = publishers.publish(
        build_mqtt_topic(mqtt_topic_prefix, sensor_data),
        json
    );

    for (const auto& outcome : outcomes) {
        const std::string message = "publish result="
            + std::string(publishing::to_string(outcome.status));
        if (outcome.status == publishing::PublishStatus::Failed) {
            logger.warn(outcome.channel, message);
        } else {
            logger.debug(outcome.channel, message);
        }
        if (outcome.channel == "tcp") {
            if (outcome.status == publishing::PublishStatus::Published) {
                runtime_metrics.record_tcp_publish_success();
            } else {
                runtime_metrics.record_tcp_publish_failed();
            }
        }
    }
}

void handle_frame(
    const protocol::Frame& frame,
    const std::string& mqtt_topic_prefix,
    publishing::PublisherGroup& publishers,
    logging::Logger& logger,
    app::RuntimeMetrics& runtime_metrics) {

    print_frame(frame);

    app::SensorData sensor_data;
    app::parse_sensor_data(frame, sensor_data);
    publish_sensor_data(
        sensor_data,
        device::ProtocolType::Uart,
        mqtt_topic_prefix,
        publishers,
        logger,
        runtime_metrics
    );
}

logging::Level parse_log_level(
    const std::string& level) {

    if (level == "INFO") {
        return logging::Level::Info;
    }

    if (level == "WARN") {
        return logging::Level::Warn;
    }

    if (level == "ERROR") {
        return logging::Level::Error;
    }

    return logging::Level::Debug;
}

void run_serial_worker(
    const config::SerialConfig& serial_config,
    const std::string& device,
    const std::string& mqtt_topic_prefix,
    publishing::PublisherGroup& publishers,
    logging::Logger& logger,
    app::RuntimeMetrics& runtime_metrics) {

    const speed_t serial_baud_rate =
        to_termios_baud_rate(
            serial_config.baud_rate
        );

    const int serial_reconnect_seconds =
        serial_config.reconnect_interval_seconds;

    const auto serial_reconnect_interval =
        std::chrono::seconds(
            serial_reconnect_seconds
        );

    const std::string reconnect_message =
        "retrying in "
        + std::to_string(serial_reconnect_seconds)
        + " seconds";

    int fd = -1;

    logger.info(
        "serial",
        "worker starting: " + device
    );

    while (fd < 0 && !app::shutdown_requested()) {
        fd = open_serial_device(
            device,
            serial_baud_rate,
            logger
        );

        if (fd < 0) {
            logger.warn(
                "serial",
                reconnect_message
            );

            app::wait_for_shutdown(
                serial_reconnect_interval
            );
        }
    }

    if (app::shutdown_requested()) {
        close_serial_device(fd);
        return;
    }

    logger.info(
        "serial",
        "waiting for raw bytes"
    );

    protocol::FrameParser parser;
    unsigned char buffer[256];

    while (!app::shutdown_requested()) {
        const ssize_t received = read(
            fd,
            buffer,
            sizeof(buffer)
        );

        if (received < 0) {
            if (errno == EINTR
                && app::shutdown_requested()) {

                break;
            }

            logger.warn(
                "serial",
                "read failed: "
                    + std::string(std::strerror(errno))
            );

            close_serial_device(fd);
            parser.reset();

            while (fd < 0
                   && !app::shutdown_requested()) {

                logger.warn(
                    "serial",
                    "connection lost, "
                        + reconnect_message
                );

                if (app::wait_for_shutdown(
                        serial_reconnect_interval)) {

                    break;
                }

                fd = open_serial_device(
                    device,
                    serial_baud_rate,
                    logger
                );
            }

            if (app::shutdown_requested()) {
                break;
            }

            logger.info(
                "serial",
                "serial connection restored"
            );

            continue;
        }

        if (received == 0) {
            continue;
        }

        print_raw_bytes(buffer, received);

        const auto frames = parser.feed(
            buffer,
            static_cast<std::size_t>(received)
        );

        const std::size_t crc_error_count =
            parser.take_crc_error_count();

        if (crc_error_count > 0) {
            runtime_metrics.record_crc_errors(
                crc_error_count
            );
            runtime_metrics.record_frames_invalid(
                crc_error_count
            );

            logger.warn(
                "protocol",
                "CRC validation failed, count="
                    + std::to_string(crc_error_count)
            );
        }

        const std::size_t length_error_count =
            parser.take_length_error_count();

        if (length_error_count > 0) {
            runtime_metrics.record_length_errors(
                length_error_count
            );
            runtime_metrics.record_frames_invalid(
                length_error_count
            );

            logger.warn(
                "protocol",
                "invalid payload length, count="
                    + std::to_string(length_error_count)
            );
        }

        const std::size_t overflow_byte_count =
            parser.take_overflow_byte_count();

        if (overflow_byte_count > 0) {
            logger.error(
                "protocol",
                "ring buffer overflow, dropped_bytes="
                    + std::to_string(overflow_byte_count)
            );
        }

        runtime_metrics.record_frames_parsed(
            frames.size()
        );

        for (const auto& frame : frames) {
            handle_frame(
                frame,
                mqtt_topic_prefix,
                publishers,
                logger,
                runtime_metrics
            );
        }
    }

    close_serial_device(fd);

    logger.info(
        "serial",
        "worker stopped: " + device
    );
}

int run_gateway(
    const config::GatewayConfig& gateway_config,
    const std::vector<std::string>& devices,
    logging::Logger& logger,
    app::RuntimeStatus& runtime_status,
    app::RuntimeMetrics& runtime_metrics) {

    mqtt::MqttClientOptions mqtt_options;
    mqtt_options.username = gateway_config.mqtt.username;
    mqtt_options.password = gateway_config.mqtt.password;
    mqtt_options.tls_enabled =
        gateway_config.mqtt.tls.enabled;
    mqtt_options.ca_file = gateway_config.mqtt.tls.ca_file;
    mqtt_options.certificate_file =
        gateway_config.mqtt.tls.certificate_file;
    mqtt_options.private_key_file =
        gateway_config.mqtt.tls.private_key_file;
    mqtt_options.tls_insecure =
        gateway_config.mqtt.tls.insecure;

    mqtt::MqttClient mqtt_client(
        gateway_config.mqtt.host,
        gateway_config.mqtt.port,
        std::move(mqtt_options)
    );

    std::unique_ptr<cache::MessageCache> message_cache;

    if (gateway_config.cache.type == "sqlite") {
        auto sqlite_cache =
            std::make_unique<cache::SqliteCache>(
                gateway_config.cache.path
            );

        if (!sqlite_cache->is_ready()) {
            logger.error(
                "cache",
                sqlite_cache->error_message()
            );

            return 1;
        }

        message_cache = std::move(sqlite_cache);
    } else {
        message_cache =
            std::make_unique<cache::FileCache>(
                gateway_config.cache.path
            );
    }

    runtime_status.set_cache_ready(true);

    logger.info(
        "cache",
        "backend=" + gateway_config.cache.type
            + ", path=" + gateway_config.cache.path
    );

    mqtt::ReliablePublisher mqtt_publisher(
        mqtt_client,
        *message_cache,
        &runtime_metrics
    );

    tcp::TcpClient tcp_client(
        gateway_config.tcp.host,
        gateway_config.tcp.port
    );

    publishing::TcpPublisher tcp_publisher(tcp_client);
    publishing::PublisherGroup publishers;

    publishers.add(mqtt_publisher);

    if (gateway_config.tcp.enabled) {
        publishers.add(tcp_publisher);
    }

    logger.info(
        "publisher",
        "enabled channels="
            + std::to_string(publishers.size())
    );

    std::unique_ptr<http::HttpServer> http_server;

    if (gateway_config.http.enabled) {
        http_server = std::make_unique<http::HttpServer>(
            gateway_config.http.host,
            gateway_config.http.port,
            runtime_status,
            runtime_metrics,
            logger
        );

        std::string http_error;
        if (!http_server->start(http_error)) {
            return 1;
        }
    }

    logger.info(
        "cache",
        "checking cached mqtt messages"
    );

    mqtt_publisher.flush_cache();

    device::DeviceManager device_manager(
        std::chrono::seconds(
            gateway_config.serial.reconnect_interval_seconds
        ),
        std::chrono::seconds(
            gateway_config.can.heartbeat_timeout_seconds
        )
    );

    for (const auto& serial_device : devices) {
        device_manager.add(std::make_unique<device::UartDevice>(
            serial_device,
            gateway_config.serial.baud_rate
        ));
    }

    if (gateway_config.modbus.enabled) {
        device::ModbusDeviceOptions options;
        options.port = gateway_config.modbus.port;
        options.baud_rate = gateway_config.modbus.baud_rate;
        options.request.slave_id = gateway_config.modbus.slave_id;
        options.request.function = gateway_config.modbus.function_code == 4
            ? modbus::FunctionCode::ReadInputRegisters
            : modbus::FunctionCode::ReadHoldingRegisters;
        options.request.start_address = gateway_config.modbus.start_address;
        options.request.register_count = gateway_config.modbus.register_count;
        options.poll_interval_ms = gateway_config.modbus.poll_interval_ms;
        options.response_timeout_ms = gateway_config.modbus.response_timeout_ms;
        device_manager.add(std::make_unique<device::ModbusDevice>(options));
    }

    if (gateway_config.can.enabled) {
        device_manager.add(std::make_unique<device::CanDevice>(
            gateway_config.can.interface
        ));
    }

    device_manager.start(
        [&](const app::SensorData& data, device::ProtocolType protocol) {
            runtime_metrics.record_frames_parsed();
            publish_sensor_data(
                data,
                protocol,
                gateway_config.mqtt.topic_prefix,
                publishers,
                logger,
                runtime_metrics
            );
        },
        [&](device::ProtocolType protocol,
            device::ReadCode,
            const std::string& error) {
            runtime_metrics.record_protocol_error();
            if (protocol == device::ProtocolType::ModbusRtu) {
                runtime_metrics.record_modbus_error();
            } else if (protocol == device::ProtocolType::SocketCan) {
                runtime_metrics.record_can_error();
            }
            logger.warn(device::to_string(protocol), error);
        }
    );

    if (!devices.empty()) {
        for (int attempt = 0; attempt < 20; ++attempt) {
            std::size_t uart_online = 0;
            for (const auto& status : device_manager.statuses()) {
                if (status.protocol == device::ProtocolType::Uart
                    && status.online) {
                    ++uart_online;
                }
            }
            if (uart_online == devices.size()) {
                for (std::size_t index = 0; index < uart_online; ++index) {
                    logger.info("serial", "serial opened by device manager");
                }
                logger.info("serial", "waiting for raw bytes");
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    const std::size_t input_count = devices.size()
        + (gateway_config.modbus.enabled ? 1U : 0U)
        + (gateway_config.can.enabled ? 1U : 0U);
    logger.info("device", "input count=" + std::to_string(input_count));
    logger.info("serial", "worker count=" + std::to_string(devices.size()));

    const auto update_device_observability = [&] {
        const auto statuses = device_manager.statuses();
        std::size_t online = 0;
        std::vector<app::DeviceHealthStatus> health_statuses;
        health_statuses.reserve(statuses.size());
        for (const auto& status : statuses) {
            if (status.online) {
                ++online;
            }
            health_statuses.push_back({
                status.device_id,
                device::to_string(status.protocol),
                status.online,
                status.last_update_unix_seconds,
                status.error_count
            });
        }
        const std::size_t offline = statuses.size() - online;
        runtime_metrics.set_device_counts(online, offline);
        runtime_status.set_device_counts(online, offline);
        runtime_status.set_device_statuses(std::move(health_statuses));
    };
    update_device_observability();

    runtime_metrics.set_serial_worker_count(
        devices.size()
    );
    runtime_status.set_serial_workers_started(
        true,
        devices.size()
    );

    const auto cache_retry_interval =
        std::chrono::seconds(
            gateway_config.mqtt
                .cache_retry_interval_seconds
        );

    while (!app::wait_for_shutdown(
        cache_retry_interval)) {

        mqtt_publisher.flush_cache();
        update_device_observability();
    }

    runtime_status.set_serial_workers_started(
        false,
        devices.size()
    );

    if (http_server != nullptr) {
        http_server->stop();
    }

    device_manager.stop();

    tcp_client.disconnect();
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string(argv[1]) == "--version") {
        std::cout
            << "linux-iot-edge-gateway "
            << app::kVersion
            << std::endl;

        return 0;
    }

    const std::string config_path =
        argc >= 2
            ? argv[1]
            : "config/gateway.yaml";

    config::GatewayConfig gateway_config;
    std::string error_message;

    if (!config::load_gateway_config(
            config_path,
            gateway_config,
            error_message)) {

        std::cerr
            << "load config failed: "
            << error_message
            << std::endl;

        return 1;
    }

    logging::Logger logger(
        gateway_config.log.path,
        parse_log_level(gateway_config.log.level)
    );

    app::RuntimeMetrics runtime_metrics;
    app::RuntimeStatus runtime_status(
        std::string(app::kVersion),
        gateway_config.cache.type,
        true,
        gateway_config.tcp.enabled
    );
    runtime_status.set_config_loaded(true);

    if (!app::install_shutdown_signal_handlers(
            error_message)) {

        logger.error(
            "app",
            error_message
        );

        return 1;
    }

    logger.info(
        "app",
        "edge gateway starting, version="
            + std::string(app::kVersion)
    );

    const std::vector<std::string> devices =
        argc >= 3
            ? std::vector<std::string>{argv[2]}
            : gateway_config.serial.devices;

    const int result = run_gateway(
        gateway_config,
        devices,
        logger,
        runtime_status,
        runtime_metrics
    );

    if (app::shutdown_requested()) {
        logger.info(
            "app",
            "shutdown requested"
        );
    }

    if (result == 0) {
        logger.info(
            "app",
            "edge gateway stopped"
        );
    }

    return result;
}
