#include "KtxLoader.hpp"

#include <Raiden/Logger.hpp>
#include <Raiden/Renderer/RenderTypes.hpp>

#include <ktx.h>
#include <volk.h>

#include <cstring>

namespace Raiden::Assets {

using namespace ::Raiden::Core;
using namespace ::Raiden::Renderer;

static const ::Raiden::Core::Logger s_logger("Raiden::Assets::KtxLoader");

static Format vkFormatToFormat(int vkFmt) {
  switch (vkFmt) {
  case VK_FORMAT_R8G8B8A8_UNORM:
    return Format::R8G8B8A8_UNORM;
  case VK_FORMAT_R8G8B8A8_SRGB:
    return Format::R8G8B8A8_SRGB;
  case VK_FORMAT_BC7_SRGB_BLOCK:
    return Format::R8G8B8A8_SRGB;
  case VK_FORMAT_BC7_UNORM_BLOCK:
    return Format::R8G8B8A8_UNORM;
  default:
    return Format::R8G8B8A8_UNORM;
  }
}

std::optional<DecodedTextureData> decodeKtx2(const std::byte *data,
                                             size_t size) {
  ktxTexture2 *ktxTex = nullptr;
  KTX_error_code result = ktxTexture2_CreateFromMemory(
      reinterpret_cast<const ktx_uint8_t *>(data), size,
      KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTex);

  if (result != KTX_SUCCESS) {
    s_logger.error("Failed to create KTX2 texture from memory: {}",
                   ktxErrorString(result));
    return std::nullopt;
  }

  if (ktxTexture2_NeedsTranscoding(ktxTex)) {
    result = ktxTexture2_TranscodeBasis(ktxTex, KTX_TTF_RGBA32, 0);
    if (result != KTX_SUCCESS) {
      s_logger.error("RGBA32 transcode failed: {}", ktxErrorString(result));
      ktxTexture_Destroy(ktxTexture(ktxTex));
      return std::nullopt;
    }
  }

  DecodedTextureData decoded;
  decoded.width = static_cast<int>(ktxTex->baseWidth);
  decoded.height = static_cast<int>(ktxTex->baseHeight);
  decoded.format = vkFormatToFormat(ktxTex->vkFormat);

  uint32_t faceCount = ktxTex->numFaces;
  decoded.type = (faceCount == 6) ? TextureType::TextureCube
                                  : TextureType::Texture2D;

  ktx_size_t faceSize = static_cast<ktx_size_t>(decoded.width) *
                        static_cast<ktx_size_t>(decoded.height) * 4;
  decoded.pixels.resize(faceSize * faceCount);

  for (uint32_t face = 0; face < faceCount; face++) {
    ktx_size_t offset = 0;
    ktxTexture_GetImageOffset(ktxTexture(ktxTex), 0, 0, face, &offset);
    const ktx_uint8_t *src =
        ktxTexture_GetData(ktxTexture(ktxTex)) + offset;
    std::memcpy(decoded.pixels.data() + faceSize * face, src, faceSize);
  }

  ktxTexture_Destroy(ktxTexture(ktxTex));

  return decoded;
}

} // namespace Raiden::Assets
