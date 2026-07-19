#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace protocol {

template <std::size_t Capacity>
class ByteRingBuffer {
    static_assert(Capacity > 0, "ring buffer capacity must be positive");

public:
    bool push(std::uint8_t byte) noexcept {
        if (size_ == Capacity) {
            return false;
        }

        const std::size_t tail = (head_ + size_) % Capacity;
        storage_[tail] = byte;
        ++size_;
        return true;
    }

    std::size_t push(
        const std::uint8_t* data,
        std::size_t length) noexcept {

        std::size_t accepted = 0;
        while (accepted < length && push(data[accepted])) {
            ++accepted;
        }

        return accepted;
    }

    void consume(std::size_t count) noexcept {
        const std::size_t consumed =
            count < size_ ? count : size_;

        head_ = (head_ + consumed) % Capacity;
        size_ -= consumed;
    }

    std::uint8_t at(std::size_t index) const {
        if (index >= size_) {
            throw std::out_of_range("ring buffer index out of range");
        }

        return storage_[(head_ + index) % Capacity];
    }

    std::vector<std::uint8_t> copy(
        std::size_t offset,
        std::size_t length) const {

        if (offset > size_ || length > size_ - offset) {
            throw std::out_of_range("ring buffer copy out of range");
        }

        std::vector<std::uint8_t> result;
        result.reserve(length);

        for (std::size_t index = 0; index < length; ++index) {
            result.push_back(at(offset + index));
        }

        return result;
    }

    void clear() noexcept {
        head_ = 0;
        size_ = 0;
    }

    std::size_t size() const noexcept {
        return size_;
    }

    constexpr std::size_t capacity() const noexcept {
        return Capacity;
    }

    bool empty() const noexcept {
        return size_ == 0;
    }

    bool full() const noexcept {
        return size_ == Capacity;
    }

private:
    std::array<std::uint8_t, Capacity> storage_{};
    std::size_t head_ = 0;
    std::size_t size_ = 0;
};

}  // namespace protocol
