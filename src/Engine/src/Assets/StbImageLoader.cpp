#include "StbImageLoader.hpp"

#include <Raiden/Logger.hpp>
#include <Raiden/Renderer/RenderTypes.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <cstring>

namespace Raiden::Assets {

using namespace ::Raiden::Core;
using namespace ::Raiden::Renderer;

static const ::Raiden::Core::Logger s_logger("Raiden::Assets::StbImageLoader");

std::optional<DecodedTextureData> decodeStbImage(const std::byte *data,
                                                 size_t size) {
  int w, h, channels;
  stbi_uc *pixels = stbi_load_from_memory(
      reinterpret_cast<const stbi_uc *>(data), static_cast<int>(size), &w, &h,
      &channels, STBI_rgb_alpha);

  if (!pixels) {
    s_logger.error("stb_image failed: {}", stbi_failure_reason());
    return std::nullopt;
  }

  DecodedTextureData decoded;
  decoded.width = w;
  decoded.height = h;
  decoded.format = Format::R8G8B8A8_SRGB;

  size_t pixelSize = static_cast<size_t>(w * h * 4);
  decoded.pixels.resize(pixelSize);
  std::memcpy(decoded.pixels.data(), pixels, pixelSize);

  stbi_image_free(pixels);

  return decoded;
}

} // namespace Raiden::Assets
