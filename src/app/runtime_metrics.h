#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>

namespace app {

class RuntimeMetrics {
public:
    RuntimeMetrics();

    std::uint64_t uptime_seconds() const noexcept;

    void set_serial_worker_count(std::size_t count) noexcept;
    void record_frames_parsed(std::size_t count = 1) noexcept;
    void record_frames_invalid(std::size_t count = 1) noexcept;
    void record_crc_errors(std::size_t count = 1) noexcept;
    void record_length_errors(std::size_t count = 1) noexcept;
    void record_mqtt_publish_success(std::size_t count = 1) noexcept;
    void record_mqtt_publish_failed(std::size_t count = 1) noexcept;
    void record_tcp_publish_success(std::size_t count = 1) noexcept;
    void record_tcp_publish_failed(std::size_t count = 1) noexcept;
    void set_cache_depth(std::size_t depth) noexcept;
    void record_cache_enqueue(std::size_t count = 1) noexcept;
    void record_cache_flush_attempt(std::size_t count = 1) noexcept;
    void set_device_counts(std::size_t online, std::size_t offline) noexcept;
    void record_protocol_error(std::size_t count = 1) noexcept;
    void record_modbus_error(std::size_t count = 1) noexcept;
    void record_can_error(std::size_t count = 1) noexcept;

    std::string render_prometheus() const;

private:
    static std::uint64_t monotonic_seconds() noexcept;

    const std::uint64_t started_at_seconds_;
    std::atomic<std::uint64_t> serial_worker_count_{0};
    std::atomic<std::uint64_t> frames_parsed_total_{0};
    std::atomic<std::uint64_t> frames_invalid_total_{0};
    std::atomic<std::uint64_t> crc_errors_total_{0};
    std::atomic<std::uint64_t> length_errors_total_{0};
    std::atomic<std::uint64_t> mqtt_publish_success_total_{0};
    std::atomic<std::uint64_t> mqtt_publish_failed_total_{0};
    std::atomic<std::uint64_t> tcp_publish_success_total_{0};
    std::atomic<std::uint64_t> tcp_publish_failed_total_{0};
    std::atomic<std::uint64_t> cache_depth_{0};
    std::atomic<std::uint64_t> cache_enqueue_total_{0};
    std::atomic<std::uint64_t> cache_flush_attempt_total_{0};
    std::atomic<std::uint64_t> device_online_total_{0};
    std::atomic<std::uint64_t> device_offline_total_{0};
    std::atomic<std::uint64_t> protocol_error_total_{0};
    std::atomic<std::uint64_t> modbus_error_total_{0};
    std::atomic<std::uint64_t> can_error_total_{0};
};

}  // namespace app
