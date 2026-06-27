#include <RaidenEngineCore/Renderer/Vulkan/VulkanBuffer.hpp>
#include <RaidenEngineCore/Logger.hpp>

#include <cstring>

namespace Raiden::Core {

static const Logger s_logger("Raiden::Core::VulkanBuffer");

VulkanBuffer::~VulkanBuffer() { shutdown(); }

bool VulkanBuffer::init(VkPhysicalDevice physicalDevice, VkDevice device,
                         VkDeviceSize size, VkBufferUsageFlags usage,
                         VkMemoryPropertyFlags properties) {
  physicalDevice_ = physicalDevice;
  device_ = device;
  size_ = size;

  VkBufferCreateInfo bufferInfo{
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = size,
    .usage = usage,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };

  if (vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer_) != VK_SUCCESS) {
    s_logger.critical("Failed to create buffer");
    return false;
  }

  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(device_, buffer_, &memRequirements);

  VkMemoryAllocateInfo allocInfo{
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = memRequirements.size,
    .memoryTypeIndex =
        findMemoryType(memRequirements.memoryTypeBits, properties),
  };

  if (vkAllocateMemory(device_, &allocInfo, nullptr, &memory_) != VK_SUCCESS) {
    s_logger.critical("Failed to allocate buffer memory");
    return false;
  }

  vkBindBufferMemory(device_, buffer_, memory_, 0);

  if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
    vkMapMemory(device_, memory_, 0, size, 0, &mapped_);
  }

  return true;
}

void VulkanBuffer::shutdown() {
  if (mapped_) {
    vkUnmapMemory(device_, memory_);
    mapped_ = nullptr;
  }
  if (memory_ != VK_NULL_HANDLE) {
    vkFreeMemory(device_, memory_, nullptr);
    memory_ = VK_NULL_HANDLE;
  }
  if (buffer_ != VK_NULL_HANDLE) {
    vkDestroyBuffer(device_, buffer_, nullptr);
    buffer_ = VK_NULL_HANDLE;
  }
}

void VulkanBuffer::upload(const void *data, VkDeviceSize size) {
  std::memcpy(mapped_, data, size);
}

uint32_t VulkanBuffer::findMemoryType(uint32_t typeFilter,
                                       VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProperties);

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) &&
        (memProperties.memoryTypes[i].propertyFlags & properties) ==
            properties) {
      return i;
    }
  }

  s_logger.critical("Failed to find suitable memory type");
  return 0;
}

}
