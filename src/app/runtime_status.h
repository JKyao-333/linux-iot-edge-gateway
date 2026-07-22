#pragma once

#include "app/runtime_metrics.h"

#include <atomic>
#include <cstddef>
#include <string>

namespace app {

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
};

}  // namespace app
