#include "tcp/tcp_client.h"

#include <iostream>
#include <string>

int main() {
    tcp::TcpClient client(
        "127.0.0.1",
        9000
    );

    const std::string json =
        R"({"device_id":16,"temperature_c":25.3,"valid":true})";

    if (!client.connect_to_server()) {
        std::cerr
            << "TCP test failed: connection failed"
            << std::endl;

        return 1;
    }

    if (!client.send_json(json)) {
        std::cerr
            << "TCP test failed: send failed"
            << std::endl;

        return 1;
    }

    std::cout
        << "TCP test passed"
        << std::endl;

    client.disconnect();
    return 0;
}
