#pragma once

#include "can/socketcan_device.h"
#include "device/device_interface.h"

#include <mutex>

namespace device {

class CanDevice final : public DeviceInterface {
public:
    explicit CanDevice(std::string interface_name);

    bool start(std::string& error_message) override;
    void stop() noexcept override;
    DeviceReadResult read(std::chrono::milliseconds timeout) override;
    DeviceStatus get_device_status() const override;

private:
    void record_error();

    canbus::SocketCanDevice socketcan_;
    mutable std::mutex status_mutex_;
    DeviceStatus status_;
};

}  // namespace device
