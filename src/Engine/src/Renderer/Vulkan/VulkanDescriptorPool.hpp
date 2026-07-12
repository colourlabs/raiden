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

  // set 1b, sampler + shadow map (used by PBR material path)
  [[nodiscard]] VkDescriptorSetLayout samplerWithShadowSetLayout() const { return samplerWithShadowSetLayout_; }
  [[nodiscard]] VkDescriptorSet samplerWithShadowSet() const { return samplerWithShadowSet_; }
  void updateShadowMapDescriptor(VkImageView shadowMapView);

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

  // set 4, IBL resources (irradiance cubemap, pre-filtered cubemap, BRDF LUT)
  [[nodiscard]] VkDescriptorSetLayout iblSetLayout() const { return iblSetLayout_; }
  [[nodiscard]] VkDescriptorSet iblSet() const { return iblSet_; }
  void updateIBLDescriptors(VkImageView irradianceView, VkImageView prefilterView,
                            VkImageView brdfView, VkSampler sampler);

  [[nodiscard]] VkSampler sampler() const { return sampler_; }
  [[nodiscard]] VkSampler clampSampler() const { return clampSampler_; }
  [[nodiscard]] VkSampler shadowSampler() const { return shadowSampler_; }
  [[nodiscard]] VkDescriptorSet clampSamplerSet() const { return clampSamplerSet_; }
  [[nodiscard]] VkDevice device() const { return device_; }

  VkDescriptorSet allocSamplerSet();
  [[nodiscard]] VkDescriptorImageInfo fallbackImageInfo() const;
  [[nodiscard]] VkDescriptorImageInfo fallbackCubeImageInfo() const;

private:
  bool createFallbackTexture();
  bool createFallbackCubeTexture();

  VkDevice device_ = VK_NULL_HANDLE;
  VkPhysicalDevice physDev_ = VK_NULL_HANDLE;
  VkCommandPool transferPool_ = VK_NULL_HANDLE;
  VkQueue graphicsQueue_ = VK_NULL_HANDLE;
  VkDescriptorPool pool_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout uboSetLayout_ = VK_NULL_HANDLE;
  VmaAllocator allocator_ = VK_NULL_HANDLE;

  VkDescriptorSetLayout samplerSetLayout_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout samplerWithShadowSetLayout_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout textureSetLayout_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout materialSetLayout_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout materialParamsSetLayout_ = VK_NULL_HANDLE;
  VkSampler sampler_ = VK_NULL_HANDLE;
  VkDescriptorSet samplerSet_ = VK_NULL_HANDLE;
  VkDescriptorSet samplerWithShadowSet_ = VK_NULL_HANDLE;

  VkSampler clampSampler_ = VK_NULL_HANDLE;
  VkDescriptorSet clampSamplerSet_ = VK_NULL_HANDLE;

  // shadow map sampler (comparison for PCF)
  VkSampler shadowSampler_ = VK_NULL_HANDLE;

  // IBL (set 4: irradiance, prefiltered, BRDF LUT)
  VkDescriptorSetLayout iblSetLayout_ = VK_NULL_HANDLE;
  VkDescriptorSet iblSet_ = VK_NULL_HANDLE;

  // 1x1 white fallback for unbound material texture slots
  VkImage fallbackImage_ = VK_NULL_HANDLE;
  VkImageView fallbackImageView_ = VK_NULL_HANDLE;
  VmaAllocation fallbackAllocation_ = VK_NULL_HANDLE;

  // 1x1 white cubemap fallback for unbound IBL cubemap slots
  VkImage fallbackCubeImage_ = VK_NULL_HANDLE;
  VkImageView fallbackCubeImageView_ = VK_NULL_HANDLE;
  VmaAllocation fallbackCubeAllocation_ = VK_NULL_HANDLE;
};

} // namespace Raiden::Renderer