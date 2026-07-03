#pragma once

#include <Raiden/EngineConfig.hpp>
#include <Raiden/Platform/InputState.hpp>
#include <vector>
#include <volk.h>

namespace Raiden::Platform {

class IPlatform {
public:
  virtual ~IPlatform() = default;

  virtual bool init(const ::Raiden::Core::WindowConfig &config, ::Raiden::Core::RenderBackend backend) = 0;
  virtual void shutdown() = 0;
  virtual bool pollEvents() = 0;

  virtual void *getNativeWindowHandle() = 0;

  virtual void getWindowSize(int &width, int &height) const = 0;

  virtual const InputState &getInputState() const = 0;
  virtual void setRelativeMouseMode(bool enabled) = 0;
  virtual void endInputFrame() = 0;

  virtual std::vector<const char *> getRequiredInstanceExtensions() const = 0;
  virtual bool createVulkanSurface(VkInstance instance,
                                   VkSurfaceKHR *surface) = 0;
};

} // namespace Raiden::Platform