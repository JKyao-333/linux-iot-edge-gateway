#include "app/sensor_data.h"
#include "app/shutdown_signal.h"
#include "cache/file_cache.h"
#include "cache/message_cache.h"
#include "cache/sqlite_cache.h"
#include "config/gateway_config.h"
#include "log/logger.h"
#include "mqtt/mqtt_client.h"
#include "mqtt/reliable_publisher.h"
#include "protocol/frame_parser.h"
#include "publisher/publisher_group.h"
#include "publisher/tcp_publisher.h"
#include "tcp/tcp_client.h"

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <termios.h>
#include <unistd.h>

namespace {

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

void handle_frame(
    const protocol::Frame& frame,
    const std::string& mqtt_topic_prefix,
    publishing::PublisherGroup& publishers,
    logging::Logger& logger) {

    print_frame(frame);

    app::SensorData sensor_data;
    app::parse_sensor_data(frame, sensor_data);

    const std::string json =
        app::sensor_data_to_json(sensor_data);

    if (!sensor_data.valid) {
        logger.warn(
            "sensor",
            "invalid data: " + json
        );

        return;
    }

    logger.info(
        "sensor",
        "parsed data: " + json
    );

    const std::string topic = build_mqtt_topic(
        mqtt_topic_prefix,
        sensor_data
    );

    const auto outcomes = publishers.publish(
        topic,
        json
    );

    for (const auto& outcome : outcomes) {
        const std::string message =
            "publish result="
            + std::string(
                publishing::to_string(outcome.status)
            );

        if (outcome.status
            == publishing::PublishStatus::Failed) {

            logger.warn(outcome.channel, message);
        } else {
            logger.debug(outcome.channel, message);
        }
    }
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

int run_gateway(
    const config::GatewayConfig& gateway_config,
    const std::string& device,
    logging::Logger& logger) {

    const speed_t serial_baud_rate =
        to_termios_baud_rate(
            gateway_config.serial.baud_rate
        );

    const int serial_reconnect_seconds =
        gateway_config.serial.reconnect_interval_seconds;

    const auto serial_reconnect_interval =
        std::chrono::seconds(
            serial_reconnect_seconds
        );

    const std::string reconnect_message =
        "retrying in "
        + std::to_string(serial_reconnect_seconds)
        + " seconds";

    int fd = -1;

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
        return 0;
    }

    logger.info(
        "serial",
        "waiting for raw bytes"
    );

    protocol::FrameParser parser;
    mqtt::MqttClient mqtt_client(
        gateway_config.mqtt.host,
        gateway_config.mqtt.port
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
            close_serial_device(fd);
            return 1;
        }

        message_cache = std::move(sqlite_cache);
    } else {
        message_cache =
            std::make_unique<cache::FileCache>(
                gateway_config.cache.path
            );
    }

    logger.info(
        "cache",
        "backend=" + gateway_config.cache.type
            + ", path=" + gateway_config.cache.path
    );

    mqtt::ReliablePublisher mqtt_publisher(
        mqtt_client,
        *message_cache
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

    logger.info(
        "cache",
        "checking cached mqtt messages"
    );

    mqtt_publisher.flush_cache();

    const auto cache_retry_interval =
        std::chrono::seconds(
            gateway_config.mqtt
                .cache_retry_interval_seconds
        );

    auto last_cache_retry =
        std::chrono::steady_clock::now();

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

        const auto now =
            std::chrono::steady_clock::now();

        if (now - last_cache_retry
            >= cache_retry_interval) {

            mqtt_publisher.flush_cache();
            last_cache_retry = now;
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
            logger.warn(
                "protocol",
                "CRC validation failed, count="
                    + std::to_string(crc_error_count)
            );
        }

        const std::size_t length_error_count =
            parser.take_length_error_count();

        if (length_error_count > 0) {
            logger.warn(
                "protocol",
                "invalid payload length, count="
                    + std::to_string(length_error_count)
            );
        }

        for (const auto& frame : frames) {
            handle_frame(
                frame,
                gateway_config.mqtt.topic_prefix,
                publishers,
                logger
            );
        }
    }

    close_serial_device(fd);
    tcp_client.disconnect();
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
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
        "edge gateway starting"
    );

    const std::string device =
        argc >= 3
            ? argv[2]
            : gateway_config.serial.device;

    const int result = run_gateway(
        gateway_config,
        device,
        logger
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
