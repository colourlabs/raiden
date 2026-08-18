#pragma once

#include <glm/glm.hpp>

namespace Raiden::ECS {

struct DirectionalLight {
  glm::vec3 direction{0.3F, -1.0F, 0.5F};
  glm::vec3 color{1.0F, 0.98F, 0.92F};
  float intensity = 1.0F;
  bool castShadows = true;

  // shadow map settings
  float shadowNear = 0.1F;
  float shadowFar = 50.0F;
  float shadowSize = 20.0F; // orthographic half-extent
  uint32_t shadowMapResolution = 2048;
};

struct PointLight {
  glm::vec3 position{0.0F};
  glm::vec3 color{1.0F, 1.0F, 1.0F};
  float intensity = 1.0F;
  float range = 10.0F;
  bool castShadows = false; // cubemap shadows - future
};

struct AmbientLight {
  glm::vec3 skyColor{0.1F, 0.15F, 0.3F};
  float skyIntensity = 1.0F;
  glm::vec3 groundColor{0.02F, 0.01F, 0.01F};
  bool useIBL = false;
};

} // namespace Raiden::ECS
