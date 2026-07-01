#pragma once

#include <RaidenEngineCore/ECS/World.hpp>
#include <RaidenEngineCore/EngineConfig.hpp>
#include <RaidenEngineCore/Renderer/RenderTypes.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanAllocator.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanBuffer.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanCommandBuffer.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanDescriptorPool.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanFrameContext.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanImage.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanPipeline.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanPipelineImpl.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanRenderPass.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanShader.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanSwapchain.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanTextureImpl.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <RaidenEngineCore/Renderer/Vulkan/IVulkanRenderDevice.hpp>

#include <glm/glm.hpp>

#include <chrono>
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

class VulkanDevice : public IVulkanRenderDevice {
public:
  VulkanDevice() = default;
  ~VulkanDevice() override;

  VulkanDevice(const VulkanDevice &) = delete;
  VulkanDevice &operator=(const VulkanDevice &) = delete;
  VulkanDevice(VulkanDevice &&) = delete;
  VulkanDevice &operator=(VulkanDevice &&) = delete;

  bool init(const EngineConfig &config, IPlatform *platform) override;
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

  VkRenderPass getRenderPass() const override {
    return renderPass_.renderPass();
  }

  uint32_t getSwapchainImageCount() const override {
    return static_cast<uint32_t>(swapchain_.imageViews().size());
  }

  VkSampleCountFlagBits getSampleCount() const override {
    return sampleCount_;
  }

  void setWorld(World *world) override { world_ = world; }

  float gpuTimeMs() const { return gpuTimeMs_; }
  uint32_t lastDrawCalls() const { return lastDrawCalls_; }
  uint32_t lastTriangles() const { return lastTriangles_; }

  std::unique_ptr<IBuffer> createBuffer(const BufferDesc &desc) override;
  std::unique_ptr<IPipeline> createPipeline(const PipelineDesc &desc) override;
  std::unique_ptr<ITexture> createTexture(const TextureDesc &desc) override;

  std::shared_ptr<IMaterial>
  createMaterial(const MaterialDesc &desc, std::shared_ptr<ITexture> albedo,
                 std::shared_ptr<ITexture> normal,
                 std::shared_ptr<ITexture> metallicRoughness,
                 std::shared_ptr<ITexture> emissive,
                 std::shared_ptr<ITexture> occlusion) override;

  void waitIdle() override {
    if (device_ != VK_NULL_HANDLE)
      vkDeviceWaitIdle(device_);
  }

  bool drawFrame(const RenderCallback &callback) override;

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
  IPlatform *platform_ = nullptr;
  EngineConfig config_;

  std::vector<std::unique_ptr<IPipeline>> pipelineOwnership_;

  bool enableValidation_ = false;

  const std::vector<const char *> validationLayers_ = {
      "VK_LAYER_KHRONOS_validation"};

  const std::vector<const char *> deviceExtensions_ = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  bool recreateSwapchain();
  bool createFramebuffers();
  void destroyFramebuffers();
  VkFormat chooseDepthFormat();

  VulkanAllocator allocator_;

  // other stuff
  VulkanSwapchain swapchain_;
  VulkanRenderPass renderPass_;
  std::vector<VkFramebuffer> framebuffers_;
  VulkanFrameContext frameContext_;

  VulkanDescriptorPool descriptorPool_;

  static constexpr uint32_t kMaxFrames = 3;
  struct PerFrame {
    VulkanBuffer uniformBuffer;
    VkDescriptorSet uniformSet = VK_NULL_HANDLE;
  };
  PerFrame perFrame_[kMaxFrames];

  VkCommandPool transferPool_ = VK_NULL_HANDLE;

  VulkanImage depthImage_;
  VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;
  VkSampleCountFlagBits sampleCount_ = VK_SAMPLE_COUNT_1_BIT;
  std::vector<VulkanImage> msaaColorImages_;

  float totalTime_ = 0.0f;

  uint32_t frameIndex_ = 0;
  std::chrono::steady_clock::time_point lastFrameTime_;
  bool timestampReady_[kMaxFrames] = {};
  World *world_ = nullptr;

  // profiler
  VkQueryPool queryPool_ = VK_NULL_HANDLE;
  float timestampPeriod_ = 1.0f;
  float gpuTimeMs_ = 0.0f;
  uint32_t lastDrawCalls_ = 0;
  uint32_t lastTriangles_ = 0;
};

} // namespace Raiden::Core
