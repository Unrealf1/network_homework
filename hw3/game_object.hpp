#pragma once

#include <cstdint>
#include <glm/glm.hpp>


using vec2 = glm::vec2;

struct GameObject {
    float radius;
    vec2 position;
    uint32_t id;
};

