#pragma once

#include <RaidenEngineCore/Renderer/RenderTypes.hpp>
#include <cstddef>
#include <vector>

namespace Raiden::Core {

struct DecodedTextureData {
  std::vector<std::byte> pixels;
  int width = 0;
  int height = 0;
  Format format = Format::R8G8B8A8_SRGB;
  TextureType type = TextureType::Texture2D;
};

} // namespace Raiden::Core
