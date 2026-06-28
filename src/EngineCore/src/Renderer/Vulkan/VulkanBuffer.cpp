#include <RaidenEngineCore/Logger.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanBuffer.hpp>

#include <cassert>
#include <cstring>

namespace Raiden::Core {

static const Logger s_logger("Raiden::Core::VulkanBuffer");

VulkanBuffer::~VulkanBuffer() { shutdown(); }

bool VulkanBuffer::init(VmaAllocator allocator, VkDeviceSize size,
                        VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage,
                        VmaAllocationCreateFlags allocFlags) {
  allocator_ = allocator;
  size_ = size;

  VkBufferCreateInfo bufferInfo{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = size,
      .usage = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };

  VmaAllocationCreateInfo allocInfo{
      .flags = allocFlags,
      .usage = memoryUsage,
  };

  if (vmaCreateBuffer(allocator_, &bufferInfo, &allocInfo, &buffer_,
                      &allocation_, nullptr) != VK_SUCCESS) {
    s_logger.critical("Failed to create buffer");
    return false;
  }

  return true;
}

void VulkanBuffer::shutdown() {
  if (mapped_) {
    vmaUnmapMemory(allocator_, allocation_);
    mapped_ = nullptr;
  }

  if (buffer_ != VK_NULL_HANDLE) {
    vmaDestroyBuffer(allocator_, buffer_, allocation_);
    buffer_ = VK_NULL_HANDLE;
    allocation_ = nullptr;
  }

  allocator_ = nullptr;
  size_ = 0;
}

bool VulkanBuffer::map() {
  if (mapped_) {
    return true;
  }

  if (vmaMapMemory(allocator_, allocation_, &mapped_) != VK_SUCCESS) {
    s_logger.error("Failed to map buffer");
    return false;
  }

  return true;
}

void VulkanBuffer::unmap() {
  if (!mapped_) {
    return;
  }

  vmaUnmapMemory(allocator_, allocation_);
  mapped_ = nullptr;
}

void VulkanBuffer::upload(const void *data, VkDeviceSize size) {
  assert(mapped_ != nullptr);
  assert(size <= size_);

  std::memcpy(mapped_, data, static_cast<size_t>(size));
}

} // namespace Raiden::Core