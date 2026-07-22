#pragma once

#include "can/can_interface.h"
#include "device/device_data.h"

#include <string>

namespace canbus {

bool parse_sensor_frame(
    const CanFrame& frame,
    device::DeviceData& output,
    std::string& error_message
);

}  // namespace canbus
