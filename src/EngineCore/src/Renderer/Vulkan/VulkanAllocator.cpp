#include <RaidenEngineCore/Renderer/Vulkan/VulkanAllocator.hpp>
#include <RaidenEngineCore/Logger.hpp>

namespace Raiden::Core {

static const Logger s_logger("Raiden::Core::VulkanAllocator");

bool VulkanAllocator::init(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device) {
  VmaVulkanFunctions functions{};
  functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
  functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

  VmaAllocatorCreateInfo allocatorCreateInfo{
    .flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT,
    .physicalDevice = physicalDevice,
    .device = device,
    .pVulkanFunctions = &functions,
    .instance = instance,
    .vulkanApiVersion = VK_API_VERSION_1_4,
  };

  allocator_ = VmaAllocator{};
  if (vmaCreateAllocator(&allocatorCreateInfo, &allocator_) != VK_SUCCESS) {
    s_logger.critical("Failed to create VMA allocator");
    return false;
  }

  return true;
};

}
