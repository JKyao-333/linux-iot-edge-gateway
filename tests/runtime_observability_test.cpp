#include "app/runtime_metrics.h"
#include "app/runtime_status.h"

#include <iostream>
#include <string>

int main() {
    app::RuntimeMetrics metrics;
    app::RuntimeStatus status("test-version", "sqlite", true, true);

    status.set_config_loaded(true);
    status.set_cache_ready(true);
    status.set_serial_workers_started(true, 2);

    metrics.set_serial_worker_count(2);
    metrics.record_frames_parsed(4);
    metrics.record_frames_invalid(2);
    metrics.record_crc_errors();
    metrics.record_length_errors();
    metrics.record_mqtt_publish_success(3);
    metrics.record_mqtt_publish_failed();
    metrics.record_tcp_publish_success(2);
    metrics.record_tcp_publish_failed();
    metrics.set_cache_depth(5);
    metrics.record_cache_enqueue(5);
    metrics.record_cache_flush_attempt(2);

    const std::string text = metrics.render_prometheus();
    const bool metrics_are_present =
        text.find("iot_gateway_frames_parsed_total 4")
            != std::string::npos
        && text.find("iot_gateway_crc_errors_total 1")
            != std::string::npos
        && text.find("iot_gateway_length_errors_total 1")
            != std::string::npos
        && text.find("iot_gateway_mqtt_publish_success_total 3")
            != std::string::npos
        && text.find("iot_gateway_tcp_publish_failed_total 1")
            != std::string::npos
        && text.find("iot_gateway_cache_depth 5")
            != std::string::npos
        && text.find("iot_gateway_cache_flush_attempt_total 2")
            != std::string::npos;

    if (!status.ready() || !metrics_are_present) {
        std::cerr << "runtime observability values are incorrect"
                  << std::endl;
        return 1;
    }

    const std::string readiness = status.readiness_json();
    if (readiness.find("\"checks\":{")
            == std::string::npos
        || readiness.find("\"ready\":true")
            == std::string::npos
        || readiness.find("\"serial_worker_count\":2")
            == std::string::npos) {

        std::cerr << "readiness JSON is incorrect" << std::endl;
        return 1;
    }

    std::cout << "runtime observability test passed"
              << std::endl;
    return 0;
}
