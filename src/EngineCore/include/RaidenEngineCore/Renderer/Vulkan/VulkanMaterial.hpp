#pragma once

#include <RaidenEngineCore/Renderer/ICommandBuffer.hpp>
#include <RaidenEngineCore/Renderer/IMaterial.hpp>
#include <RaidenEngineCore/Renderer/ITexture.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanBuffer.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanDescriptorPool.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanAllocator.hpp>

#include <memory>
#include <volk.h>

namespace Raiden::Core {

class IPipeline;

class VulkanMaterial final : public IMaterial {
public:
  VulkanMaterial() = default;
  ~VulkanMaterial() override;

  VulkanMaterial(const VulkanMaterial &) = delete;
  VulkanMaterial &operator=(const VulkanMaterial &) = delete;

  bool init(VkDevice device, VmaAllocator allocator, VulkanDescriptorPool &pool,
            IPipeline *pipeline, const MaterialDesc &desc,
            std::shared_ptr<ITexture> albedo,
            std::shared_ptr<ITexture> normal = nullptr,
            std::shared_ptr<ITexture> metallicRoughness = nullptr,
            std::shared_ptr<ITexture> emissive = nullptr,
            std::shared_ptr<ITexture> occlusion = nullptr);

  void bind(ICommandBuffer &cmd) override;
  void shutdown();

private:
  void uploadParams(const MaterialDesc &desc);
  void writeDescriptors(VulkanDescriptorPool &pool);

  VkDevice device_ = VK_NULL_HANDLE;
  VulkanDescriptorPool *pool_ = nullptr;
  VkDescriptorSet materialSet_ = VK_NULL_HANDLE;
  VkDescriptorSet paramsSet_ = VK_NULL_HANDLE;
  VulkanBuffer paramsUbo_;
  IPipeline *pipeline_ = nullptr; // non-owning

  std::shared_ptr<ITexture> albedo_;
  std::shared_ptr<ITexture> normal_;
  std::shared_ptr<ITexture> metallicRoughness_;
  std::shared_ptr<ITexture> emissive_;
  std::shared_ptr<ITexture> occlusion_;
};

} // namespace Raiden::Core