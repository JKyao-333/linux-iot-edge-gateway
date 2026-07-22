#pragma once

#include "device/device_interface.h"
#include "device/serial_port.h"
#include "protocol/frame_parser.h"

#include <deque>
#include <mutex>

namespace device {

class UartDevice final : public DeviceInterface {
public:
    UartDevice(std::string path, int baud_rate);

    bool start(std::string& error_message) override;
    void stop() noexcept override;
    DeviceReadResult read(std::chrono::milliseconds timeout) override;
    DeviceStatus get_device_status() const override;

private:
    void record_errors(std::size_t count = 1);

    std::string path_;
    int baud_rate_;
    SerialPort port_;
    protocol::FrameParser parser_;
    std::deque<app::SensorData> pending_;
    mutable std::mutex status_mutex_;
    DeviceStatus status_;
};

}  // namespace device
