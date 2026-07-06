#pragma once

#include <Raiden/Renderer/RenderTypes.hpp>
#include <cstdint>
#include <string>

namespace Raiden::Renderer {

struct MaterialDesc {
  std::string shader = "builtin://pbr";

  // PBR factors
  glm::vec4 baseColorFactor = {1.0F, 1.0F, 1.0F, 1.0F};
  glm::vec3 emissiveFactor = {0.0F, 0.0F, 0.0F};
  float metallicFactor = 0.0F;
  float roughnessFactor = 1.0F;
  float occlusionStrength = 1.0F;

  // texture slots
  std::string baseColorTexture;
  std::string normalTexture;
  std::string metallicRoughnessTexture;
  std::string emissiveTexture;
  std::string occlusionTexture;

  // alpha
  enum class AlphaMode : std::uint8_t { Opaque, Mask, Blend };
  AlphaMode alphaMode = AlphaMode::Opaque;
  float alphaCutoff = 0.5F;

  // uv transform
  glm::vec2 uvOffset = {0.0F, 0.0F};
  glm::vec2 uvScale = {1.0F, 1.0F};
  float uvRotation = 0.0F;

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
  IMaterial() = default;
  virtual ~IMaterial() = default;

  IMaterial(const IMaterial &) = delete;
  IMaterial &operator=(const IMaterial &) = delete;
  IMaterial(IMaterial &&) = delete;
  IMaterial &operator=(IMaterial &&) = delete;

  virtual void bind(ICommandBuffer &cmd) = 0;
};

} // namespace Raiden::Renderer