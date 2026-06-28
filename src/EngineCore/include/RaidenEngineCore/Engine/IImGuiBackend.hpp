#pragma once

#include <RaidenEngineCore/Renderer/ICommandBuffer.hpp>

namespace Raiden::Core {

class IImGuiBackend {
public:
  virtual ~IImGuiBackend() = default;

  virtual bool init() = 0;
  virtual void newFrame() = 0;
  virtual void renderDrawData(ICommandBuffer &cmd) = 0;
  virtual void shutdown() = 0;
  virtual void waitIdle() = 0;
};

} // namespace Raiden::Core
