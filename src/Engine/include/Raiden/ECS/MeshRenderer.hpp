#pragma once

#include <string>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace Raiden::ECS {

struct MeshRenderer {
  std::string meshPath;
  std::string texturePath;
  std::string normalMap;
  std::string metallicRoughnessMap;
  std::string occlusionMap;
  std::string shader = "builtin://pbr";

  glm::vec4 baseColorFactor{1.0F, 1.0F, 1.0F, 1.0F};
  float metallic = 0.0F;
  float roughness = 0.8F;

  // Multiplies mesh UVs before sampling material textures. This keeps a
  // large surface from stretching a single texture across its whole extent.
  glm::vec2 uvScale{1.0F, 1.0F};
  bool triplanarMapping = false;

  bool castShadows = true;
};

} // namespace Raiden::ECS
