#include "app/runtime_metrics.h"

#include <chrono>
#include <sstream>

namespace app {

namespace {

void write_metric(
    std::ostringstream& output,
    const char* name,
    const char* type,
    const char* help,
    std::uint64_t value) {

    output << "# HELP " << name << " " << help << '\n'
           << "# TYPE " << name << " " << type << '\n'
           << name << " " << value << '\n';
}

}  // namespace

RuntimeMetrics::RuntimeMetrics()
    : started_at_seconds_(monotonic_seconds()) {}

std::uint64_t RuntimeMetrics::monotonic_seconds() noexcept {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
}

std::uint64_t RuntimeMetrics::uptime_seconds() const noexcept {
    return monotonic_seconds() - started_at_seconds_;
}

void RuntimeMetrics::set_serial_worker_count(
    std::size_t count) noexcept {

    serial_worker_count_.store(count);
}

void RuntimeMetrics::record_frames_parsed(
    std::size_t count) noexcept {

    frames_parsed_total_.fetch_add(count);
}

void RuntimeMetrics::record_frames_invalid(
    std::size_t count) noexcept {

    frames_invalid_total_.fetch_add(count);
}

void RuntimeMetrics::record_crc_errors(
    std::size_t count) noexcept {

    crc_errors_total_.fetch_add(count);
}

void RuntimeMetrics::record_length_errors(
    std::size_t count) noexcept {

    length_errors_total_.fetch_add(count);
}

void RuntimeMetrics::record_mqtt_publish_success(
    std::size_t count) noexcept {

    mqtt_publish_success_total_.fetch_add(count);
}

void RuntimeMetrics::record_mqtt_publish_failed(
    std::size_t count) noexcept {

    mqtt_publish_failed_total_.fetch_add(count);
}

void RuntimeMetrics::record_tcp_publish_success(
    std::size_t count) noexcept {

    tcp_publish_success_total_.fetch_add(count);
}

void RuntimeMetrics::record_tcp_publish_failed(
    std::size_t count) noexcept {

    tcp_publish_failed_total_.fetch_add(count);
}

void RuntimeMetrics::set_cache_depth(
    std::size_t depth) noexcept {

    cache_depth_.store(depth);
}

void RuntimeMetrics::record_cache_enqueue(
    std::size_t count) noexcept {

    cache_enqueue_total_.fetch_add(count);
}

void RuntimeMetrics::record_cache_flush_attempt(
    std::size_t count) noexcept {

    cache_flush_attempt_total_.fetch_add(count);
}

std::string RuntimeMetrics::render_prometheus() const {
    std::ostringstream output;

    write_metric(
        output,
        "iot_gateway_uptime_seconds",
        "gauge",
        "Seconds since the gateway process initialized runtime metrics.",
        uptime_seconds()
    );
    write_metric(
        output,
        "iot_gateway_serial_worker_count",
        "gauge",
        "Configured serial worker threads.",
        serial_worker_count_.load()
    );
    write_metric(
        output,
        "iot_gateway_frames_parsed_total",
        "counter",
        "Protocol frames accepted by the frame parser.",
        frames_parsed_total_.load()
    );
    write_metric(
        output,
        "iot_gateway_frames_invalid_total",
        "counter",
        "Protocol frames rejected for CRC or length errors.",
        frames_invalid_total_.load()
    );
    write_metric(
        output,
        "iot_gateway_crc_errors_total",
        "counter",
        "Frames rejected by CRC16-Modbus validation.",
        crc_errors_total_.load()
    );
    write_metric(
        output,
        "iot_gateway_length_errors_total",
        "counter",
        "Frames rejected for an invalid payload length.",
        length_errors_total_.load()
    );
    write_metric(
        output,
        "iot_gateway_mqtt_publish_success_total",
        "counter",
        "MQTT publish attempts acknowledged by the client.",
        mqtt_publish_success_total_.load()
    );
    write_metric(
        output,
        "iot_gateway_mqtt_publish_failed_total",
        "counter",
        "MQTT publish attempts that were not accepted by the broker.",
        mqtt_publish_failed_total_.load()
    );
    write_metric(
        output,
        "iot_gateway_tcp_publish_success_total",
        "counter",
        "TCP JSON Lines messages sent successfully.",
        tcp_publish_success_total_.load()
    );
    write_metric(
        output,
        "iot_gateway_tcp_publish_failed_total",
        "counter",
        "TCP JSON Lines send attempts that failed.",
        tcp_publish_failed_total_.load()
    );
    write_metric(
        output,
        "iot_gateway_cache_depth",
        "gauge",
        "Messages currently retained in the MQTT offline cache.",
        cache_depth_.load()
    );
    write_metric(
        output,
        "iot_gateway_cache_enqueue_total",
        "counter",
        "Messages appended to the MQTT offline cache.",
        cache_enqueue_total_.load()
    );
    write_metric(
        output,
        "iot_gateway_cache_flush_attempt_total",
        "counter",
        "Offline cache replay passes attempted.",
        cache_flush_attempt_total_.load()
    );

    return output.str();
}

}  // namespace app
