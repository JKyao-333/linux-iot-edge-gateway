#include "device/modbus_device.h"

#include <chrono>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>

int main() {
    const int master = posix_openpt(O_RDWR | O_NOCTTY);
    if (master < 0 || grantpt(master) != 0 || unlockpt(master) != 0) {
        std::cerr << "unable to create pseudo terminal" << std::endl;
        return 1;
    }

    const char* slave_name = ptsname(master);
    if (slave_name == nullptr) {
        close(master);
        return 1;
    }

    device::ModbusDeviceOptions options;
    options.port = slave_name;
    options.poll_interval_ms = 1;
    options.response_timeout_ms = 30;
    device::ModbusDevice input(options);
    std::string error;
    if (!input.start(error)) {
        std::cerr << error << std::endl;
        close(master);
        return 1;
    }

    const auto started_at = std::chrono::steady_clock::now();
    const auto result = input.read(std::chrono::milliseconds(500));
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started_at
    );
    input.stop();
    close(master);
    if (result.code != device::ReadCode::Timeout
        || input.get_device_status().error_count == 0
        || elapsed >= std::chrono::milliseconds(250)) {
        std::cerr << "Modbus no-response timeout was not reported" << std::endl;
        return 1;
    }

    std::cout << "Modbus timeout test passed" << std::endl;
    return 0;
}
