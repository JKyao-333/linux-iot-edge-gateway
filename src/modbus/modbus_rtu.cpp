#include "modbus/modbus_rtu.h"

#include "protocol/crc16.h"

#include <sstream>

namespace modbus {

namespace {

void append_u16_be(
    std::vector<std::uint8_t>& output,
    std::uint16_t value) {

    output.push_back(static_cast<std::uint8_t>(value >> 8));
    output.push_back(static_cast<std::uint8_t>(value & 0xFF));
}

std::uint16_t read_u16_be(
    const std::vector<std::uint8_t>& input,
    std::size_t offset) {

    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(input[offset]) << 8
        | static_cast<std::uint16_t>(input[offset + 1])
    );
}

bool has_valid_crc(const std::vector<std::uint8_t>& frame) {
    if (frame.size() < 4) {
        return false;
    }

    const std::size_t payload_size = frame.size() - 2;
    const std::uint16_t expected = protocol::crc16_modbus(
        frame.data(),
        payload_size
    );
    const std::uint16_t received =
        static_cast<std::uint16_t>(frame[payload_size])
        | static_cast<std::uint16_t>(frame[payload_size + 1]) << 8;

    return expected == received;
}

}  // namespace

std::vector<std::uint8_t> build_read_request(
    const ReadRequest& request) {

    std::vector<std::uint8_t> frame;
    frame.reserve(8);
    frame.push_back(request.slave_id);
    frame.push_back(static_cast<std::uint8_t>(request.function));
    append_u16_be(frame, request.start_address);
    append_u16_be(frame, request.register_count);

    const std::uint16_t crc = protocol::crc16_modbus(
        frame.data(),
        frame.size()
    );
    frame.push_back(static_cast<std::uint8_t>(crc & 0xFF));
    frame.push_back(static_cast<std::uint8_t>(crc >> 8));
    return frame;
}

ReadResponse parse_read_response(
    const std::vector<std::uint8_t>& frame,
    const ReadRequest& request) {

    ReadResponse response;
    if (frame.size() < 5) {
        response.error = "Modbus response is too short";
        return response;
    }

    if (!has_valid_crc(frame)) {
        response.status = ResponseStatus::CrcError;
        response.error = "Modbus response CRC mismatch";
        return response;
    }

    if (frame[0] != request.slave_id) {
        response.status = ResponseStatus::AddressMismatch;
        response.error = "Modbus slave address mismatch";
        return response;
    }

    const std::uint8_t function =
        static_cast<std::uint8_t>(request.function);
    if (frame[1] == static_cast<std::uint8_t>(function | 0x80)) {
        if (frame.size() != 5) {
            response.status = ResponseStatus::LengthError;
            response.error = "Modbus exception response length is invalid";
            return response;
        }

        response.status = ResponseStatus::ExceptionResponse;
        response.exception_code = frame[2];
        response.error = "Modbus slave exception code="
            + std::to_string(response.exception_code);
        return response;
    }

    if (frame[1] != function) {
        response.status = ResponseStatus::FunctionMismatch;
        response.error = "Modbus function code mismatch";
        return response;
    }

    const std::size_t byte_count = frame[2];
    const std::size_t expected_byte_count =
        static_cast<std::size_t>(request.register_count) * 2;
    if (byte_count != expected_byte_count
        || frame.size() != byte_count + 5) {

        response.status = ResponseStatus::LengthError;
        response.error = "Modbus register byte count is invalid";
        return response;
    }

    response.registers.reserve(request.register_count);
    for (std::size_t offset = 3;
         offset < 3 + byte_count;
         offset += 2) {

        response.registers.push_back(read_u16_be(frame, offset));
    }

    response.status = ResponseStatus::Ok;
    response.error.clear();
    return response;
}

bool registers_to_device_data(
    const ReadResponse& response,
    std::uint8_t device_id,
    device::DeviceData& output,
    std::string& error_message) {

    if (response.status != ResponseStatus::Ok) {
        error_message = response.error;
        return false;
    }

    if (response.registers.size() < 6) {
        error_message = "six sensor registers are required";
        return false;
    }

    output = device::DeviceData{};
    output.device_id = device_id;
    output.protocol = device::ProtocolType::ModbusRtu;
    output.temperature_c =
        static_cast<std::int16_t>(response.registers[0]) / 10.0;
    output.humidity_percent = response.registers[1] / 10.0;
    output.gas_ppm = response.registers[2];
    output.battery_mv = response.registers[3];
    output.status = static_cast<std::uint8_t>(response.registers[4]);
    output.sequence = response.registers[5];
    error_message.clear();
    return true;
}

}  // namespace modbus
