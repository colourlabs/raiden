#include <Raiden/Logger.hpp>
#include <Renderer/Vulkan/VulkanSampler.hpp>

namespace Raiden::Renderer {

static const ::Raiden::Core::Logger s_logger("Raiden::Renderer::VulkanSampler");

VulkanSampler::~VulkanSampler() { shutdown(); }

static VkFilter toVkFilter(SamplerFilter filter) {
  switch (filter) {
  case SamplerFilter::Nearest:
    return VK_FILTER_NEAREST;
  case SamplerFilter::Linear:
    return VK_FILTER_LINEAR;
  }
  return VK_FILTER_LINEAR;
}

static VkSamplerMipmapMode toVkMipmapMode(SamplerMipmapMode mode) {
  switch (mode) {
  case SamplerMipmapMode::Nearest:
    return VK_SAMPLER_MIPMAP_MODE_NEAREST;
  case SamplerMipmapMode::Linear:
    return VK_SAMPLER_MIPMAP_MODE_LINEAR;
  }
  return VK_SAMPLER_MIPMAP_MODE_LINEAR;
}

static VkSamplerAddressMode toVkAddressMode(SamplerAddressMode mode) {
  switch (mode) {
  case SamplerAddressMode::Repeat:
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
  case SamplerAddressMode::MirroredRepeat:
    return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
  case SamplerAddressMode::ClampToEdge:
    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  case SamplerAddressMode::ClampToBorder:
    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  }
  return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

static VkCompareOp toVkCompareOp(CompareOp op) {
  switch (op) {
  case CompareOp::Less:
    return VK_COMPARE_OP_LESS;
  case CompareOp::LessOrEqual:
    return VK_COMPARE_OP_LESS_OR_EQUAL;
  }
  return VK_COMPARE_OP_LESS;
}

bool VulkanSampler::init(VkDevice device, const SamplerDesc &desc) {
  device_ = device;

  VkSamplerCreateInfo samplerInfo{
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = toVkFilter(desc.magFilter),
      .minFilter = toVkFilter(desc.minFilter),
      .mipmapMode = toVkMipmapMode(desc.mipmapMode),
      .addressModeU = toVkAddressMode(desc.addressModeU),
      .addressModeV = toVkAddressMode(desc.addressModeV),
      .addressModeW = toVkAddressMode(desc.addressModeW),
      .mipLodBias = desc.mipLodBias,
      .anisotropyEnable = desc.anisotropyEnable ? VK_TRUE : VK_FALSE,
      .maxAnisotropy = desc.maxAnisotropy,
      .compareEnable = VK_FALSE,
      .compareOp = toVkCompareOp(desc.compareOp),
      .minLod = desc.minLod,
      .maxLod = desc.maxLod,
      .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
      .unnormalizedCoordinates = VK_FALSE,
  };

  if (vkCreateSampler(device_, &samplerInfo, nullptr, &sampler_) !=
      VK_SUCCESS) {
    s_logger.error("Failed to create sampler");
    return false;
  }

  return true;
}

void VulkanSampler::shutdown() {
  if (sampler_ != VK_NULL_HANDLE) {
    vkDestroySampler(device_, sampler_, nullptr);
    sampler_ = VK_NULL_HANDLE;
  }
  device_ = VK_NULL_HANDLE;
}

} // namespace Raiden::Renderer
