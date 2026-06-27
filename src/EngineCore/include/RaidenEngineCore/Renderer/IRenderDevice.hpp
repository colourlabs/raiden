#pragma once

#include <RaidenEngineCore/EngineConfig.hpp>

namespace Raiden::Core {

class IPlatform;

class IRenderDevice {
public:
  virtual ~IRenderDevice() = default;

  virtual bool init(const EngineConfig &config, IPlatform *platform) = 0;
  virtual void shutdown() = 0;

  virtual bool drawFrame() = 0;
};

} // namespace Raiden::Core
