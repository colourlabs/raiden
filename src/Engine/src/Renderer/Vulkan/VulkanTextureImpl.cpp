#include <Raiden/Logger.hpp>
#include <Renderer/Vulkan/VulkanBuffer.hpp>
#include <Renderer/Vulkan/VulkanTextureImpl.hpp>

#include <cstddef>
#include <cstring>

namespace Raiden::Renderer {

static const ::Raiden::Core::Logger
    s_logger("Raiden::Renderer::VulkanTextureImpl");

VkFormat toVkFormat(Format format) {
  switch (format) {
  case Format::R32G32_Float:
    return VK_FORMAT_R32G32_SFLOAT;
  case Format::R32G32B32_Float:
    return VK_FORMAT_R32G32B32_SFLOAT;
  case Format::R32G32B32A32_Float:
    return VK_FORMAT_R32G32B32A32_SFLOAT;
  case Format::R8G8B8A8_UNORM:
    return VK_FORMAT_R8G8B8A8_UNORM;
  case Format::R8G8B8A8_SRGB:
    return VK_FORMAT_R8G8B8A8_SRGB;
  }
  return VK_FORMAT_UNDEFINED;
}

VulkanTextureImpl::~VulkanTextureImpl() { shutdown(); }

bool VulkanTextureImpl::init(VkDevice device, VmaAllocator allocator,
                             VkQueue queue, VkCommandPool cmdPool,
                             const TextureDesc &desc) {
  device_ = device;
  allocator_ = allocator;
  queue_ = queue;
  cmdPool_ = cmdPool;
  width_ = desc.width;
  height_ = desc.height;
  format_ = desc.format;
  type_ = desc.type;
  vkFormat_ = toVkFormat(desc.format);

  if (vkFormat_ == VK_FORMAT_UNDEFINED) {
    s_logger.error("Unsupported texture format");
    return false;
  }

  bool isCube = (desc.type == TextureType::TextureCube);

  VulkanImageInfo imgInfo{
      .width = width_,
      .height = height_,
      .format = vkFormat_,
      .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      .memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
      .aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
      .arrayLayers = isCube ? 6U : 1U,
      .createFlags = isCube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0U,
      .viewType = isCube ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D,
  };

  if (!image_.init(device_, allocator_, imgInfo)) {
    return false;
  }

  VkSamplerCreateInfo samplerInfo{
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .addressModeU = isCube ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
                             : VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeV = isCube ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
                             : VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeW = isCube ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
                             : VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .anisotropyEnable = VK_TRUE,
      .maxAnisotropy = 16.0F,
  };

  if (vkCreateSampler(device_, &samplerInfo, nullptr, &sampler_) !=
      VK_SUCCESS) {
    s_logger.error("Failed to create texture sampler");
    return false;
  }

  return true;
}

Format VulkanTextureImpl::getFormat() const { return format_; }

void VulkanTextureImpl::shutdown() {
  if (sampler_ != VK_NULL_HANDLE) {
    vkDestroySampler(device_, sampler_, nullptr);
    sampler_ = VK_NULL_HANDLE;
  }
  image_.shutdown();
  device_ = VK_NULL_HANDLE;
  allocator_ = nullptr;
  queue_ = VK_NULL_HANDLE;
  cmdPool_ = VK_NULL_HANDLE;
}

void VulkanTextureImpl::upload(const void *data, size_t size) {
  if ((data == nullptr) || size == 0) {
    return;
  }

  uint32_t layerCount = image_.arrayLayers();
  uint32_t faceSize = width_ * height_ * 4;
  auto expectedSize =
      static_cast<size_t>(faceSize) * static_cast<size_t>(layerCount);

  // staging buffer
  VulkanBuffer staging;
  if (!staging.init(allocator_, expectedSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)) {
    s_logger.error("Failed to create staging buffer for texture upload");
    return;
  }

  if (!staging.map()) {
    s_logger.error("Failed to map staging buffer");
    staging.shutdown();
    return;
  }
  staging.upload(data, expectedSize);
  staging.unmap();

  // temporary command buffer for the transfer
  VkCommandBufferAllocateInfo allocInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = cmdPool_,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };

