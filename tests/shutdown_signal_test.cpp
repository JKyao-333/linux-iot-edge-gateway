#include "app/shutdown_signal.h"

#include <chrono>
#include <csignal>
#include <iostream>
#include <string>

namespace {

bool verify_signal(int signal_number) {
    app::reset_shutdown_request_for_test();

    if (std::raise(signal_number) != 0) {
        return false;
    }

    return app::wait_for_shutdown(
        std::chrono::milliseconds(200)
    );
}

}  // namespace

int main() {
    std::string error_message;

    if (!app::install_shutdown_signal_handlers(
            error_message)) {

        std::cerr
            << "signal handler installation failed: "
            << error_message
            << std::endl;

        return 1;
    }

    const bool sigint_handled =
        verify_signal(SIGINT);
    const bool sigterm_handled =
        verify_signal(SIGTERM);

    std::cout
        << "SIGINT handled: "
        << std::boolalpha
        << sigint_handled
        << std::endl;

    std::cout
        << "SIGTERM handled: "
        << std::boolalpha
        << sigterm_handled
        << std::endl;

    return sigint_handled && sigterm_handled
        ? 0
        : 1;
}
