#pragma once

#include "device/device_data.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace modbus {

enum class FunctionCode : std::uint8_t {
    ReadHoldingRegisters = 0x03,
    ReadInputRegisters = 0x04
};

enum class ResponseStatus {
    Ok,
    CrcError,
    ExceptionResponse,
    LengthError,
    AddressMismatch,
    FunctionMismatch
};

struct ReadRequest {
    std::uint8_t slave_id = 1;
    FunctionCode function = FunctionCode::ReadHoldingRegisters;
    std::uint16_t start_address = 0;
    std::uint16_t register_count = 6;
};

struct ReadResponse {
    ResponseStatus status = ResponseStatus::LengthError;
    std::vector<std::uint16_t> registers;
    std::uint8_t exception_code = 0;
    std::string error;
};

std::vector<std::uint8_t> build_read_request(
    const ReadRequest& request
);

ReadResponse parse_read_response(
    const std::vector<std::uint8_t>& frame,
    const ReadRequest& request
);

bool registers_to_device_data(
    const ReadResponse& response,
    std::uint8_t device_id,
    device::DeviceData& output,
    std::string& error_message
);

}  // namespace modbus
