#pragma once


#include <random>


inline auto& get_random_generator() {
    static std::random_device device;
    static std::mt19937 generator(device());
    return generator;
}
