#pragma once

#include <RaidenEngineCore/EngineConfig.hpp>
#include <volk.h>
#include <vector>

namespace Raiden::Core {

class IPlatform {
public:
  virtual ~IPlatform() = default;

  virtual bool init(const WindowConfig& config, RenderBackend backend) = 0;
  virtual void shutdown() = 0;
  virtual bool pollEvents() = 0;

  virtual void* getNativeWindowHandle() = 0;

  virtual void getWindowSize(int &width, int &height) const = 0;

  virtual std::vector<const char *> getRequiredInstanceExtensions() const = 0;
  virtual bool createVulkanSurface(VkInstance instance, VkSurfaceKHR *surface) = 0;
};

} // namespace Raiden::Core