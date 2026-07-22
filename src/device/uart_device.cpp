#include "device/uart_device.h"

#include "app/sensor_data.h"

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

UartDevice::UartDevice(std::string path, int baud_rate)
    : path_(std::move(path)), baud_rate_(baud_rate) {
    status_.protocol = ProtocolType::Uart;
}

bool UartDevice::start(std::string& error_message) {
    if (!port_.open_port(path_, baud_rate_, error_message)) {
        record_error();
        return false;
    }
    parser_.reset();
    std::lock_guard<std::mutex> lock(status_mutex_);
    status_.online = true;
    return true;
}

void UartDevice::stop() noexcept {
    port_.close_port();
    std::lock_guard<std::mutex> lock(status_mutex_);
    status_.online = false;
}

DeviceReadResult UartDevice::read(std::chrono::milliseconds timeout) {
    if (!pending_.empty()) {
        app::SensorData data = pending_.front();
        pending_.pop_front();
        return {ReadCode::Data, data, {}};
    }

    std::uint8_t buffer[256]{};
    std::string error;
    const int received = port_.read_some(buffer, sizeof(buffer), timeout, error);
    if (received < 0) {
        record_error();
        stop();
        return {ReadCode::TransportError, std::nullopt, error};
    }
    if (received == 0) {
        return {};
    }

    const auto frames = parser_.feed(buffer, static_cast<std::size_t>(received));
    const std::size_t errors = parser_.take_crc_error_count()
        + parser_.take_length_error_count()
        + parser_.take_overflow_byte_count();
    if (errors > 0) {
        record_error();
    }

    for (const auto& frame : frames) {
        app::SensorData data;
        if (app::parse_sensor_data(frame, data)) {
            pending_.push_back(std::move(data));
        } else {
            record_error();
        }
    }

    if (pending_.empty()) {
        if (errors > 0) {
            return {ReadCode::ProtocolError, std::nullopt, "UART frame rejected"};
        }
        return {};
    }

    app::SensorData data = pending_.front();
    pending_.pop_front();
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        status_.device_id = data.device_id;
        status_.online = true;
        status_.last_update_unix_seconds = unix_seconds();
    }
    return {ReadCode::Data, data, {}};
}

DeviceStatus UartDevice::get_device_status() const {
    std::lock_guard<std::mutex> lock(status_mutex_);
    return status_;
}

void UartDevice::record_error() {
    std::lock_guard<std::mutex> lock(status_mutex_);
    ++status_.error_count;
}

}  // namespace device
