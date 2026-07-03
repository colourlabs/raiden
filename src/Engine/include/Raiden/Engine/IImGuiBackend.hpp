#pragma once

#include <Raiden/Renderer/ICommandBuffer.hpp>

namespace Raiden::Engine {

class IImGuiBackend {
public:
  virtual ~IImGuiBackend() = default;

  virtual bool init() = 0;
  virtual void newFrame() = 0;
  virtual void renderDrawData(::Raiden::Renderer::ICommandBuffer &cmd) = 0;
  virtual void shutdown() = 0;
  virtual void waitIdle() = 0;
};

} // namespace Raiden::Engine
