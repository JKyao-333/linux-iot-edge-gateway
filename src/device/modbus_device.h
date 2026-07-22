#pragma once

#include "device/device_interface.h"
#include "device/serial_port.h"
#include "modbus/modbus_rtu.h"

#include <mutex>

namespace device {

struct ModbusDeviceOptions {
    std::string port;
    int baud_rate = 115200;
    modbus::ReadRequest request;
    int poll_interval_ms = 1000;
    int response_timeout_ms = 500;
};

class ModbusDevice final : public DeviceInterface {
public:
    explicit ModbusDevice(ModbusDeviceOptions options);

    bool start(std::string& error_message) override;
    void stop() noexcept override;
    DeviceReadResult read(std::chrono::milliseconds timeout) override;
    DeviceStatus get_device_status() const override;

private:
    void record_error();

    ModbusDeviceOptions options_;
    SerialPort port_;
    std::chrono::steady_clock::time_point next_poll_{};
    mutable std::mutex status_mutex_;
    DeviceStatus status_;
};

}  // namespace device
