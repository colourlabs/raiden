#pragma once

#include <glm/vec3.hpp>
#include <cstdint>

namespace Raiden::Physics {

struct PhysicsConfig {
  float fixedTimestep = 1.0F / 60.0F;
  glm::vec3 gravity{0.0F, -9.81F, 0.0F};
  uint32_t solverPositions = 2;
  uint32_t solverVelocities = 4;
  uint32_t maxBodies = 1024;
  uint32_t maxBodyPairs = 4096;
  uint32_t maxContactConstraints = 1024;
};

} // namespace Raiden::Physics
