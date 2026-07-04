#include <Raiden/Engine/VulkanImGuiBackend.hpp>
#include <Raiden/Logger.hpp>
#include <Renderer/Vulkan/VulkanCommandBuffer.hpp>

#include <imgui_impl_vulkan.h>

namespace Raiden::Engine {

using namespace ::Raiden::Renderer;

static const ::Raiden::Core::Logger
    s_logger("Raiden::Engine::VulkanImGuiBackend");

VulkanImGuiBackend::VulkanImGuiBackend(IVulkanRenderDevice &device,
                                       VkRenderPass renderPass,
                                       uint32_t imageCount)
    : device_(device), renderPass_(renderPass), imageCount_(imageCount),
      vkDevice_(device.getDevice()) {}

VulkanImGuiBackend::~VulkanImGuiBackend() { shutdown(); }

bool VulkanImGuiBackend::init() {
  ImGui_ImplVulkan_InitInfo initInfo{};
  initInfo.Instance = device_.getInstance();
  initInfo.PhysicalDevice = device_.getPhysicalDevice();
  initInfo.Device = vkDevice_;
  initInfo.QueueFamily = device_.getGraphicsQueueIndex();
  initInfo.Queue = device_.getGraphicsQueue();
  initInfo.DescriptorPool = VK_NULL_HANDLE;
  initInfo.DescriptorPoolSize = 2;
  initInfo.RenderPass = renderPass_;
  initInfo.Subpass = 0;
  initInfo.MinImageCount = imageCount_;
  initInfo.ImageCount = imageCount_;
  initInfo.MSAASamples = device_.getSampleCount();
  initInfo.MinAllocationSize = 1024ULL * 1024;

  if (!ImGui_ImplVulkan_Init(&initInfo)) {
    s_logger.error("Failed to init ImGui Vulkan backend");
    return false;
  }

  if (!ImGui_ImplVulkan_CreateFontsTexture()) {
    s_logger.error("Failed to create ImGui font texture");
    return false;
  }

  shutdown_ = false;
  return true;
}

void VulkanImGuiBackend::newFrame() { ImGui_ImplVulkan_NewFrame(); }

void VulkanImGuiBackend::renderDrawData(ICommandBuffer &cmd) {
  auto &vkCmdBuf = dynamic_cast<VulkanCommandBuffer &>(cmd);
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vkCmdBuf.handle());
}

void VulkanImGuiBackend::shutdown() {
  if (shutdown_) {
    return;
  }

  shutdown_ = true;
  ImGui_ImplVulkan_Shutdown();
}

void VulkanImGuiBackend::waitIdle() {
  if (vkDevice_ != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(vkDevice_);
  }
}

} // namespace Raiden::Engine
