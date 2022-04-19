#pragma once

#include <cstdint>
#include <glm/glm.hpp>


using vec2 = glm::vec2;

struct GameObject {
    vec2 position;
    vec2 velocity;
    float radius;
    uint32_t id;
};

