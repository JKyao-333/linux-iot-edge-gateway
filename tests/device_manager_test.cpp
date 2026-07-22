#include "device/device_manager.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

namespace {

class FakeDevice final : public device::DeviceInterface {
public:
    bool start(std::string&) override {
        ++start_count;
        status_.online = true;
        status_.protocol = device::ProtocolType::SocketCan;
        return true;
    }

    void stop() noexcept override {
        status_.online = false;
    }

    device::DeviceReadResult read(std::chrono::milliseconds timeout) override {
        if (calls_++ == 0) {
            app::SensorData data;
            data.device_id = 7;
            data.valid = true;
            status_.device_id = 7;
            status_.last_update_unix_seconds = 1;
            device::DeviceReadResult result;
            result.code = device::ReadCode::Data;
            result.data = data;
            result.error = "bad frame followed by valid frame";
            result.error_stats.crc_errors = 1;
            result.error_stats.generic_errors = 1;
            return result;
        }
        if (calls_ == 2) {
            ++status_.error_count;
            return {device::ReadCode::ProtocolError, std::nullopt, "test error"};
        }
        if (calls_ == 3) {
            ++status_.error_count;
            return {device::ReadCode::TransportError, std::nullopt, "link lost"};
        }
        std::this_thread::sleep_for(timeout);
        return {};
    }

    device::DeviceStatus get_device_status() const override {
        return status_;
    }

    std::atomic<int> start_count{0};

private:
    int calls_ = 0;
    device::DeviceStatus status_;
};

}  // namespace

int main() {
    device::DeviceManager manager(
        std::chrono::seconds(1),
        std::chrono::seconds(1)
    );
    auto fake = std::make_unique<FakeDevice>();
    FakeDevice* fake_ptr = fake.get();
    manager.add(std::move(fake));

    std::atomic<int> data_count{0};
    std::atomic<int> error_count{0};
    std::atomic<std::size_t> classified_error_count{0};
    manager.start(
        [&](const app::SensorData&, device::ProtocolType) { ++data_count; },
        [&](device::ProtocolType,
            device::ReadCode,
            const std::string&,
            const device::ProtocolErrorStats& stats) {
            ++error_count;
            classified_error_count.fetch_add(stats.total());
        }
    );
    for (int attempt = 0; attempt < 50
         && (error_count.load() < 3 || fake_ptr->start_count.load() < 2);
         ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    const auto statuses = manager.statuses();
    manager.stop();
    if (data_count.load() != 1 || error_count.load() != 3
        || classified_error_count.load() != 2
        || fake_ptr->start_count.load() < 2
        || statuses.size() != 1 || statuses[0].device_id != 7
        || statuses[0].error_count != 2 || statuses[0].online) {
        std::cerr << "device manager aggregation failed" << std::endl;
        return 1;
    }

    std::cout << "device manager test passed" << std::endl;
    return 0;
}
