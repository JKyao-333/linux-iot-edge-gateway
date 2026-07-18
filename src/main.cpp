#include "app/sensor_data.h"
#include "protocol/frame_parser.h"
#include "mqtt/mqtt_client.h"
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include "cache/file_cache.h"
#include "mqtt/reliable_publisher.h"
#include <cerrno>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include "log/logger.h"
#include <chrono>
#include "tcp/tcp_client.h"
#include <thread>
static bool configure_serial(int fd, speed_t baudrate) {
    termios tty{};

    if (tcgetattr(fd, &tty) != 0) {
        std::cerr << "tcgetattr failed: " << std::strerror(errno) << std::endl;
        return false;
    }

    cfmakeraw(&tty);
    cfsetispeed(&tty, baudrate);
    cfsetospeed(&tty, baudrate);

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
        std::cerr << "tcsetattr failed: " << std::strerror(errno) << std::endl;
        return false;
    }

    return true;
}
static int open_serial_device(
    const std::string& device,
    logging::Logger& logger
) {
    int fd = open(
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

    if (!configure_serial(fd, B115200)) {
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
static void print_raw_bytes(const unsigned char* buffer, ssize_t length) {
    std::cout << "RX " << length << " bytes: ";

    for (ssize_t i = 0; i < length; ++i) {
        std::cout << "0x"
                  << std::uppercase
                  << std::hex
                  << std::setw(2)
                  << std::setfill('0')
                  << static_cast<int>(buffer[i])
                  << " ";
    }

    std::cout << std::dec << std::endl;
}

static void print_frame(const protocol::Frame& frame) {
    std::cout << "Parsed frame: cmd=0x"
              << std::uppercase << std::hex << std::setw(2)
              << std::setfill('0') << static_cast<int>(frame.cmd)
              << " device_id=0x"
              << std::setw(2) << static_cast<int>(frame.device_id)
              << " payload=";

    for (uint8_t byte : frame.payload) {
        std::cout << "0x"
                  << std::setw(2)
                  << static_cast<int>(byte)
                  << " ";
    }

    std::cout << "crc=0x"
              << std::setw(4)
              << frame.crc
              << std::dec
              << std::endl;
}
static std::string build_mqtt_topic(const app::SensorData& sensor_data) {
    return "sensor/" + std::to_string(static_cast<int>(sensor_data.device_id)) + "/data";
}
static void handle_frame(
    const protocol::Frame& frame,
    mqtt::ReliablePublisher& publisher,
    tcp::TcpClient& tcp_client,
    logging::Logger& logger) {
    print_frame(frame);

    app::SensorData sensor_data;
    app::parse_sensor_data(frame, sensor_data);

    const std::string json = app::sensor_data_to_json(sensor_data);

    if (sensor_data.valid) {
        logger.info(
    "sensor",
    "parsed data: " + json
);

        const std::string topic = build_mqtt_topic(sensor_data);
        publisher.publish(topic, json);
           if (!tcp_client.send_json(json)) {
            logger.warn(
                "tcp",
                "TCP publish failed"
            );
        }
     } else {
        logger.warn(
    "sensor",
    "invalid data: " + json
);
    }
}
int main(int argc, char* argv[]) {
   logging::Logger logger(
    "logs/gateway.log",
    logging::Level::Debug
);

logger.info(
    "app",
    "edge gateway starting"
);

    if (argc < 2) {
        std::cerr << "Usage: edge_gateway <serial_device>" << std::endl;
        std::cerr << "Example: ./edge_gateway /tmp/tty_gateway" << std::endl;
        return 1;
    }

    std::string device = argv[1];
    int fd = -1;

    while (fd < 0) {
        fd = open_serial_device(device, logger);

        if (fd < 0) {
            logger.warn(
                "serial",
                "retrying in 2 seconds"
            );

            std::this_thread::sleep_for(
                std::chrono::seconds(2)
            );
        }
    }
    logger.info(
    "serial",
    "serial opened: " + device
);

logger.info(
    "serial",
    "waiting for raw bytes"
);
    protocol::FrameParser parser;
    mqtt::MqttClient mqtt_client("localhost", 1883);
    cache::FileCache file_cache(
    "data/pending_messages.cache"
);

mqtt::ReliablePublisher publisher(
    mqtt_client,
    file_cache
);
tcp::TcpClient tcp_client(
    "127.0.0.1",
    9000
);
logger.info(
    "cache",
    "checking cached mqtt messages"
);

publisher.flush_cache();
    unsigned char buffer[256];
auto last_cache_retry =
    std::chrono::steady_clock::now();
    while (true) {
        ssize_t n = read(fd, buffer, sizeof(buffer));
        if (n < 0) {
            const std::string error_message =
                std::strerror(errno);

            logger.warn(
                "serial",
                "read failed: " + error_message
            );

            close(fd);
            fd = -1;

            // 丢弃断线前尚未完成的半帧。
            parser.reset();

            while (fd < 0) {
                logger.warn(
                    "serial",
                    "connection lost, retrying in 2 seconds"
                );

                std::this_thread::sleep_for(
                    std::chrono::seconds(2)
                );

                fd = open_serial_device(
                    device,
                    logger
                );
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
    >= std::chrono::seconds(5)) {

    publisher.flush_cache();
    last_cache_retry = now;
}

        if (n == 0) {
            continue;
        }

        print_raw_bytes(buffer, n);

        auto frames = parser.feed(buffer, static_cast<std::size_t>(n));
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
    publisher,
    tcp_client,
    logger
);
        }
    }

    close(fd);
    return 0;
}
