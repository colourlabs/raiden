#pragma once

#include <RaidenEngineCore/ECS/World.hpp>
#include <RaidenEngineCore/Renderer/IRenderDevice.hpp>
#include <volk.h>

namespace Raiden::Core {

class IVulkanRenderDevice : public IRenderDevice {
public:
  virtual ~IVulkanRenderDevice() override = default;

  virtual VkInstance getInstance() const = 0;
  virtual VkPhysicalDevice getPhysicalDevice() const = 0;
  virtual VkDevice getDevice() const = 0;
  virtual VkQueue getGraphicsQueue() const = 0;
  virtual VkQueue getPresentQueue() const = 0;
  virtual uint32_t getGraphicsQueueIndex() const = 0;
  virtual uint32_t getPresentQueueIndex() const = 0;
  virtual bool hasValidation() const = 0;
  virtual VkRenderPass getRenderPass() const = 0;
  virtual uint32_t getSwapchainImageCount() const = 0;

  virtual void setWorld(World *world) = 0;
};

} // namespace Raiden::Core
