#pragma once

#include <chrono>
#include <string>

namespace app {

bool install_shutdown_signal_handlers(
    std::string& error_message
);

bool shutdown_requested() noexcept;

bool wait_for_shutdown(
    std::chrono::milliseconds timeout
);

void reset_shutdown_request_for_test() noexcept;

}  // namespace app
