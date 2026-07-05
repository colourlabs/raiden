#pragma once

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace Raiden::Renderer {

enum class BufferUsage : std::uint8_t {
  Vertex,
  Index,
  Uniform,
};

enum class MemoryAccess : std::uint8_t {
  GpuOnly,   // device-local, not CPU-mappable
  CpuToGpu,  // mappable, written by CPU, read by GPU
};

enum class IndexType : std::uint8_t {
  Uint16,
  Uint32,
};

enum class BlendFactor : uint8_t {
  Zero,
  One,
  SrcAlpha,
  OneMinusSrcAlpha,
  DstAlpha,
  OneMinusDstAlpha,
  SrcColor,
  OneMinusSrcColor,
  DstColor,
  OneMinusDstColor,
  Src1Color,
  OneMinusSrc1Color,
};

enum class BlendOp : uint8_t {
  Add,
  Subtract,
  ReverseSubtract,
  Min,
  Max,
};

struct BufferDesc {
  size_t size = 0;
  BufferUsage usage = BufferUsage::Vertex;
  MemoryAccess access = MemoryAccess::GpuOnly;
  IndexType indexType = IndexType::Uint16;
};

// unified descriptor types for the public API

enum class Format : uint8_t {
  R32G32_Float,
  R32G32B32_Float,
  R32G32B32A32_Float,
  R8G8B8A8_UNORM,
  R8G8B8A8_SRGB,
};

struct VertexAttribute {
  uint32_t location;
  Format format;
  uint32_t offset;
};

struct VertexLayout {
  uint32_t stride;
  std::vector<VertexAttribute> attributes;
};

struct ShaderDesc {
  std::string_view path;
};

enum class CullMode : uint8_t {
  None,
  Front,
  Back,
};

enum class CompareOp : uint8_t {
  Less,
  LessOrEqual,
};

struct PipelineDesc {
  ShaderDesc shader;
  VertexLayout vertexLayout;
  bool depthTestEnable = true;
  bool depthWriteEnable = true;
  bool blendEnable = false;
  CullMode cullMode = CullMode::Back;
  CompareOp depthCompareOp = CompareOp::Less;
  BlendFactor blendSrcFactor = BlendFactor::SrcAlpha;
  BlendFactor blendDstFactor = BlendFactor::OneMinusSrcAlpha;
  BlendOp blendOp = BlendOp::Add;
  BlendFactor blendSrcAlphaFactor = BlendFactor::One;
  BlendFactor blendDstAlphaFactor = BlendFactor::OneMinusSrcAlpha;
  BlendOp blendAlphaOp = BlendOp::Add;
};

enum class TextureType : uint8_t {
  Texture2D,
  TextureCube,
};

struct TextureDesc {
  uint32_t width;
  uint32_t height;
  Format format;
  TextureType type = TextureType::Texture2D;
};

enum class SamplerFilter : uint8_t {
  Nearest,
  Linear,
};

enum class SamplerMipmapMode : uint8_t {
  Nearest,
  Linear,
};

enum class SamplerAddressMode : uint8_t {
  Repeat,
  MirroredRepeat,
  ClampToEdge,
  ClampToBorder,
};

struct SamplerDesc {
  SamplerFilter magFilter = SamplerFilter::Linear;
  SamplerFilter minFilter = SamplerFilter::Linear;
  SamplerMipmapMode mipmapMode = SamplerMipmapMode::Linear;
  SamplerAddressMode addressModeU = SamplerAddressMode::Repeat;
  SamplerAddressMode addressModeV = SamplerAddressMode::Repeat;
  SamplerAddressMode addressModeW = SamplerAddressMode::Repeat;
  bool anisotropyEnable = true;
  float maxAnisotropy = 16.0F;
  CompareOp compareOp = CompareOp::Less;
  float minLod = 0.0F;
  float maxLod = 1000.0F;
  float mipLodBias = 0.0F;
};

struct alignas(16) FrameUniforms {
  glm::mat4 projection;
  glm::mat4 view;
  glm::mat4 model;
  glm::vec4 extra;        // x=time, y=dt, z=resolutionX, w=resolutionY
  glm::vec4 cameraPos;    // xyz=camera world position, w unused
  glm::vec4 lightDir;     // xyz=direction toward light, w=intensity
  glm::vec4 lightColor;   // rgb=color, w=unused
  glm::vec4 ambientSky;   // rgb=hemispheric sky color, w=intensity
  glm::vec4 ambientGround; // rgb=hemispheric ground color, w=unused
};

struct Vertex {
  glm::vec3 pos;
  glm::vec3 normal;
  glm::vec3 color;
  glm::vec2 uv;
};

} // namespace Raiden::Renderer