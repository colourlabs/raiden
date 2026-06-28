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
};

} // namespace Raiden::Core
