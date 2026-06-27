#pragma once

#include <RaidenEngineCore/Renderer/IRenderDevice.hpp>

#include <optional>
#include <vector>

namespace Raiden::Core {

struct QueueFamilyIndices {
  std::optional<uint32_t> graphicsFamily;
  std::optional<uint32_t> presentFamily;

  [[nodiscard]] bool isComplete() const {
    return graphicsFamily.has_value() && presentFamily.has_value();
  }
};

struct SwapChainSupport {
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;
};

class VulkanDevice final : public IRenderDevice {
public:
  VulkanDevice() = default;
  ~VulkanDevice() override;

  VulkanDevice(const VulkanDevice &) = delete;
  VulkanDevice &operator=(const VulkanDevice &) = delete;
  VulkanDevice(VulkanDevice &&) = delete;
  VulkanDevice &operator=(VulkanDevice &&) = delete;

  bool init(const EngineConfig &config, void *nativeWindowHandle) override;
  void shutdown() override;

  VkInstance getInstance() const override { return instance_; }
  VkPhysicalDevice getPhysicalDevice() const override {
    return physicalDevice_;
  }
  VkDevice getDevice() const override { return device_; }
  VkQueue getGraphicsQueue() const override { return graphicsQueue_; }
  VkQueue getPresentQueue() const override { return presentQueue_; }
  uint32_t getGraphicsQueueIndex() const override {
    return graphicsQueueIndex_;
  }
  uint32_t getPresentQueueIndex() const override { return presentQueueIndex_; }
  bool hasValidation() const override { return enableValidation_; }

private:
  static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
      VkDebugUtilsMessageSeverityFlagBitsEXT severity,
      VkDebugUtilsMessageTypeFlagsEXT type,
      const VkDebugUtilsMessengerCallbackDataEXT *data, void *userData);

  bool checkValidationSupport();
  VkResult createDebugUtilsMessengerEXT(
      VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *createInfo,
      const VkAllocationCallbacks *allocator,
      VkDebugUtilsMessengerEXT *messenger);
  void destroyDebugUtilsMessengerEXT(VkInstance instance,
                                     VkDebugUtilsMessengerEXT messenger,
                                     const VkAllocationCallbacks *allocator);

  QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device,
                                       VkSurfaceKHR surface);
  bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface);
  bool checkDeviceExtensionSupport(VkPhysicalDevice device);
  SwapChainSupport querySwapChainSupport(VkPhysicalDevice device,
                                         VkSurfaceKHR surface);

  VkInstance instance_ = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
  VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  VkSurfaceKHR surface_ = VK_NULL_HANDLE;
  VkQueue graphicsQueue_ = VK_NULL_HANDLE;
  VkQueue presentQueue_ = VK_NULL_HANDLE;
  uint32_t graphicsQueueIndex_ = 0;
  uint32_t presentQueueIndex_ = 0;

  bool enableValidation_ = false;

  const std::vector<const char *> validationLayers_ = {
      "VK_LAYER_KHRONOS_validation"};

  const std::vector<const char *> deviceExtensions_ = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME};
};

} // namespace Raiden::Core
