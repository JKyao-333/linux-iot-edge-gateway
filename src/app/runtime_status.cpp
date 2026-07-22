#include "app/runtime_status.h"

#include <sstream>
#include <utility>

namespace app {

RuntimeStatus::RuntimeStatus(
    std::string version,
    std::string cache_backend,
    bool mqtt_enabled,
    bool tcp_enabled)
    : version_(std::move(version)),
      cache_backend_(std::move(cache_backend)),
      mqtt_enabled_(mqtt_enabled),
      tcp_enabled_(tcp_enabled) {}

void RuntimeStatus::set_config_loaded(bool loaded) noexcept {
    config_loaded_.store(loaded);
}

void RuntimeStatus::set_cache_ready(bool ready) noexcept {
    cache_ready_.store(ready);
}

void RuntimeStatus::set_serial_workers_started(
    bool started,
    std::size_t worker_count) noexcept {

    serial_worker_count_.store(worker_count);
    serial_workers_started_.store(started);
}

bool RuntimeStatus::ready() const noexcept {
    return config_loaded_.load()
        && cache_ready_.load()
        && serial_workers_started_.load();
}

std::string RuntimeStatus::health_json(
    const RuntimeMetrics& metrics) const {

    std::ostringstream output;
    output << std::boolalpha
           << "{\"status\":\"ok\",\"version\":\""
           << version_
           << "\",\"uptime_seconds\":"
           << metrics.uptime_seconds()
           << ",\"serial_workers\":"
           << serial_worker_count_.load()
           << ",\"mqtt_enabled\":" << mqtt_enabled_
           << ",\"tcp_enabled\":" << tcp_enabled_
           << ",\"cache_backend\":\""
           << cache_backend_
           << "\"}";

    return output.str();
}

std::string RuntimeStatus::readiness_json() const {
    const bool config_loaded = config_loaded_.load();
    const bool cache_ready = cache_ready_.load();
    const bool workers_started =
        serial_workers_started_.load();
    const bool is_ready = ready();

    std::ostringstream output;
    output << std::boolalpha
           << "{\"ready\":" << is_ready
           << ",\"checks\":{\"config_loaded\":"
           << config_loaded
           << ",\"cache_ready\":" << cache_ready
           << ",\"serial_workers_started\":"
           << workers_started
           << "}"
           << ",\"serial_worker_count\":"
           << serial_worker_count_.load()
           << ",\"mqtt_enabled\":" << mqtt_enabled_
           << ",\"tcp_enabled\":" << tcp_enabled_
           << ",\"cache_backend\":\""
           << cache_backend_
           << "\"}";

    return output.str();
}

}  // namespace app
