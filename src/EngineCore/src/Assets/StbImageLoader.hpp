#pragma once

#include <RaidenEngineCore/Renderer/ITexture.hpp>
#include <cstddef>
#include <memory>

namespace Raiden::Core {

class IRenderDevice;

// loads PNG, JPEG, BMP, GIF, etc. via stb_image
// returns a ready-to-use ITexture or nullptr on failure
std::shared_ptr<ITexture> loadStbImage(IRenderDevice &device,
                                       const std::byte *data, size_t size);

} // namespace Raiden::Core
