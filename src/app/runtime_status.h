#pragma once

#include "app/runtime_metrics.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace app {

struct DeviceHealthStatus {
    std::uint32_t device_id = 0;
    std::string protocol;
    bool online = false;
    std::uint64_t last_update_unix_seconds = 0;
    std::uint64_t error_count = 0;
};

class RuntimeStatus {
public:
    RuntimeStatus(
        std::string version,
        std::string cache_backend,
        bool mqtt_enabled,
        bool tcp_enabled
    );

    void set_config_loaded(bool loaded) noexcept;
    void set_cache_ready(bool ready) noexcept;
    void set_serial_workers_started(
        bool started,
        std::size_t worker_count
    ) noexcept;
    void set_device_counts(std::size_t online, std::size_t offline) noexcept;
    void set_device_statuses(std::vector<DeviceHealthStatus> statuses);

    bool ready() const noexcept;
    std::string health_json(
        const RuntimeMetrics& metrics
    ) const;
    std::string readiness_json() const;

private:
    const std::string version_;
    const std::string cache_backend_;
    const bool mqtt_enabled_;
    const bool tcp_enabled_;
    std::atomic<bool> config_loaded_{false};
    std::atomic<bool> cache_ready_{false};
    std::atomic<bool> serial_workers_started_{false};
    std::atomic<std::size_t> serial_worker_count_{0};
    std::atomic<std::size_t> device_online_count_{0};
    std::atomic<std::size_t> device_offline_count_{0};
    mutable std::mutex device_statuses_mutex_;
    std::vector<DeviceHealthStatus> device_statuses_;
};

}  // namespace app
