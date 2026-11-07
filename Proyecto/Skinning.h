#pragma once
#include <glm/glm.hpp>
constexpr int MAX_SKIN_BONES = 48;
struct SkinVertex
{
    glm::ivec4 joints = glm::ivec4(0);
    glm::vec4 weights = glm::vec4(0.0f);
};
