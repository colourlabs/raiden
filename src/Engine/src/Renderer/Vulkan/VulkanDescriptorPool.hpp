#pragma once

#include <Renderer/Vulkan/VulkanAllocator.hpp>
#include <volk.h>

namespace Raiden::Renderer {

class VulkanDescriptorPool {
public:
  VulkanDescriptorPool() = default;
  ~VulkanDescriptorPool();

  VulkanDescriptorPool(const VulkanDescriptorPool &) = delete;
  VulkanDescriptorPool &operator=(const VulkanDescriptorPool &) = delete;
  VulkanDescriptorPool(VulkanDescriptorPool &&) = delete;
  VulkanDescriptorPool &operator=(VulkanDescriptorPool &&) = delete;

  bool init(VkDevice device, VkPhysicalDevice physDev, VmaAllocator allocator,
            VkCommandPool transferPool, VkQueue graphicsQueue);
  void shutdown();

  [[nodiscard]] VkDescriptorPool handle() const { return pool_; }

  // set 0, frame UBO
  [[nodiscard]] VkDescriptorSetLayout uboSetLayout() const { return uboSetLayout_; }
  [[nodiscard]] const VkDescriptorSetLayout *uboSetLayoutAddr() const {
    return &uboSetLayout_;
  }

  // set 1, sampler (shared, used by both simple and material paths)
  [[nodiscard]] VkDescriptorSetLayout samplerSetLayout() const { return samplerSetLayout_; }
  [[nodiscard]] VkDescriptorSet samplerSet() const { return samplerSet_; }

  // set 2, simple single texture (VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, binding 0)
  [[nodiscard]] VkDescriptorSetLayout textureSetLayout() const { return textureSetLayout_; }

  // set 2, material textures (albedo, normal, metallicRoughness, emissive,
  // occlusion)
  [[nodiscard]] VkDescriptorSetLayout materialSetLayout() const { return materialSetLayout_; }
  [[nodiscard]] const VkDescriptorSetLayout *materialSetLayoutAddr() const {
    return &materialSetLayout_;
  }

  // set 3, material params UBO
  [[nodiscard]] VkDescriptorSetLayout materialParamsSetLayout() const {
    return materialParamsSetLayout_;
  }
  [[nodiscard]] const VkDescriptorSetLayout *materialParamsSetLayoutAddr() const {
    return &materialParamsSetLayout_;
  }

  [[nodiscard]] VkSampler sampler() const { return sampler_; }
  [[nodiscard]] VkSampler clampSampler() const { return clampSampler_; }
  [[nodiscard]] VkDescriptorSet clampSamplerSet() const { return clampSamplerSet_; }
  [[nodiscard]] VkDevice device() const { return device_; }

  VkDescriptorSet allocSamplerSet();
  [[nodiscard]] VkDescriptorImageInfo fallbackImageInfo() const;

private:
  bool createFallbackTexture();

  VkDevice device_ = VK_NULL_HANDLE;
  VkPhysicalDevice physDev_ = VK_NULL_HANDLE;
  VkCommandPool transferPool_ = VK_NULL_HANDLE;
  VkQueue graphicsQueue_ = VK_NULL_HANDLE;
  VkDescriptorPool pool_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout uboSetLayout_ = VK_NULL_HANDLE;
  VmaAllocator allocator_ = VK_NULL_HANDLE;

  VkDescriptorSetLayout samplerSetLayout_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout textureSetLayout_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout materialSetLayout_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout materialParamsSetLayout_ = VK_NULL_HANDLE;
  VkSampler sampler_ = VK_NULL_HANDLE;
  VkDescriptorSet samplerSet_ = VK_NULL_HANDLE;

  VkSampler clampSampler_ = VK_NULL_HANDLE;
  VkDescriptorSet clampSamplerSet_ = VK_NULL_HANDLE;

  // 1x1 white fallback for unbound material texture slots
  VkImage fallbackImage_ = VK_NULL_HANDLE;
  VkImageView fallbackImageView_ = VK_NULL_HANDLE;
  VmaAllocation fallbackAllocation_ = VK_NULL_HANDLE;
};

} // namespace Raiden::Renderer