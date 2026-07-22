#include "device/can_device.h"

#include "can/can_parser.h"

#include <chrono>

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

CanDevice::CanDevice(std::string interface_name)
    : socketcan_(std::move(interface_name)) {
    status_.protocol = ProtocolType::SocketCan;
}

bool CanDevice::start(std::string& error_message) {
    if (!socketcan_.open_device(error_message)) {
        record_error();
        return false;
    }
    std::lock_guard<std::mutex> lock(status_mutex_);
    status_.online = true;
    return true;
}

void CanDevice::stop() noexcept {
    socketcan_.close_device();
    std::lock_guard<std::mutex> lock(status_mutex_);
    status_.online = false;
}

DeviceReadResult CanDevice::read(std::chrono::milliseconds timeout) {
    canbus::CanFrame frame;
    std::string error;
    const int result = socketcan_.read_frame(frame, timeout, error);
    if (result == 0) {
        return {};
    }
    if (result < 0) {
        record_error();
        stop();
        return {ReadCode::TransportError, std::nullopt, error};
    }
    if (result == 2) {
        record_error();
        return {ReadCode::ProtocolError, std::nullopt, error};
    }

    DeviceData device_data;
    if (!canbus::parse_sensor_frame(frame, device_data, error)) {
        record_error();
        return {ReadCode::ProtocolError, std::nullopt, error};
    }

    app::SensorData sensor_data = to_sensor_data(device_data);
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        status_.device_id = sensor_data.device_id;
        status_.online = true;
        status_.last_update_unix_seconds = unix_seconds();
    }
    return {ReadCode::Data, sensor_data, {}};
}

DeviceStatus CanDevice::get_device_status() const {
    std::lock_guard<std::mutex> lock(status_mutex_);
    return status_;
}

void CanDevice::record_error() {
    std::lock_guard<std::mutex> lock(status_mutex_);
    ++status_.error_count;
}

}  // namespace device