  VkCommandBuffer cmd = nullptr;
  if (vkAllocateCommandBuffers(device_, &allocInfo, &cmd) != VK_SUCCESS) {
    s_logger.error("Failed to allocate transfer command buffer");
    staging.shutdown();
    return;
  }

  VkCommandBufferBeginInfo beginInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  vkBeginCommandBuffer(cmd, &beginInfo);

  transitionLayout(cmd, VK_IMAGE_LAYOUT_UNDEFINED,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  // one copy region per array layer (face)
  std::vector<VkBufferImageCopy> copyRegions;
  copyRegions.reserve(layerCount);
  for (uint32_t i = 0; i < layerCount; i++) {
    VkBufferImageCopy region{
        .bufferOffset = static_cast<VkDeviceSize>(faceSize) * i,
        .bufferRowLength = 0,
        .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, i, 1},
        .imageExtent = {width_, height_, 1},
    };
    copyRegions.push_back(region);
  }

  vkCmdCopyBufferToImage(cmd, staging.buffer(), image_.image(),
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         static_cast<uint32_t>(copyRegions.size()),
                         copyRegions.data());

  transitionLayout(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  vkEndCommandBuffer(cmd);

  VkFenceCreateInfo fenceInfo{
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
  };

  VkFence fence = nullptr;
  if (vkCreateFence(device_, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
    s_logger.error("Failed to create transfer fence");
    vkFreeCommandBuffers(device_, cmdPool_, 1, &cmd);
    staging.shutdown();
    return;
  }

  VkSubmitInfo submitInfo{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &cmd,
  };

  if (vkQueueSubmit(queue_, 1, &submitInfo, fence) != VK_SUCCESS) {
    s_logger.error("Failed to submit texture transfer");
  }

  vkWaitForFences(device_, 1, &fence, VK_TRUE, UINT64_MAX);
  vkDestroyFence(device_, fence, nullptr);
  vkFreeCommandBuffers(device_, cmdPool_, 1, &cmd);
  staging.shutdown();
}

VkDescriptorSet VulkanTextureImpl::getOrCreateDescriptorSet(
    VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout,
    VkSampler sampler) const {
  if (descriptorSet_ != VK_NULL_HANDLE) {
    return descriptorSet_;
  }

  std::lock_guard<std::mutex> lock(descMutex_);
  if (descriptorSet_ != VK_NULL_HANDLE) {
    return descriptorSet_;
  }

  VkDescriptorSetAllocateInfo allocInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = pool,
      .descriptorSetCount = 1,
      .pSetLayouts = &layout,
  };

  if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet_) !=
      VK_SUCCESS) {
    s_logger.error("Failed to allocate descriptor set for texture");
    return VK_NULL_HANDLE;
  }

  VkDescriptorImageInfo imageInfo{
      .sampler = sampler_,
      .imageView = image_.view(),
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };

  VkWriteDescriptorSet write{
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = descriptorSet_,
      .dstBinding = 0,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
      .pImageInfo = &imageInfo,
  };

  vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

  return descriptorSet_;
}

void VulkanTextureImpl::transitionLayout(VkCommandBuffer cmd,
                                         VkImageLayout oldLayout,
                                         VkImageLayout newLayout) {
  uint32_t layerCount = image_.arrayLayers();
  VkImageMemoryBarrier barrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .oldLayout = oldLayout,
      .newLayout = newLayout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image_.image(),
      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, layerCount},
  };

  VkPipelineStageFlags srcStage = 0;
  VkPipelineStageFlags dstStage = 0;

  if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
      newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  }

  vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1,
                       &barrier);
}

} // namespace Raiden::Renderer
