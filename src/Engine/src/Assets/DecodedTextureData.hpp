#pragma once

#include <Raiden/Renderer/RenderTypes.hpp>
#include <cstddef>
#include <vector>

namespace Raiden::Assets {

struct DecodedTextureData {
  std::vector<std::byte> pixels;
  uint32_t width = 0;
  uint32_t height = 0;
  ::Raiden::Renderer::Format format = ::Raiden::Renderer::Format::R8G8B8A8_SRGB;
  ::Raiden::Renderer::TextureType type = ::Raiden::Renderer::TextureType::Texture2D;
};

} // namespace Raiden::Assets
