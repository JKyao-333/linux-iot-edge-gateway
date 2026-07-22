#pragma once

#include "app/sensor_data.h"
#include "device/device_data.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace device {

enum class ReadCode {
    Data,
    Timeout,
    ProtocolError,
    TransportError
};

struct ProtocolErrorStats {
    std::size_t crc_errors = 0;
    std::size_t length_errors = 0;
    std::size_t overflow_bytes = 0;
    std::size_t generic_errors = 0;

    std::size_t total() const noexcept {
        return crc_errors + length_errors + overflow_bytes + generic_errors;
    }

    std::size_t invalid_frame_count() const noexcept {
        return crc_errors + length_errors + generic_errors;
    }

    bool empty() const noexcept {
        return total() == 0;
    }
};

struct DeviceReadResult {
    ReadCode code = ReadCode::Timeout;
    std::optional<app::SensorData> data;
    std::string error;
    ProtocolErrorStats error_stats;
};

struct DeviceStatus {
    std::uint32_t device_id = 0;
    ProtocolType protocol = ProtocolType::Uart;
    bool online = false;
    std::uint64_t last_update_unix_seconds = 0;
    std::uint64_t error_count = 0;
};

class DeviceInterface {
public:
    virtual ~DeviceInterface() = default;

    virtual bool start(std::string& error_message) = 0;
    virtual void stop() noexcept = 0;
    virtual DeviceReadResult read(
        std::chrono::milliseconds timeout
    ) = 0;
    virtual DeviceStatus get_device_status() const = 0;
};

}  // namespace device
