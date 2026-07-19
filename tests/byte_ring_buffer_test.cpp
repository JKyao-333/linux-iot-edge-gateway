#include "protocol/byte_ring_buffer.h"

#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    protocol::ByteRingBuffer<5> buffer;

    const std::uint8_t first[] = {1, 2, 3, 4};
    if (buffer.push(first, 4) != 4 || buffer.size() != 4) {
        std::cerr << "initial push failed" << std::endl;
        return 1;
    }

    buffer.consume(3);

    const std::uint8_t wrapped[] = {5, 6, 7, 8};
    if (buffer.push(wrapped, 4) != 4 || !buffer.full()) {
        std::cerr << "wrapped push failed" << std::endl;
        return 1;
    }

    const std::vector<std::uint8_t> expected = {4, 5, 6, 7, 8};
    if (buffer.copy(0, buffer.size()) != expected) {
        std::cerr << "wrapped order is incorrect" << std::endl;
        return 1;
    }

    if (buffer.push(9)) {
        std::cerr << "full buffer accepted an extra byte" << std::endl;
        return 1;
    }

    buffer.consume(5);
    if (!buffer.empty() || buffer.size() != 0) {
        std::cerr << "consume did not empty the buffer" << std::endl;
        return 1;
    }

    std::cout
        << "ring buffer wraparound test passed, capacity="
        << buffer.capacity()
        << std::endl;

    return 0;
}
