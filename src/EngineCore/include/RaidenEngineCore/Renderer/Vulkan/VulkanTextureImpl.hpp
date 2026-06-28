#pragma once

#include <RaidenEngineCore/Renderer/ITexture.hpp>
#include <RaidenEngineCore/Renderer/RenderTypes.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanImage.hpp>
#include <volk.h>

namespace Raiden::Core {

class VulkanTextureImpl final : public ITexture {
public:
  VulkanTextureImpl() = default;
  ~VulkanTextureImpl() override;

  VulkanTextureImpl(const VulkanTextureImpl &) = delete;
  VulkanTextureImpl &operator=(const VulkanTextureImpl &) = delete;
  VulkanTextureImpl(VulkanTextureImpl &&) = delete;
  VulkanTextureImpl &operator=(VulkanTextureImpl &&) = delete;

  bool init(VkDevice device, VmaAllocator allocator, VkQueue queue,
            VkCommandPool cmdPool, const TextureDesc &desc);
  void shutdown();

  void upload(const void *data, size_t size) override;

  VkImageView view() const { return image_.view(); }
  VkDescriptorSet getOrCreateDescriptorSet(VkDevice device,
                                           VkDescriptorPool pool,
                                           VkDescriptorSetLayout layout,
                                           VkSampler sampler) const;

private:
  void transitionLayout(VkCommandBuffer cmd, VkImageLayout oldLayout,
                        VkImageLayout newLayout);

  VkDevice device_ = VK_NULL_HANDLE;
  VmaAllocator allocator_ = nullptr;
  VkQueue queue_ = VK_NULL_HANDLE;
  VkCommandPool cmdPool_ = VK_NULL_HANDLE;
  VulkanImage image_;
  VkFormat vkFormat_ = VK_FORMAT_UNDEFINED;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  mutable VkDescriptorSet descriptorSet_ = VK_NULL_HANDLE;
};

VkFormat toVkFormat(Format format);

} // namespace Raiden::Core
