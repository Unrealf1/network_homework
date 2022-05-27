#pragma once


#include <array>
#include <ranges>

#include <bytestream.hpp>


template<size_t SIZE> 
using secret_t = std::array<std::byte, SIZE>;


template<std::ranges::contiguous_range range>
void inner_calc_sum(const range& bytes, uint32_t& sum) {
    for (const auto& byte : bytes) {
        sum += uint32_t(byte);
    }
}

template<std::ranges::contiguous_range range1, std::ranges::contiguous_range range2>
uint32_t calc_sum(const range1& bytes, const range2& secret) {
    uint32_t sum = 0;
    inner_calc_sum(bytes, sum);
    inner_calc_sum(secret, sum);
    return sum;
}


template<size_t SIZE, std::ranges::contiguous_range range>
bool verify(const range message, const secret_t<SIZE>& secret) {
    if (message.size() < sizeof(uint32_t)) {
        return false;
    }
    uint32_t message_sum = *((const uint32_t*)(&*(message.end() - sizeof(uint32_t))));
    auto calculated_sum = calc_sum(message.subspan(0, message.size() - sizeof(uint32_t)), secret);
    return calculated_sum == message_sum;
}

template<size_t SIZE>
inline void sign(OutByteStream& message, const secret_t<SIZE>& secret) {
    auto sum = calc_sum(message.get_span(), secret);
    message << sum;
}

