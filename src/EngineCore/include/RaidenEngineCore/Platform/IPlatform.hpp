#pragma once

#include <RaidenEngineCore/EngineConfig.hpp>

namespace Raiden::Core {

class IPlatform {
public:
  virtual ~IPlatform() = default;

  virtual bool init(const WindowConfig& config, RenderBackend backend) = 0;
  virtual void shutdown() = 0;
  virtual bool pollEvents() = 0;

  virtual void* getNativeWindowHandle() = 0;
};

} // namespace Raiden::Core
