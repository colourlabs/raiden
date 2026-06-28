#pragma once

#include <RaidenEngineCore/EngineConfig.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanSwapchain.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanFrameContext.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanRenderPass.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanShader.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanPipeline.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanBuffer.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanImage.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanAllocator.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanPipelineImpl.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanTextureImpl.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanCommandBuffer.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanDescriptorPool.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <RaidenEngineCore/Renderer/Vulkan/IVulkanRenderDevice.hpp>

#include <glm/glm.hpp>

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

struct Vertex {
  glm::vec2 pos;
  glm::vec3 color;
};

struct alignas(16) FrameUniforms {
  glm::mat4 projection;
  glm::mat4 view;
  glm::vec4 extra; // x=time, y=deltaTime, z=resolutionX, w=resolutionY
};

class VulkanDevice final : public IVulkanRenderDevice {
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

  std::unique_ptr<IBuffer> createBuffer(const BufferDesc &desc) override;
  std::unique_ptr<IPipeline> createPipeline(const PipelineDesc &desc) override;
  std::unique_ptr<ITexture> createTexture(const TextureDesc &desc) override;

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
  IPlatform* platform_ = nullptr;
  EngineConfig config_;

  bool enableValidation_ = false;

  const std::vector<const char *> validationLayers_ = {
      "VK_LAYER_KHRONOS_validation"};

  const std::vector<const char *> deviceExtensions_ = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  bool drawFrame() override;
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

  VulkanShader vertexShader_;
  VulkanShader fragmentShader_;
  VulkanPipeline pipeline_;
  VulkanBuffer vertexBuffer_;
  VulkanBuffer indexBuffer_;
  VulkanImage depthImage_;
  VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;

  float totalTime_ = 0.0f;
};

} // namespace Raiden::Core
