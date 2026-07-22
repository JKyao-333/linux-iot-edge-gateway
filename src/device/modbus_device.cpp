#include "device/modbus_device.h"

#include <algorithm>
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

ModbusDevice::ModbusDevice(ModbusDeviceOptions options)
    : options_(std::move(options)) {
    status_.device_id = options_.request.slave_id;
    status_.protocol = ProtocolType::ModbusRtu;
}

bool ModbusDevice::start(std::string& error_message) {
    if (!port_.open_port(options_.port, options_.baud_rate, error_message)) {
        record_error();
        return false;
    }
    next_poll_ = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(status_mutex_);
    status_.online = true;
    return true;
}

void ModbusDevice::stop() noexcept {
    port_.close_port();
    std::lock_guard<std::mutex> lock(status_mutex_);
    status_.online = false;
}

DeviceReadResult ModbusDevice::read(std::chrono::milliseconds timeout) {
    const auto now = std::chrono::steady_clock::now();
    if (now < next_poll_) {
        std::this_thread::sleep_for(std::min(
            timeout,
            std::chrono::duration_cast<std::chrono::milliseconds>(next_poll_ - now)
        ));
        return {};
    }
    next_poll_ = now + std::chrono::milliseconds(options_.poll_interval_ms);

    std::string error;
    if (!port_.discard_input(error)) {
        record_error();
        stop();
        return {ReadCode::TransportError, std::nullopt, error};
    }
    const auto request = modbus::build_read_request(options_.request);
    if (!port_.write_all(request.data(), request.size(), error)) {
        record_error();
        stop();
        return {ReadCode::TransportError, std::nullopt, error};
    }

    std::vector<std::uint8_t> response;
    const auto response_timeout = std::chrono::milliseconds(
        options_.response_timeout_ms
    );
    const auto deadline = std::chrono::steady_clock::now()
        + response_timeout;
    std::size_t expected_size = 5;
    while (std::chrono::steady_clock::now() < deadline && response.size() < expected_size) {
        std::uint8_t buffer[256]{};
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()
        );
        const int received = port_.read_some(buffer, sizeof(buffer), remaining, error);
        if (received < 0) {
            record_error();
            stop();
            return {ReadCode::TransportError, std::nullopt, error};
        }
        if (received == 0) {
            break;
        }
        response.insert(response.end(), buffer, buffer + received);
        if (response.size() >= 2 && (response[1] & 0x80U) != 0) {
            expected_size = 5;
        } else if (response.size() >= 3) {
            expected_size = 3U + response[2] + 2U;
        }
    }

    if (response.empty()) {
        record_error();
        {
            std::lock_guard<std::mutex> lock(status_mutex_);
            status_.online = false;
        }
        return {ReadCode::Timeout, std::nullopt, "Modbus response timeout"};
    }

    const auto parsed = modbus::parse_read_response(response, options_.request);
    if (parsed.status != modbus::ResponseStatus::Ok) {
        record_error();
        return {ReadCode::ProtocolError, std::nullopt, parsed.error};
    }

    DeviceData device_data;
    if (!modbus::registers_to_device_data(
            parsed,
            options_.request.slave_id,
            device_data,
            error)) {
        record_error();
        return {ReadCode::ProtocolError, std::nullopt, error};
    }

    app::SensorData sensor_data = to_sensor_data(device_data);
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        status_.online = true;
        status_.last_update_unix_seconds = unix_seconds();
    }
    return {ReadCode::Data, sensor_data, {}};
}

DeviceStatus ModbusDevice::get_device_status() const {
    std::lock_guard<std::mutex> lock(status_mutex_);
    return status_;
}

void ModbusDevice::record_error() {
    std::lock_guard<std::mutex> lock(status_mutex_);
    ++status_.error_count;
}

}  // namespace device
