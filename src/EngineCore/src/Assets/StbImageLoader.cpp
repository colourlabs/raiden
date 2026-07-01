#include "StbImageLoader.hpp"

#include <RaidenEngineCore/Logger.hpp>
#include <RaidenEngineCore/Renderer/IRenderDevice.hpp>
#include <RaidenEngineCore/Renderer/RenderTypes.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace Raiden::Core {

static const Logger s_logger("Raiden::Core::StbImageLoader");

std::shared_ptr<ITexture> loadStbImage(IRenderDevice &device,
                                       const std::byte *data, size_t size) {
  int w, h, channels;
  stbi_uc *pixels = stbi_load_from_memory(
      reinterpret_cast<const stbi_uc *>(data), static_cast<int>(size), &w, &h,
      &channels, STBI_rgb_alpha);

  if (!pixels) {
    s_logger.error("stb_image failed: {}", stbi_failure_reason());
    return nullptr;
  }

  TextureDesc desc{
      .width = static_cast<uint32_t>(w),
      .height = static_cast<uint32_t>(h),
      .format = Format::R8G8B8A8_SRGB,
  };

  auto tex = device.createTexture(desc);
  if (!tex) {
    s_logger.error("Failed to create texture from raw image ({}x{})", w, h);
    stbi_image_free(pixels);
    return nullptr;
  }

  tex->upload(pixels, static_cast<size_t>(w * h * 4));

  stbi_image_free(pixels);

  s_logger.info("Raw image loaded ({}x{})", w, h);
  return tex;
}

} // namespace Raiden::Core
