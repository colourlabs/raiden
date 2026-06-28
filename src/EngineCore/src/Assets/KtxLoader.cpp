#include "KtxLoader.hpp"

#include <RaidenEngineCore/Logger.hpp>
#include <RaidenEngineCore/Renderer/IRenderDevice.hpp>
#include <RaidenEngineCore/Renderer/RenderTypes.hpp>

#include <ktx.h>
#include <vk_mem_alloc.h>

namespace Raiden::Core {

static const Logger s_logger("Raiden::Core::KtxLoader");

// map KTX VkFormat back into 
static Format vkFormatToFormat(VkFormat fmt) {
  switch (fmt) {
  case VK_FORMAT_R8G8B8A8_UNORM:
    return Format::R8G8B8A8_UNORM;
  case VK_FORMAT_R8G8B8A8_SRGB:
    return Format::R8G8B8A8_SRGB;
  case VK_FORMAT_BC7_SRGB_BLOCK:
    return Format::R8G8B8A8_SRGB; // compressed, treat as srgb
  case VK_FORMAT_BC7_UNORM_BLOCK:
    return Format::R8G8B8A8_UNORM;
  default:
    return Format::R8G8B8A8_UNORM;
  }
}

std::shared_ptr<ITexture> loadKtx2(IRenderDevice &device, const std::byte *data,
                                   size_t size) {
  // create KTX texture from memory
  ktxTexture2 *ktxTex = nullptr;
  KTX_error_code result = ktxTexture2_CreateFromMemory(
      reinterpret_cast<const ktx_uint8_t *>(data), size,
      KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTex);

  if (result != KTX_SUCCESS) {
    s_logger.error("Failed to create KTX2 texture from memory: {}",
                   ktxErrorString(result));
    return nullptr;
  }

  // transcode Basis Universal compressed data if needed
  // BC7 on desktop, ASTC on mobile, BC7 is the safe desktop default
  if (ktxTexture2_NeedsTranscoding(ktxTex)) {
    // pick best format based on what's available
    ktx_transcode_fmt_e targetFormat = KTX_TTF_BC7_RGBA;

    result = ktxTexture2_TranscodeBasis(ktxTex, targetFormat, 0);
    if (result != KTX_SUCCESS) {
      s_logger.warn("BC7 transcode failed ({}), falling back to RGBA32",
                    ktxErrorString(result));

      // fall back to uncompressed RGBA, always supported
      result = ktxTexture2_TranscodeBasis(ktxTex, KTX_TTF_RGBA32, 0);
      if (result != KTX_SUCCESS) {
        s_logger.error("RGBA32 transcode failed: {}", ktxErrorString(result));
        ktxTexture_Destroy(ktxTexture(ktxTex));
        return nullptr;
      }
    }
  }

  uint32_t width = ktxTex->baseWidth;
  uint32_t height = ktxTex->baseHeight;
  VkFormat vkFmt = static_cast<VkFormat>(ktxTex->vkFormat);
  Format fmt = vkFormatToFormat(vkFmt);

  s_logger.info("KTX2: {}x{} vkFormat={} mips={}", width, height,
                static_cast<int>(vkFmt), ktxTex->numLevels);

  // get the base level pixel data
  ktx_size_t offset = 0;
  result = ktxTexture_GetImageOffset(ktxTexture(ktxTex), 0, 0, 0, &offset);
  if (result != KTX_SUCCESS) {
    s_logger.error("Failed to get KTX2 image offset: {}",
                   ktxErrorString(result));
    ktxTexture_Destroy(ktxTexture(ktxTex));
    return nullptr;
  }

  const ktx_uint8_t *pixels = ktxTexture_GetData(ktxTexture(ktxTex)) + offset;
  ktx_size_t pixelSize = ktxTexture_GetDataSize(ktxTexture(ktxTex));

  // create texture through the device abstraction
  TextureDesc desc{
      .width = width,
      .height = height,
      .format = fmt,
  };

  auto tex = device.createTexture(desc);
  if (!tex) {
    s_logger.error("Failed to create texture for KTX2 asset");
    ktxTexture_Destroy(ktxTexture(ktxTex));
    return nullptr;
  }

  tex->upload(pixels, pixelSize);

  ktxTexture_Destroy(ktxTexture(ktxTex));

  s_logger.info("KTX2 texture loaded successfully ({}x{})", width, height);
  return tex;
}

} // namespace Raiden::Core