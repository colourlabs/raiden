#pragma once

#include <glm/glm.hpp>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace Raiden::Core {

enum class BufferUsage {
  Vertex,
  Index,
  Uniform,
};

enum class MemoryAccess {
  GpuOnly,   // device-local, not CPU-mappable
  CpuToGpu,  // mappable, written by CPU, read by GPU
};

struct BufferDesc {
  size_t size = 0;
  BufferUsage usage = BufferUsage::Vertex;
  MemoryAccess access = MemoryAccess::GpuOnly;
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

struct PipelineDesc {
  ShaderDesc shader;
  VertexLayout vertexLayout;
  bool depthTestEnable = true;
};

struct TextureDesc {
  uint32_t width;
  uint32_t height;
  Format format;
};

struct alignas(16) FrameUniforms {
  glm::mat4 projection;
  glm::mat4 view;
  glm::mat4 model;
  glm::vec4 extra;        // x=time, y=dt, z=resolutionX, w=resolutionY
  glm::vec4 cameraPos;    // xyz=camera world position, w unused
  glm::vec4 lightDir;     // xyz=direction toward light, w=intensity
  glm::vec4 lightColor;   // rgb=color, w=unused
};

struct Vertex {
  glm::vec3 pos;
  glm::vec3 normal;
  glm::vec3 color;
  glm::vec2 uv;
};

} // namespace Raiden::Core