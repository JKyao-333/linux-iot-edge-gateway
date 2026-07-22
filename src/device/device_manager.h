#pragma once

#include "device/device_interface.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace device {

class DeviceManager {
public:
    using DataHandler = std::function<void(const app::SensorData&, ProtocolType)>;
    using ErrorHandler = std::function<void(ProtocolType, ReadCode, const std::string&)>;

    DeviceManager(
        std::chrono::seconds reconnect_interval,
        std::chrono::seconds heartbeat_timeout
    );
    ~DeviceManager();

    void add(std::unique_ptr<DeviceInterface> device);
    void start(DataHandler data_handler, ErrorHandler error_handler);
    void stop() noexcept;
    std::vector<DeviceStatus> statuses() const;
    std::size_t online_count() const;
    std::size_t offline_count() const;

private:
    void run_device(DeviceInterface& device);

    std::chrono::seconds reconnect_interval_;
    std::chrono::seconds heartbeat_timeout_;
    std::vector<std::unique_ptr<DeviceInterface>> devices_;
    std::vector<std::thread> workers_;
    DataHandler data_handler_;
    ErrorHandler error_handler_;
    std::atomic<bool> running_{false};
    std::atomic<std::uint64_t> started_at_unix_seconds_{0};
};

}  // namespace device
