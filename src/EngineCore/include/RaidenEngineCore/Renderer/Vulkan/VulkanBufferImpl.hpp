#pragma once

#include <RaidenEngineCore/Renderer/IBuffer.hpp>
#include <RaidenEngineCore/Renderer/RenderTypes.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanBuffer.hpp>

namespace Raiden::Core {

class VulkanBufferImpl : public IBuffer {
public:
  bool init(VmaAllocator allocator, const BufferDesc &desc) {
    size_ = desc.size;

    VkBufferUsageFlags vkUsage = 0;
    switch (desc.usage) {
    case BufferUsage::Vertex:
      vkUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
      break;
    case BufferUsage::Index:
      vkUsage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
      break;
    case BufferUsage::Uniform:
      vkUsage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
      break;
    }

    VmaMemoryUsage memUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    VmaAllocationCreateFlags allocFlags = 0;
    if (desc.access == MemoryAccess::CpuToGpu) {
      memUsage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
      allocFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    return buffer_.init(allocator, desc.size, vkUsage, memUsage, allocFlags);
  }

  void upload(const void *data, size_t size) override {
    if (buffer_.map()) {
      buffer_.upload(data, size);
      buffer_.unmap();
    }
  }

  size_t size() const override { return size_; }

  VkBuffer handle() const { return buffer_.buffer(); }

private:
  VulkanBuffer buffer_;
  size_t size_ = 0;
};

} // namespace Raiden::Core