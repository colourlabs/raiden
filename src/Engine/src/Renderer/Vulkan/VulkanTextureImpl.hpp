#pragma once

#include <Raiden/Renderer/ITexture.hpp>
#include <Raiden/Renderer/RenderTypes.hpp>
#include <Renderer/Vulkan/VulkanImage.hpp>
#include <mutex>
#include <volk.h>

namespace Raiden::Renderer {

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

  uint32_t getWidth() const override { return width_; }
  uint32_t getHeight() const override { return height_; }
  Format getFormat() const override;
  TextureType getType() const override { return type_; }
  uint32_t getMipLevels() const override { return 1; }

  VkImageView view() const { return image_.view(); }
  VkDescriptorSet getOrCreateDescriptorSet(VkDevice device,
                                           VkDescriptorPool pool,
                                           VkDescriptorSetLayout layout,
                                           VkSampler sampler) const;
  VkSampler sampler() const { return sampler_; }

private:
  void transitionLayout(VkCommandBuffer cmd, VkImageLayout oldLayout,
                        VkImageLayout newLayout);

  VkDevice device_ = VK_NULL_HANDLE;
  VmaAllocator allocator_ = nullptr;
  VkQueue queue_ = VK_NULL_HANDLE;
  VkCommandPool cmdPool_ = VK_NULL_HANDLE;
  VulkanImage image_;
  VkFormat vkFormat_ = VK_FORMAT_UNDEFINED;
  Format format_ = Format::R8G8B8A8_UNORM;
  TextureType type_ = TextureType::Texture2D;
  VkSampler sampler_ = VK_NULL_HANDLE;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  mutable std::mutex descMutex_;
  mutable VkDescriptorSet descriptorSet_ = VK_NULL_HANDLE;
};

VkFormat toVkFormat(Format format);

} // namespace Raiden::Renderer
