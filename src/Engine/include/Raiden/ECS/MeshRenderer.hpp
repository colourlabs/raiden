#pragma once

#include <string>

#include <glm/vec4.hpp>

namespace Raiden::ECS {

struct MeshRenderer {
  std::string meshPath;
  std::string texturePath;
  std::string shader = "builtin://pbr";

  glm::vec4 baseColorFactor{1.0F, 1.0F, 1.0F, 1.0F};
  float metallic = 0.0F;
  float roughness = 0.8F;

  bool castShadows = true;
};

} // namespace Raiden::ECS
