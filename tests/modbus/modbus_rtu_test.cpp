#include "modbus/modbus_rtu.h"
#include "protocol/crc16.h"

#include <cstdint>
#include <iostream>
#include <vector>

namespace {

void append_crc(std::vector<std::uint8_t>& frame) {
    const std::uint16_t crc = protocol::crc16_modbus(
        frame.data(),
        frame.size()
    );
    frame.push_back(static_cast<std::uint8_t>(crc & 0xFF));
    frame.push_back(static_cast<std::uint8_t>(crc >> 8));
}

}  // namespace

int main() {
    const modbus::ReadRequest request{
        1,
        modbus::FunctionCode::ReadHoldingRegisters,
        0,
        6
    };

    const std::vector<std::uint8_t> expected_request = {
        0x01, 0x03, 0x00, 0x00, 0x00, 0x06, 0xC5, 0xC8
    };
    if (modbus::build_read_request(request) != expected_request) {
        std::cerr << "Modbus request frame mismatch" << std::endl;
        return 1;
    }

    modbus::ReadRequest input_request = request;
    input_request.function = modbus::FunctionCode::ReadInputRegisters;
    const std::vector<std::uint8_t> expected_input_request = {
        0x01, 0x04, 0x00, 0x00, 0x00, 0x06, 0x70, 0x08
    };
    if (modbus::build_read_request(input_request) != expected_input_request) {
        std::cerr << "Modbus function 04 request frame mismatch" << std::endl;
        return 1;
    }

    std::vector<std::uint8_t> response = {
        0x01, 0x03, 0x0C,
        0x00, 0xFD,
        0x02, 0x60,
        0x01, 0x2C,
        0x0E, 0x74,
        0x00, 0x00,
        0x00, 0x09
    };
    append_crc(response);

    const auto parsed = modbus::parse_read_response(response, request);
    device::DeviceData data;
    std::string error;
    if (!modbus::registers_to_device_data(parsed, 1, data, error)
        || data.temperature_c != 25.3
        || data.humidity_percent != 60.8
        || data.gas_ppm != 300
        || data.battery_mv != 3700
        || data.sequence != 9) {

        std::cerr << "valid Modbus response was not converted" << std::endl;
        return 1;
    }

    response.back() ^= 0x01;
    if (modbus::parse_read_response(response, request).status
        != modbus::ResponseStatus::CrcError) {

        std::cerr << "CRC error was not detected" << std::endl;
        return 1;
    }

    std::vector<std::uint8_t> exception = {0x01, 0x83, 0x02};
    append_crc(exception);
    const auto exception_result =
        modbus::parse_read_response(exception, request);
    if (exception_result.status
            != modbus::ResponseStatus::ExceptionResponse
        || exception_result.exception_code != 0x02) {

        std::cerr << "slave exception was not detected" << std::endl;
        return 1;
    }

    std::vector<std::uint8_t> wrong_length = {
        0x01, 0x03, 0x02, 0x00, 0xFD
    };
    append_crc(wrong_length);
    if (modbus::parse_read_response(wrong_length, request).status
        != modbus::ResponseStatus::LengthError) {

        std::cerr << "length error was not detected" << std::endl;
        return 1;
    }

    std::cout << "Modbus RTU test passed" << std::endl;
    return 0;
}
