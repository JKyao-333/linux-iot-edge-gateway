#include "device/device_manager.h"

#include <chrono>
#include <thread>

namespace device {

namespace {

std::uint64_t unix_seconds() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
}

}  // namespace

DeviceManager::DeviceManager(
    std::chrono::seconds reconnect_interval,
    std::chrono::seconds heartbeat_timeout)
    : reconnect_interval_(reconnect_interval),
      heartbeat_timeout_(heartbeat_timeout) {}

DeviceManager::~DeviceManager() {
    stop();
}

void DeviceManager::add(std::unique_ptr<DeviceInterface> input) {
    if (running_.load()) {
        return;
    }
    devices_.push_back(std::move(input));
}

void DeviceManager::start(
    DataHandler data_handler,
    ErrorHandler error_handler) {

    if (running_.exchange(true)) {
        return;
    }
    started_at_unix_seconds_.store(unix_seconds());
    data_handler_ = std::move(data_handler);
    error_handler_ = std::move(error_handler);
    for (auto& input : devices_) {
        workers_.emplace_back([this, input = input.get()] {
            run_device(*input);
        });
    }
}

void DeviceManager::stop() noexcept {
    if (!running_.exchange(false) && workers_.empty()) {
        return;
    }
    for (auto& input : devices_) {
        input->stop();
    }
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
}

void DeviceManager::run_device(DeviceInterface& input) {
    bool started = false;
    while (running_.load()) {
        if (!started) {
            std::string error;
            started = input.start(error);
            if (!started) {
                if (error_handler_) {
                    error_handler_(
                        input.get_device_status().protocol,
                        ReadCode::TransportError,
                        error,
                        {}
                    );
                }
                std::this_thread::sleep_for(reconnect_interval_);
                continue;
            }
        }

        DeviceReadResult result = input.read(std::chrono::milliseconds(250));
        const ProtocolType protocol = input.get_device_status().protocol;
        if (result.code == ReadCode::Data && result.data.has_value()) {
            if (data_handler_) {
                data_handler_(*result.data, protocol);
            }
        }

        const bool report_error = !result.error_stats.empty()
            || result.code == ReadCode::ProtocolError
            || result.code == ReadCode::TransportError
            || (result.code == ReadCode::Timeout
                && protocol == ProtocolType::ModbusRtu);
        if (report_error && error_handler_) {
            error_handler_(
                protocol,
                result.code,
                result.error,
                result.error_stats
            );
        }

        if (result.code == ReadCode::TransportError) {
            input.stop();
            started = false;
        }
    }
    input.stop();
}

std::vector<DeviceStatus> DeviceManager::statuses() const {
    std::vector<DeviceStatus> result;
    result.reserve(devices_.size());
    const std::uint64_t now = unix_seconds();
    for (const auto& input : devices_) {
        DeviceStatus status = input->get_device_status();
        const std::uint64_t heartbeat_reference =
            status.last_update_unix_seconds != 0
                ? status.last_update_unix_seconds
                : started_at_unix_seconds_.load();
        if (status.online && heartbeat_reference != 0
            && now > heartbeat_reference
            && now - heartbeat_reference
                > static_cast<std::uint64_t>(heartbeat_timeout_.count())) {
            status.online = false;
        }
        result.push_back(status);
    }
    return result;
}

std::size_t DeviceManager::online_count() const {
    std::size_t count = 0;
    for (const auto& status : statuses()) {
        if (status.online) {
            ++count;
        }
    }
    return count;
}

std::size_t DeviceManager::offline_count() const {
    return statuses().size() - online_count();
}

}  // namespace device
