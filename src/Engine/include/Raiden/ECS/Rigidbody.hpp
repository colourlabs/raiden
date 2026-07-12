#pragma once

#include <cstdint>

namespace Raiden::ECS {

struct Rigidbody {
  enum class Type : uint8_t { Static, Kinematic, Dynamic };

  Type type = Type::Static;
  float mass = 1.0F;
  float friction = 0.5F;
  float restitution = 0.1F;

  uint32_t bodyId = UINT32_MAX;
};

} // namespace Raiden::ECS
