#pragma once

#include <glm/vec3.hpp>

#include <cstdint>

namespace Raiden::Physics {

struct ContactEvent {
  enum class Type : uint8_t { Added, Persisted, Removed };

  Type type;
  uint32_t bodyIdA = UINT32_MAX;
  uint32_t bodyIdB = UINT32_MAX;
  glm::vec3 contactNormal{0.0F};
};

} // namespace Raiden::Physics
