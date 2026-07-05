#pragma once

#include <Raiden/Renderer/RenderTypes.hpp>
#include <cstdint>
#include <string>

namespace Raiden::Renderer {

struct MaterialDesc {
  std::string shader = "builtin://pbr";

  // PBR factors
  glm::vec4 baseColorFactor = {1.0F, 1.0F, 1.0F, 1.0F};
  glm::vec3 emissiveFactor = {0.0f, 0.0f, 0.0f};
  float metallicFactor = 0.0f;
  float roughnessFactor = 1.0f;
  float occlusionStrength = 1.0f;

  // texture slots
  std::string baseColorTexture;
  std::string normalTexture;
  std::string metallicRoughnessTexture;
  std::string emissiveTexture;
  std::string occlusionTexture;

  // alpha
  enum class AlphaMode : std::uint8_t { Opaque, Mask, Blend };
  AlphaMode alphaMode = AlphaMode::Opaque;
  float alphaCutoff = 0.5f;

  // uv transform
  glm::vec2 uvOffset = {0.0f, 0.0f};
  glm::vec2 uvScale = {1.0f, 1.0f};
  float uvRotation = 0.0f;

  // misc
  bool doubleSided = false;
  bool depthTest = true;
  bool normalMapDirectX = false;
  bool castShadows = true;
  bool receiveShadows = true;

  enum class VertexColorMode : std::uint8_t { Ignore, Multiply, Add };
  VertexColorMode vertexColorMode = VertexColorMode::Ignore;

  bool operator==(const MaterialDesc &) const = default;
};

class ICommandBuffer;

class IMaterial {
public:
  virtual ~IMaterial() = default;
  virtual void bind(ICommandBuffer &cmd) = 0;
};

} // namespace Raiden::Renderer