#pragma once

#include <cstdint>

namespace Raiden::Core {

class IPipeline;
class IBuffer;
class ITexture;

class ICommandBuffer {
public:
  virtual ~ICommandBuffer() = default;

  virtual void bindPipeline(const IPipeline &pipeline) = 0;
  virtual void bindVertexBuffer(const IBuffer &buffer) = 0;
  virtual void bindIndexBuffer(const IBuffer &buffer) = 0;
  virtual void bindTexture(uint32_t slot, const ITexture &texture) = 0;
  virtual void drawIndexed(uint32_t indexCount) = 0;

  virtual void setViewport(int x, int y, int w, int h) = 0;
  virtual void setScissor(int x, int y, int w, int h) = 0;
  virtual void draw(uint32_t vertexCount, uint32_t instanceCount = 1,
                    uint32_t firstVertex = 0, uint32_t firstInstance = 0) = 0;
  virtual void drawIndexedInstanced(uint32_t indexCount,
                                    uint32_t instanceCount = 1,
                                    uint32_t firstIndex = 0,
                                    int32_t vertexOffset = 0,
                                    uint32_t firstInstance = 0) = 0;
  virtual void pushConstants(uint32_t offset, uint32_t size,
                             const void *data) = 0;
};

} // namespace Raiden::Core
