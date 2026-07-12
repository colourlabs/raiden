#pragma once

#include <glm/vec3.hpp>

#include <cstdint>

namespace Raiden::ECS {

struct Collider {
  enum class Shape : uint8_t { Box, Sphere, Capsule };

  Shape shape = Shape::Box;
  glm::vec3 halfExtents{0.5F};
  float radius = 0.5F;
  float height = 1.0F;
};

} // namespace Raiden::ECS
