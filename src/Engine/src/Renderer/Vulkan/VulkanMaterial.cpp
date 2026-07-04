
#include <Raiden/Logger.hpp>
#include <Renderer/Vulkan/VulkanCommandBuffer.hpp>
#include <Renderer/Vulkan/VulkanMaterial.hpp>
#include <Renderer/Vulkan/VulkanPipelineImpl.hpp>
#include <Renderer/Vulkan/VulkanTextureImpl.hpp>

#include <array>

namespace Raiden::Renderer {

static const ::Raiden::Core::Logger s_logger("Raiden::Renderer::VulkanMaterial");

// material params UBO
struct MaterialParams {
  glm::vec4 baseColorFactor;
  glm::vec4 emissiveFactor; // w unused
  float metallicFactor;
  float roughnessFactor;
  float occlusionStrength;
  float alphaCutoff;
  int alphaMode;        // 0= Opaque, 1= Mask, 2= Blend
  int normalMapDirectX; // 0= OpenGL, 1= DirectX convention
  int hasNormalMap;     // shader needs to know if slot is bound
  int hasEmissiveMap;
  int hasOcclusionMap;
  int hasMetallicRoughnessMap;
  glm::vec2 uvOffset;
  glm::vec2 uvScale;
  float uvRotation;
  int vertexColorMode; // 0=Ignore, 1=Multiply, 2=Add
  std::array<float, 2> _pad; // keep 16-byte alignment
};

bool VulkanMaterial::init(VkDevice device, VmaAllocator allocator,
                          VulkanDescriptorPool &pool, IPipeline *pipeline,
                          const MaterialDesc &desc,
                          std::shared_ptr<ITexture> albedo,
                          std::shared_ptr<ITexture> normal,
                          std::shared_ptr<ITexture> metallicRoughness,
                          std::shared_ptr<ITexture> emissive,
                          std::shared_ptr<ITexture> occlusion) {
  device_ = device;
  pool_ = &pool;
  pipeline_ = pipeline;

  // keep textures alive as long as this material is alive
  albedo_ = std::move(albedo);
  normal_ = std::move(normal);
  metallicRoughness_ = std::move(metallicRoughness);
  emissive_ = std::move(emissive);
  occlusion_ = std::move(occlusion);

  // allocate material params UBO
  bool ok = paramsUbo_.init(
      allocator, sizeof(MaterialParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
      VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

  if (!ok) {
    s_logger.error("Failed to create material params UBO");
    return false;
  }
  paramsUbo_.map();
  uploadParams(desc);

  // allocate descriptor set for texture slots
  VkDescriptorSetAllocateInfo allocInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = pool.handle(),
      .descriptorSetCount = 1,
      .pSetLayouts = pool.materialSetLayoutAddr(),
  };

  if (vkAllocateDescriptorSets(device_, &allocInfo, &materialSet_) !=
      VK_SUCCESS) {
    s_logger.error("Failed to allocate material descriptor set");
    return false;
  }

  // allocate descriptor set for params UBO
  VkDescriptorSetAllocateInfo paramsAllocInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = pool.handle(),
      .descriptorSetCount = 1,
      .pSetLayouts = pool.materialParamsSetLayoutAddr(),
  };

  if (vkAllocateDescriptorSets(device_, &paramsAllocInfo, &paramsSet_) !=
      VK_SUCCESS) {
    s_logger.error("Failed to allocate material params descriptor set");
    return false;
  }

  writeDescriptors(pool);

  s_logger.info("Material initialized.");
  return true;
}

void VulkanMaterial::uploadParams(const MaterialDesc &desc) {
  MaterialParams params{
      .baseColorFactor = desc.baseColorFactor,
      .emissiveFactor = glm::vec4(desc.emissiveFactor, 0.0F),
      .metallicFactor = desc.metallicFactor,
      .roughnessFactor = desc.roughnessFactor,
      .occlusionStrength = desc.occlusionStrength,
      .alphaCutoff = desc.alphaCutoff,
      .alphaMode = static_cast<int>(desc.alphaMode),
      .normalMapDirectX = desc.normalMapDirectX ? 1 : 0,
      .hasNormalMap = normal_ ? 1 : 0,
      .hasEmissiveMap = emissive_ ? 1 : 0,
      .hasOcclusionMap = occlusion_ ? 1 : 0,
      .hasMetallicRoughnessMap = metallicRoughness_ ? 1 : 0,
      .uvOffset = desc.uvOffset,
      .uvScale = desc.uvScale,
      .uvRotation = desc.uvRotation,
      .vertexColorMode = static_cast<int>(desc.vertexColorMode),
  };

  paramsUbo_.upload(&params, sizeof(params));
}

void VulkanMaterial::writeDescriptors(VulkanDescriptorPool &pool) {
  // helper to get the VkImageView + VkSampler from an ITexture
  // falls back to a 1x1 white texture if the slot is null
  auto getImageInfo =
      [&](const std::shared_ptr<ITexture> &tex) -> VkDescriptorImageInfo {
    if (tex) {
      auto *vkTex = static_cast<VulkanTextureImpl *>(tex.get());
      return {
          .sampler = vkTex->sampler(),
          .imageView = vkTex->view(),
          .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      };
    }

    // fallback, white 1x1 texture from pool
    return pool.fallbackImageInfo();
  };

  VkDescriptorImageInfo albedoInfo = getImageInfo(albedo_);
  VkDescriptorImageInfo normalInfo = getImageInfo(normal_);
  VkDescriptorImageInfo metallicRoughnessInfo =
      getImageInfo(metallicRoughness_);
  VkDescriptorImageInfo emissiveInfo = getImageInfo(emissive_);
  VkDescriptorImageInfo occlusionInfo = getImageInfo(occlusion_);

  // texture descriptor writes, binding slots match the shader layout
  std::array<VkWriteDescriptorSet, 5> writes{{
      {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = materialSet_,
          .dstBinding = 0, // albedo
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
          .pImageInfo = &albedoInfo,
      },
      {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = materialSet_,
          .dstBinding = 1, // normal
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
          .pImageInfo = &normalInfo,
      },
      {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = materialSet_,
          .dstBinding = 2, // metallicRoughness
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
          .pImageInfo = &metallicRoughnessInfo,
      },
      {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = materialSet_,
          .dstBinding = 3, // emissive
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
          .pImageInfo = &emissiveInfo,
      },
      {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = materialSet_,
          .dstBinding = 4, // occlusion
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
          .pImageInfo = &occlusionInfo,
      },
  }};

  vkUpdateDescriptorSets(device_, static_cast<uint32_t>(writes.size()),
                         writes.data(), 0, nullptr);

  // params UBO write
  VkDescriptorBufferInfo bufferInfo{
      .buffer = paramsUbo_.buffer(),
      .offset = 0,
      .range = sizeof(MaterialParams),
  };

  VkWriteDescriptorSet paramsWrite{
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = paramsSet_,
      .dstBinding = 0,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .pBufferInfo = &bufferInfo,
  };

  vkUpdateDescriptorSets(device_, 1, &paramsWrite, 0, nullptr);
}

void VulkanMaterial::bind(ICommandBuffer &cmd) {
  auto *vkCmd = static_cast<VulkanCommandBuffer *>(&cmd);
  VkCommandBuffer raw = vkCmd->handle();

  VulkanPipelineImpl *vkPipeline = nullptr;

  if (pipeline_ != nullptr) {
    vkPipeline = static_cast<VulkanPipelineImpl *>(pipeline_);
    vkCmdBindPipeline(raw, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      vkPipeline->handle());
  }

  if (vkPipeline != nullptr) {
    vkCmd->setPipelineLayout(vkPipeline->layout());

    VkDescriptorSet samplerSet = pool_->samplerSet();
    std::array<VkDescriptorSet, 3> sets = {samplerSet, materialSet_, paramsSet_};
    vkCmdBindDescriptorSets(raw, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            vkPipeline->layout(),
                            1, static_cast<uint32_t>(sets.size()), sets.data(),
                            0, nullptr);
  }
}

void VulkanMaterial::shutdown() {
  paramsUbo_.shutdown();
  materialSet_ = VK_NULL_HANDLE;
  paramsSet_ = VK_NULL_HANDLE;
  pipeline_ = nullptr;
  device_ = VK_NULL_HANDLE;
}

VulkanMaterial::~VulkanMaterial() { shutdown(); }

} // namespace Raiden::Renderer
