#include "app/shutdown_signal.h"

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <thread>

namespace {

volatile std::sig_atomic_t shutdown_flag = 0;

extern "C" void handle_shutdown_signal(int signal_number) {
    static_cast<void>(signal_number);
    shutdown_flag = 1;
}

bool install_handler(
    int signal_number,
    const char* signal_name,
    std::string& error_message) {

    struct sigaction action {};
    action.sa_handler = handle_shutdown_signal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    if (sigaction(signal_number, &action, nullptr) != 0) {
        error_message =
            "install " + std::string(signal_name)
            + " handler failed: "
            + std::strerror(errno);

        return false;
    }

    return true;
}

}  // namespace

namespace app {

bool install_shutdown_signal_handlers(
    std::string& error_message) {

    shutdown_flag = 0;

    if (!install_handler(
            SIGINT,
            "SIGINT",
            error_message)) {

        return false;
    }

    if (!install_handler(
            SIGTERM,
            "SIGTERM",
            error_message)) {

        return false;
    }

    error_message.clear();
    return true;
}

bool shutdown_requested() noexcept {
    return shutdown_flag != 0;
}

bool wait_for_shutdown(
    std::chrono::milliseconds timeout) {

    const auto deadline =
        std::chrono::steady_clock::now() + timeout;

    while (!shutdown_requested()) {
        const auto now =
            std::chrono::steady_clock::now();

        if (now >= deadline) {
            break;
        }

        const auto remaining =
            std::chrono::duration_cast<
                std::chrono::milliseconds
            >(deadline - now);

        std::this_thread::sleep_for(
            std::min(
                remaining,
                std::chrono::milliseconds(50)
            )
        );
    }

    return shutdown_requested();
}

void reset_shutdown_request_for_test() noexcept {
    shutdown_flag = 0;
}

}  // namespace app
